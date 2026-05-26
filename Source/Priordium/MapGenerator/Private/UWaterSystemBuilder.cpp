// Copyright Priordium. All Rights Reserved.
//
// UWaterSystemBuilder.cpp
// Water body generation implementation.

#include "UWaterSystemBuilder.h"
#include "UHeightmapGenerator.h"
#include "UMapGeneratorSettings.h"
#include "GameFramework/Actor.h"
#include "WaterZoneSpawnUtils.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"
#include "WaterBodyComponent.h"
#include "WaterSplineMetadata.h"
#if WITH_EDITOR
#include "Editor.h"
#include "Subsystems/EditorActorSubsystem.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWaterSystem, Log, All);

// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------

UWaterSystemBuilder::UWaterSystemBuilder()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWaterSystemBuilder::BeginPlay()
{
	Super::BeginPlay();
}

// ----------------------------------------------------------------------------
// Private helper
// ----------------------------------------------------------------------------

FVector2D UWaterSystemBuilder::GridToWorld(const FIntPoint& Cell, const FVector2D& MapOrigin, float CellSize)
{
	return FVector2D(
		MapOrigin.X + Cell.X * CellSize,
		MapOrigin.Y + Cell.Y * CellSize);
}

float UWaterSystemBuilder::SampleDirectionalSlope(
	const UHeightmapGenerator* Heightmap,
	int32 X, int32 Y,
	int32 DirX, int32 DirY,
	int32 Radius)
{
	if (!Heightmap) return 0.f;

	const FIntPoint Res = Heightmap->GetResolution();
	const int32 TX = X + DirX * Radius;
	const int32 TY = Y + DirY * Radius;

	if (TX < 0 || TX >= Res.X || TY < 0 || TY >= Res.Y)
		return 0.f;

	// True cell distance: diagonal directions are √2 longer than cardinal ones.
	const float CellDist = (DirX != 0 && DirY != 0)
		? static_cast<float>(Radius) * UE_SQRT_2
		: static_cast<float>(Radius);

	return (Heightmap->GetHeightAt(X, Y) - Heightmap->GetHeightAt(TX, TY)) / CellDist;
}

// ----------------------------------------------------------------------------
// Shared 8-directional lookup tables (cardinal + diagonal).
// Used by SampleDirectionalSlope, FindRiverSources, and TraceRiverPath.
// ----------------------------------------------------------------------------
static const int32  Dir8X[]   = {  0,  1, 1,  1, 0, -1, -1, -1 };
static const int32  Dir8Y[]   = { -1, -1, 0,  1, 1,  1,  0, -1 };
static constexpr int32 NumDir8 = 8;

namespace
{
	void ConfigureWaterLandscapeLayer(UWaterBodyComponent* WaterComp)
	{
		if (!IsValid(WaterComp))
		{
			return;
		}

		if (FBoolProperty* AffectsLandscapeProp = FindFProperty<FBoolProperty>(WaterComp->GetClass(), TEXT("bAffectsLandscape")))
		{
			AffectsLandscapeProp->SetPropertyValue_InContainer(WaterComp, true);
		}

		// Target pre-created Water edit layer (created in landscape build step).
		const FName FixedLayerName(TEXT("Water"));
		const TCHAR* CandidateLayerProps[] =
		{
			TEXT("WaterLayerName"),
			TEXT("LandscapeLayerName"),
			TEXT("AffectsLandscapeLayerName"),
			TEXT("LayerName")
		};

		for (const TCHAR* PropName : CandidateLayerProps)
		{
			if (FNameProperty* NameProp = FindFProperty<FNameProperty>(WaterComp->GetClass(), PropName))
			{
				NameProp->SetPropertyValue_InContainer(WaterComp, FixedLayerName);
			}
		}
	}

	void LinkWaterBodyToZone(UWaterBodyComponent* WaterComp, AActor* ZoneActor)
	{
		if (!IsValid(WaterComp) || !IsValid(ZoneActor))
		{
			return;
		}

		if (FObjectProperty* ZoneOverrideProp = FindFProperty<FObjectProperty>(WaterComp->GetClass(), TEXT("WaterZoneOverride")))
		{
			UClass* RequiredClass = ZoneOverrideProp->PropertyClass;
			if (!RequiredClass || ZoneActor->IsA(RequiredClass))
			{
				ZoneOverrideProp->SetObjectPropertyValue_InContainer(WaterComp, ZoneActor);
			}
		}
	}

	void LinkZoneOnObject(UObject* TargetObject, AActor* ZoneActor)
	{
		if (!IsValid(TargetObject) || !IsValid(ZoneActor))
		{
			return;
		}

		static const TCHAR* CandidateProps[] =
		{
			TEXT("WaterZoneOverride"),
			TEXT("OwningWaterZone"),
			TEXT("WaterZone")
		};

		for (const TCHAR* PropName : CandidateProps)
		{
			if (FObjectProperty* ObjProp = FindFProperty<FObjectProperty>(TargetObject->GetClass(), PropName))
			{
				UClass* RequiredClass = ObjProp->PropertyClass;
				if (!RequiredClass || ZoneActor->IsA(RequiredClass))
				{
					ObjProp->SetObjectPropertyValue_InContainer(TargetObject, ZoneActor);
				}
			}
		}
	}

	/**
	 * Sets lake-specific terrain-carving falloff on a WaterBodyComponent.
	 *
	 * Target path (all via reflection to avoid hard Water plugin header dependency):
	 *   UWaterBodyComponent
	 *     -> WaterHeightmapSettings  (FWaterBodyHeightmapSettings)
	 *         -> FalloffSettings     (FWaterFalloffSettings)
	 *             -> FalloffMode  = EWaterBrushFalloffMode::Angle  (uint8 = 0)
	 *             -> FalloffAngle = 25.0f
	 *             -> EdgeOffset   = 0.0f
	 */
	void ApplyLakeFalloffSettings(UWaterBodyComponent* WaterComp)
	{
		if (!IsValid(WaterComp))
		{
			return;
		}

		// --- Step 1: reach WaterHeightmapSettings on the component ---
		FStructProperty* HeightmapSettingsProp =
			FindFProperty<FStructProperty>(WaterComp->GetClass(), TEXT("WaterHeightmapSettings"));
		if (!HeightmapSettingsProp)
		{
			UE_LOG(LogWaterSystem, Warning,
				TEXT("ApplyLakeFalloffSettings -- WaterHeightmapSettings not found on %s."),
				*WaterComp->GetClass()->GetName());
			return;
		}

		void* HeightmapSettingsPtr =
			HeightmapSettingsProp->ContainerPtrToValuePtr<void>(WaterComp);
		if (!HeightmapSettingsPtr)
		{
			return;
		}

		// --- Step 2: reach FalloffSettings inside the heightmap settings struct ---
		FStructProperty* FalloffSettingsProp =
			FindFProperty<FStructProperty>(HeightmapSettingsProp->Struct, TEXT("FalloffSettings"));
		if (!FalloffSettingsProp)
		{
			UE_LOG(LogWaterSystem, Warning,
				TEXT("ApplyLakeFalloffSettings -- FalloffSettings not found inside FWaterBodyHeightmapSettings."));
			return;
		}

		void* FalloffSettingsPtr =
			FalloffSettingsProp->ContainerPtrToValuePtr<void>(HeightmapSettingsPtr);
		if (!FalloffSettingsPtr)
		{
			return;
		}

		UScriptStruct* FalloffStruct = FalloffSettingsProp->Struct;

		// --- Step 3: FalloffMode = Angle (enum value 0) ---
		// EWaterBrushFalloffMode is a uint8 enum; try FEnumProperty first, then FByteProperty.
		if (FEnumProperty* ModeProp = FindFProperty<FEnumProperty>(FalloffStruct, TEXT("FalloffMode")))
		{
			ModeProp->GetUnderlyingProperty()->SetIntPropertyValue(
				ModeProp->ContainerPtrToValuePtr<void>(FalloffSettingsPtr), 0LL); // 0 = Angle
		}
		else if (FByteProperty* ModeByteProp = FindFProperty<FByteProperty>(FalloffStruct, TEXT("FalloffMode")))
		{
			ModeByteProp->SetPropertyValue_InContainer(FalloffSettingsPtr, static_cast<uint8>(0)); // 0 = Angle
		}

		// --- Step 4: FalloffAngle = 25 ---
		if (FFloatProperty* AngleProp = FindFProperty<FFloatProperty>(FalloffStruct, TEXT("FalloffAngle")))
		{
			AngleProp->SetPropertyValue_InContainer(FalloffSettingsPtr, 25.0f);
		}
		else
		{
			UE_LOG(LogWaterSystem, Warning,
				TEXT("ApplyLakeFalloffSettings -- FalloffAngle not found inside FWaterFalloffSettings."));
		}

		// --- Step 5: EdgeOffset = 0 ---
		if (FFloatProperty* EdgeOffsetProp = FindFProperty<FFloatProperty>(FalloffStruct, TEXT("EdgeOffset")))
		{
			EdgeOffsetProp->SetPropertyValue_InContainer(FalloffSettingsPtr, 0.0f);
		}
		else
		{
			UE_LOG(LogWaterSystem, Warning,
				TEXT("ApplyLakeFalloffSettings -- EdgeOffset not found inside FWaterFalloffSettings."));
		}

		UE_LOG(LogWaterSystem, Display,
			TEXT("ApplyLakeFalloffSettings -- FalloffMode=Angle, FalloffAngle=25, EdgeOffset=0 applied."));
	}

	/**
	 * Sets the blurring radius on a river WaterBodyComponent.
	 *
	 * Target path (via reflection):
	 *   UWaterBodyComponent
	 *     -> WaterHeightmapSettings  (FWaterBodyHeightmapSettings)
	 *         -> Effects             (struct)
	 *             -> Blurring        (struct)
	 *                 -> Radius      (float) = TargetRadius
	 */
	void ApplyRiverBlurringRadius(UWaterBodyComponent* WaterComp, float TargetRadius)
	{
		if (!IsValid(WaterComp))
			return;

		// Step 1 -- reach WaterHeightmapSettings (same as ApplyLakeFalloffSettings).
		FStructProperty* HeightmapSettingsProp =
			FindFProperty<FStructProperty>(WaterComp->GetClass(), TEXT("WaterHeightmapSettings"));
		if (!HeightmapSettingsProp)
		{
			UE_LOG(LogWaterSystem, Warning,
				TEXT("ApplyRiverBlurringRadius -- 'WaterHeightmapSettings' not found on %s."),
				*WaterComp->GetClass()->GetName());
			return;
		}

		void* HeightmapSettingsPtr =
			HeightmapSettingsProp->ContainerPtrToValuePtr<void>(WaterComp);
		if (!HeightmapSettingsPtr)
			return;

		// Step 2 -- reach Effects inside WaterHeightmapSettings.
		FStructProperty* EffectsProp =
			FindFProperty<FStructProperty>(HeightmapSettingsProp->Struct, TEXT("Effects"));
		if (!EffectsProp)
		{
			UE_LOG(LogWaterSystem, Warning,
				TEXT("ApplyRiverBlurringRadius -- 'Effects' not found inside WaterHeightmapSettings."));
			return;
		}

		void* EffectsPtr = EffectsProp->ContainerPtrToValuePtr<void>(HeightmapSettingsPtr);
		if (!EffectsPtr)
			return;

		// Debug: log every property inside the Effects struct so we can find correct names.
		UE_LOG(LogWaterSystem, Warning, TEXT("ApplyRiverBlurringRadius -- Effects struct '%s' properties:"),
			*EffectsProp->Struct->GetName());
		for (TFieldIterator<FProperty> It(EffectsProp->Struct); It; ++It)
		{
			UE_LOG(LogWaterSystem, Warning, TEXT("  Effects.%s  (%s)"),
				*It->GetName(), *It->GetClass()->GetName());
		}

		// Step 3 -- reach Blurring inside Effects.
		FStructProperty* BlurringProp =
			FindFProperty<FStructProperty>(EffectsProp->Struct, TEXT("Blurring"));
		if (!BlurringProp)
		{
			UE_LOG(LogWaterSystem, Warning,
				TEXT("ApplyRiverBlurringRadius -- 'Blurring' not found inside Effects."));
			return;
		}

		void* BlurringPtr = BlurringProp->ContainerPtrToValuePtr<void>(EffectsPtr);
		if (!BlurringPtr)
			return;

		// Debug: log every property inside the Blurring struct so we can find correct names.
		UE_LOG(LogWaterSystem, Warning, TEXT("ApplyRiverBlurringRadius -- Blurring struct '%s' properties:"),
			*BlurringProp->Struct->GetName());
		for (TFieldIterator<FProperty> It(BlurringProp->Struct); It; ++It)
		{
			UE_LOG(LogWaterSystem, Warning, TEXT("  Blurring.%s  (%s)"),
				*It->GetName(), *It->GetClass()->GetName());
		}

		// Step 4 -- set Radius inside Blurring.
		FIntProperty* RadiusProp =
			FindFProperty<FIntProperty>(BlurringProp->Struct, TEXT("Radius"));
		if (!RadiusProp)
		{
			UE_LOG(LogWaterSystem, Warning,
				TEXT("ApplyRiverBlurringRadius -- 'Radius' not found inside Blurring."));
			return;
		}

		RadiusProp->SetPropertyValue_InContainer(BlurringPtr, TargetRadius);

		UE_LOG(LogWaterSystem, Display,
			TEXT("ApplyRiverBlurringRadius -- WaterHeightmapSettings.Effects.Blurring.Radius set to %.1f."),
			TargetRadius);
	}

	/**
	 * Reads the ZoneExtent (full width × height, in cm) that was written to a
	 * WaterZone actor by ApplyZoneExtent(). Returns FVector2D::ZeroVector if the
	 * property is not found.
	 */
	FVector2D ReadZoneExtent(AActor* Zone)
	{
		if (!IsValid(Zone)) return FVector2D::ZeroVector;
		if (FStructProperty* Prop = FindFProperty<FStructProperty>(Zone->GetClass(), TEXT("ZoneExtent")))
		{
			if (FVector2D* Ptr = Prop->ContainerPtrToValuePtr<FVector2D>(Zone))
				return *Ptr;
		}
		return FVector2D::ZeroVector;
	}
}

TArray<FIntPoint> UWaterSystemBuilder::FindRiverSources(
	const UHeightmapGenerator* Heightmap,
	const UMapGeneratorSettings* Settings) const
{
	TArray<FIntPoint> Sources;

	if (!Heightmap || !Heightmap->IsInitialized())
	{
		UE_LOG(LogWaterSystem, Warning,
			TEXT("FindRiverSources -- Heightmap is null or uninitialized."));
		return Sources;
	}

	if (!Settings)
	{
		UE_LOG(LogWaterSystem, Warning,
			TEXT("FindRiverSources -- Settings is null."));
		return Sources;
	}

	if (Settings->MaxRiverCount <= 0)
	{
		UE_LOG(LogWaterSystem, Display,
			TEXT("FindRiverSources -- MaxRiverCount is 0, skipping source search."));
		return Sources;
	}

	const FIntPoint Res = Heightmap->GetResolution();
	const int32 ResX    = Res.X;
	const int32 ResY    = Res.Y;

	// Height normalisation above sea level: stretches [SeaLevel, 1.0] → [0, 1]
	// so elevation differences between high candidates are clearly visible in
	// the score product (NormHeight * MaxGradient).
	const float SeaLevel   = Settings->SeaLevel;
	const float AboveRange = FMath::Max(1.0f - SeaLevel, KINDA_SMALL_NUMBER);

	/**
	 * FCandidateSource
	 * Score = NormHeight * MaxRegionalGradient
	 *   NormHeight          = (H - SeaLevel) / (1 - SeaLevel)
	 *   MaxRegionalGradient = max SampleDirectionalSlope over 8 directions
	 *                         at RiverSlopeRadius cells away
	 */
	struct FCandidateSource { FIntPoint Cell; float Score; float NormHeight; float MaxGradient; };
	TArray<FCandidateSource> Candidates;

	for (int32 Y = 1; Y < ResY - 1; ++Y)
	{
		for (int32 X = 1; X < ResX - 1; ++X)
		{
			const float H = Heightmap->GetHeightAt(X, Y);
			if (H <= SeaLevel) continue;

			// Must be a strict local maximum among immediate neighbours.
			bool bIsLocalMax = true;
			for (int32 dy = -1; dy <= 1 && bIsLocalMax; ++dy)
				for (int32 dx = -1; dx <= 1 && bIsLocalMax; ++dx)
				{
					if (dx == 0 && dy == 0) continue;
					if (Heightmap->GetHeightAt(X + dx, Y + dy) >= H)
						bIsLocalMax = false;
				}
			if (!bIsLocalMax) continue;

			// Steepest downhill slope in any of the 8 directions,
			// sampled RiverSlopeRadius cells away via the shared helper.
			float MaxGrad = 0.f;
			for (int32 d = 0; d < NumDir8; ++d)
			{
				const float Grad = SampleDirectionalSlope(
					Heightmap, X, Y, Dir8X[d], Dir8Y[d], Settings->RiverSlopeRadius);
				if (Grad > MaxGrad) MaxGrad = Grad;
			}

			const float NormH = (H - SeaLevel) / AboveRange;
			Candidates.Add({ FIntPoint(X, Y), NormH * MaxGrad, NormH, MaxGrad });
		}
	}

	// Best combined score first; break ties by normalised elevation.
	Candidates.Sort([](const FCandidateSource& A, const FCandidateSource& B)
	{
		if (!FMath::IsNearlyEqual(A.Score, B.Score, 1e-7f))
			return A.Score > B.Score;
		return A.NormHeight > B.NormHeight;
	});

	const float MinDistSq = MinRiverSourceDistance * MinRiverSourceDistance;
	for (const FCandidateSource& C : Candidates)
	{
		if (Sources.Num() >= Settings->MaxRiverCount) break;

		bool bTooClose = false;
		for (const FIntPoint& E : Sources)
		{
			const float DX = static_cast<float>(C.Cell.X - E.X);
			const float DY = static_cast<float>(C.Cell.Y - E.Y);
			if (DX * DX + DY * DY < MinDistSq) { bTooClose = true; break; }
		}
		if (!bTooClose) Sources.Add(C.Cell);
	}

	if (Sources.Num() > 0)
	{
		const auto& Best = Candidates[0];
		UE_LOG(LogWaterSystem, Display,
			TEXT("FindRiverSources -- found %d sources (candidates: %d, requested: %d, slopeRadius: %d cells). "
			     "Best: cell=(%d,%d) normHeight=%.3f maxRegGrad=%.5f score=%.6f."),
			Sources.Num(), Candidates.Num(), Settings->MaxRiverCount, Settings->RiverSlopeRadius,
			Best.Cell.X, Best.Cell.Y, Best.NormHeight, Best.MaxGradient, Best.Score);
	}
	else
	{
		UE_LOG(LogWaterSystem, Display,
			TEXT("FindRiverSources -- found 0 sources (candidates: %d, requested: %d)."),
			Candidates.Num(), Settings->MaxRiverCount);
	}

	return Sources;
}

TArray<FIntPoint> UWaterSystemBuilder::TraceRiverPath(
	const UHeightmapGenerator* Heightmap,
	const UMapGeneratorSettings* Settings,
	const FIntPoint& Source) const
{
	TArray<FIntPoint> Path;

	if (!Heightmap || !Heightmap->IsInitialized())
	{
		UE_LOG(LogWaterSystem, Warning,
			TEXT("TraceRiverPath -- Heightmap is null or uninitialized."));
		return Path;
	}

	if (!Settings)
	{
		UE_LOG(LogWaterSystem, Warning,
			TEXT("TraceRiverPath -- Settings is null."));
		return Path;
	}

	const FIntPoint Res = Heightmap->GetResolution();
	const int32 ResX    = Res.X;
	const int32 ResY    = Res.Y;
	const int32 SlopeRadius = Settings->RiverSlopeRadius;

	// Deterministic random stream, unique per source position.
	// Used only for the ±MeanderAngle perturbation so rivers look naturally winding
	// while still following the steepest-descent direction as the primary driver.
	const int32 RiverSeed = Settings->Seed
		^ (Source.X * 73856093)
		^ (Source.Y * 19349663);
	FRandomStream Stream(RiverSeed != 0 ? RiverSeed : 42);

	// Maximum angular deviation from the steepest direction (degrees).
	// Configured per Data Asset so it can be tuned in the editor without recompiling.
	const float MeanderAngle = FMath::Max(0.0f, Settings->RiverMeanderAngle);

	Path.Add(Source);
	FIntPoint Current = Source;

	// Upper bound on steps: the effective grid with RiverSlopeRadius stride
	// has at most (ResX/R)*(ResY/R) distinct cells; add headroom for winding.
	const int32 MaxSteps = FMath::Max(
		(ResX / SlopeRadius) * (ResY / SlopeRadius) * 4, 64);

	TSet<FIntPoint> Visited;
	Visited.Add(Source);

	for (int32 Step = 0; Step < MaxSteps; ++Step)
	{
		const float CurrentH = Heightmap->GetHeightAt(Current.X, Current.Y);

		if (CurrentH <= Settings->SeaLevel)
			break;

		// Collect all strictly downhill unvisited neighbours.
		struct FDirCandidate { FIntPoint Cell; int32 Dir; float Grad; };
		TArray<FDirCandidate, TInlineAllocator<8>> Downhill;

		for (int32 d = 0; d < NumDir8; ++d)
		{
			const int32 NX = Current.X + Dir8X[d] * SlopeRadius;
			const int32 NY = Current.Y + Dir8Y[d] * SlopeRadius;

			if (NX < 0 || NX >= ResX || NY < 0 || NY >= ResY) continue;
			if (Visited.Contains(FIntPoint(NX, NY)))            continue;

			const float Grad = SampleDirectionalSlope(
				Heightmap, Current.X, Current.Y, Dir8X[d], Dir8Y[d], SlopeRadius);
			if (Grad > 0.f)
				Downhill.Add({ FIntPoint(NX, NY), d, Grad });
		}

		// No strictly downhill unvisited neighbour → flat basin / local minimum.
		// AppendRiverDefinitions will create a terminus lake here if above sea level.
		if (Downhill.IsEmpty())
			break;

		// Step 1 -- find the steepest downhill direction among the 8 candidates.
		int32 BestIdx = 0;
		for (int32 k = 1; k < Downhill.Num(); ++k)
			if (Downhill[k].Grad > Downhill[BestIdx].Grad) BestIdx = k;

		// Step 2 -- take the steepest direction's exact angle, then add ±MeanderAngle.
		// This is the LAST step: the angle is already determined, we only offset it here.
		const float SteepAngle =
			FMath::Atan2(static_cast<float>(Dir8Y[Downhill[BestIdx].Dir]),
			             static_cast<float>(Dir8X[Downhill[BestIdx].Dir]));
		const float FinalAngle =
			SteepAngle + FMath::DegreesToRadians(Stream.FRandRange(-MeanderAngle, MeanderAngle));

		// Step 3 -- move SlopeRadius cells in the final continuous direction.
		// No snapping back to the 8-direction grid: the landing cell can be anywhere.
		const int32 RawNX = Current.X + FMath::RoundToInt(FMath::Cos(FinalAngle) * static_cast<float>(SlopeRadius));
		const int32 RawNY = Current.Y + FMath::RoundToInt(FMath::Sin(FinalAngle) * static_cast<float>(SlopeRadius));
		const FIntPoint MeanderCell(
			FMath::Clamp(RawNX, 0, ResX - 1),
			FMath::Clamp(RawNY, 0, ResY - 1));

		// Always use the meandered cell.
		// Only fall back to the unperturbed steepest direction if the meandered cell
		// is already on the path (prevents the river from looping back on itself).
		// No strict downhill check here: a ±10° deflection over 40 cells shifts us
		// only ~7 cells sideways; the overall trajectory is still downhill.
		const FIntPoint Chosen =
			(!Visited.Contains(MeanderCell) && MeanderCell != Current)
			? MeanderCell
			: Downhill[BestIdx].Cell;

		Path.Add(Chosen);
		Visited.Add(Chosen);
		Current = Chosen;
	}

	return Path;
}

TArray<FIntPoint> UWaterSystemBuilder::FindLakeLocations(
	const UHeightmapGenerator* Heightmap,
	const UMapGeneratorSettings* Settings) const
{
	TArray<FIntPoint> Lakes;

	if (!Heightmap || !Heightmap->IsInitialized())
	{
		UE_LOG(LogWaterSystem, Warning,
			TEXT("FindLakeLocations -- Heightmap is null or uninitialized."));
		return Lakes;
	}

	if (!Settings)
	{
		UE_LOG(LogWaterSystem, Warning,
			TEXT("FindLakeLocations -- Settings is null."));
		return Lakes;
	}

	if (Settings->MaxLakeCount <= 0)
	{
		UE_LOG(LogWaterSystem, Display,
			TEXT("FindLakeLocations -- MaxLakeCount is 0, skipping lake search."));
		return Lakes;
	}

	const FIntPoint Res = Heightmap->GetResolution();
	const int32 ResX    = Res.X;
	const int32 ResY    = Res.Y;

	// Representative lake radius used for perimeter sampling.
	// Using the midpoint of [Min, Max] gives a reasonable "typical lake" footprint
	// without committing to the final randomised radius at this stage.
	const int32 RadiusMinCells  = FMath::Max(2, Settings->LakeRadiusMinCells);
	const int32 RadiusMaxCells  = FMath::Max(RadiusMinCells, Settings->LakeRadiusMaxCells);
	const int32 SampleRadiusCells = (RadiusMinCells + RadiusMaxCells) / 2;

	// 16 evenly-spaced angles around the perimeter ring, matching the lake spline resolution.
	constexpr int32 NumRingSamples = 16;

	/**
	 * FLakeCandidate
	 *
	 * DrainFraction  -- fraction of perimeter-ring sample points that are at or below
	 *                   the center height.  0 = perfect closed bowl (no drainage).
	 *                   1 = every perimeter point drains downhill from the center.
	 *                   This is the primary sort key (ascending).
	 *
	 * DepthBelow     -- average immediate-neighbor height minus center height.
	 *                   Larger = deeper depression.
	 *                   Used as a tiebreaker when two candidates share the same
	 *                   DrainFraction bucket (ascending sort with 0.01 tolerance).
	 */
	struct FLakeCandidate
	{
		FIntPoint Cell;
		float     DrainFraction;
		float     DepthBelow;
	};
	TArray<FLakeCandidate> Candidates;

	for (int32 Y = 1; Y < ResY - 1; ++Y)
	{
		for (int32 X = 1; X < ResX - 1; ++X)
		{
			const float H = Heightmap->GetHeightAt(X, Y);

			// ---- Condition 1: strict local minimum (all 8 immediate neighbors higher) ----
			// This guarantees the center of the lake sits in a genuine depression and
			// that the immediate shoreline does not immediately run uphill in all directions.
			bool bIsLocalMin = true;
			float NeighborSum = 0.0f;
			for (int32 dy = -1; dy <= 1 && bIsLocalMin; ++dy)
				for (int32 dx = -1; dx <= 1 && bIsLocalMin; ++dx)
				{
					if (dx == 0 && dy == 0) continue;
					const float NH = Heightmap->GetHeightAt(X + dx, Y + dy);
					if (NH <= H) bIsLocalMin = false;
					NeighborSum += NH;
				}

			if (!bIsLocalMin) continue;

			const float AvgNeighbor = NeighborSum / 8.0f;
			const float DepthBelow  = AvgNeighbor - H;

			// ---- Condition 2: perimeter containment score ----
			// Sample 16 points around a ring at the representative lake radius.
			// Count how many are at or below the center height -- those are the
			// directions where water would drain out of the lake in reality.
			int32 ValidSamples   = 0;
			int32 DrainingPoints = 0;

			for (int32 s = 0; s < NumRingSamples; ++s)
			{
				const float Angle = (static_cast<float>(s) / NumRingSamples) * 2.0f * PI;
				const int32 SX    = X + FMath::RoundToInt(SampleRadiusCells * FMath::Cos(Angle));
				const int32 SY    = Y + FMath::RoundToInt(SampleRadiusCells * FMath::Sin(Angle));

				// Skip samples that fall outside the heightmap.
				if (SX < 0 || SX >= ResX || SY < 0 || SY >= ResY)
					continue;

				++ValidSamples;
				if (Heightmap->GetHeightAt(SX, SY) <= H)
					++DrainingPoints;
			}

			// Out-of-bounds candidates (all samples invalid) are penalised with DrainFraction = 1.
			const float DrainFraction = (ValidSamples > 0)
				? static_cast<float>(DrainingPoints) / static_cast<float>(ValidSamples)
				: 1.0f;

			Candidates.Add({ FIntPoint(X, Y), DrainFraction, DepthBelow });
		}
	}

	// Primary sort: fewest draining perimeter points first (best-contained bowls).
	// Tiebreaker: deepest depression first (more topographic relief).
	// Tolerance of 0.01 on DrainFraction avoids false ties from floating-point noise.
	Candidates.Sort([](const FLakeCandidate& A, const FLakeCandidate& B)
	{
		if (!FMath::IsNearlyEqual(A.DrainFraction, B.DrainFraction, 0.01f))
			return A.DrainFraction < B.DrainFraction;
		return A.DepthBelow > B.DepthBelow;
	});

	const float MinDistSq = MinLakeDistance * MinLakeDistance;
	for (const FLakeCandidate& C : Candidates)
	{
		if (Lakes.Num() >= Settings->MaxLakeCount) break;

		bool bTooClose = false;
		for (const FIntPoint& E : Lakes)
		{
			const float DX = static_cast<float>(C.Cell.X - E.X);
			const float DY = static_cast<float>(C.Cell.Y - E.Y);
			if (DX * DX + DY * DY < MinDistSq) { bTooClose = true; break; }
		}
		if (!bTooClose) Lakes.Add(C.Cell);
	}

	UE_LOG(LogWaterSystem, Display,
		TEXT("FindLakeLocations -- found %d lakes (candidates: %d, requested: %d, sampleRadius: %d cells)."),
		Lakes.Num(), Candidates.Num(), Settings->MaxLakeCount, SampleRadiusCells);

	return Lakes;
}


int32 UWaterSystemBuilder::AppendRiverDefinitions(
	const UHeightmapGenerator* Heightmap,
	const UMapGeneratorSettings* Settings,
	const FVector2D& MapOrigin,
	float CellSize,
	TArray<FIntPoint>& OutFlatTermini,
	TArray<float>&    OutFlatTerminiWorldZ)
{
	if (!Settings->bGenerateRivers || Settings->MaxRiverCount <= 0)
		return 0;

	const int32 BeforeCount = WaterBodyDefinitions.Num();
	TArray<FIntPoint> Sources = FindRiverSources(Heightmap, Settings);

	for (const FIntPoint& Source : Sources)
	{
		TArray<FIntPoint> Path = TraceRiverPath(Heightmap, Settings, Source);
		if (Path.Num() < 2) continue;

		// Check whether the river stopped inside the landscape (flat basin).
		// A lake should be placed at the terminus unless the river flowed off
		// the heightmap edge (in which case no lake is needed: it drained away).
		const FIntPoint& Terminus  = Path.Last();
		const FIntPoint  MapRes    = Heightmap->GetResolution();
		const bool bAtMapEdge =
			Terminus.X <= 0 || Terminus.X >= MapRes.X - 1 ||
			Terminus.Y <= 0 || Terminus.Y >= MapRes.Y - 1;

		const float TermH      = Heightmap->GetHeightAt(Terminus.X, Terminus.Y);
		const float TermWorldZ = (TermH - 0.5f) * 12800.f;

		if (bAtMapEdge)
		{
			UE_LOG(LogWaterSystem, Display,
				TEXT("AppendRiverDefinitions -- river from (%d,%d) exited map at edge (%d,%d). No lake."),
				Source.X, Source.Y, Terminus.X, Terminus.Y);
		}
		else if (TermH <= Settings->SeaLevel)
		{
			// River drained to sea level inside the map (coastal terminus).
			// No terminus lake: the river mouth meets the ocean.
			UE_LOG(LogWaterSystem, Display,
				TEXT("AppendRiverDefinitions -- river from (%d,%d) reached sea level at (%d,%d) h=%.3f (SeaLevel=%.3f). No lake (coastal mouth)."),
				Source.X, Source.Y, Terminus.X, Terminus.Y, TermH, Settings->SeaLevel);
		}
		else
		{
			// True flat-basin terminus above sea level: place a terminus lake.
			OutFlatTermini.Add(Terminus);
			OutFlatTerminiWorldZ.Add(TermWorldZ);

			UE_LOG(LogWaterSystem, Display,
				TEXT("AppendRiverDefinitions -- river from (%d,%d) reached flat basin at (%d,%d) h=%.3f (Z=%.0f cm); terminus lake queued."),
				Source.X, Source.Y, Terminus.X, Terminus.Y, TermH, TermWorldZ);
		}

		FWaterBodyDefinition River;
		River.WaterType = EWaterBodyType::River;
		River.Depth     = 100.0f;

		// River width: starts very narrow at the source, widens linearly to full
		// width at the mouth.  SourceWidth is the width of the first spline point;
		// MouthWidth is the width of the last spline point and is also stored in
		// River.Width for use by the WaterZone bounds computation.
		constexpr float SourceWidth = 200.0f;   // 2 m at source
		constexpr float MouthWidth  = 4000.0f;  // 40 m at mouth
		River.Width = MouthWidth;

		const int32 N = Path.Num();
		River.SplinePoints.Reserve(N);
		River.PointWidths.Reserve(N);

		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D WP = GridToWorld(Path[i], MapOrigin, CellSize);
			const float H      = Heightmap->GetHeightAt(Path[i].X, Path[i].Y);
			const float WorldZ = (H - 0.5f) * 12800.0f;
			River.SplinePoints.Add(FVector(WP.X, WP.Y, WorldZ));

			// Linear interpolation: t=0 at source, t=1 at mouth.
			const float t = (N > 1) ? static_cast<float>(i) / static_cast<float>(N - 1) : 1.f;
			River.PointWidths.Add(FMath::Lerp(SourceWidth, MouthWidth, t));
		}

		WaterBodyDefinitions.Add(River);
	}

	return WaterBodyDefinitions.Num() - BeforeCount;
}

int32 UWaterSystemBuilder::AppendLakeDefinitions(
	const UHeightmapGenerator* Heightmap,
	const UMapGeneratorSettings* Settings,
	const FVector2D& MapOrigin,
	float CellSize,
	FRandomStream& Stream,
	const TArray<FIntPoint>& ForcedCenters,
	const TArray<float>&     ForcedCenterWorldZ,
	bool bForcedCentersOnly)
{
	// Terminus lakes (ForcedCenters) are always generated regardless of the
	// bGenerateLakes flag — they are river endpoints, not optional decoration.
	// Only the standalone lake discovery pass is gated by bGenerateLakes / MaxLakeCount.
	const bool bCanGenerateStandalone = Settings->bGenerateLakes && Settings->MaxLakeCount > 0;

	if (ForcedCenters.IsEmpty() && !bCanGenerateStandalone)
		return 0;

	const int32 BeforeCount = WaterBodyDefinitions.Num();

	// Build the candidate list.
	// ForcedCenters (river-terminus lakes) are always added first.
	// FindLakeLocations() fills remaining capacity UNLESS bForcedCentersOnly is set,
	// which is used for the terminus-lake pass so that standalone lake discovery
	// happens in a separate, later call.
	TArray<FIntPoint> LakeCenters;
	TArray<float>     LakeCenterWorldZ; // parallel: -FLT_MAX = derive from heightmap

	for (int32 fi = 0; fi < ForcedCenters.Num(); ++fi)
	{
		if (!LakeCenters.Contains(ForcedCenters[fi]))
		{
			LakeCenters.Add(ForcedCenters[fi]);
			LakeCenterWorldZ.Add(ForcedCenterWorldZ.IsValidIndex(fi)
				? ForcedCenterWorldZ[fi]
				: -FLT_MAX);
		}
	}

	// Only run the expensive auto-discovery if explicitly requested and capacity remains.
	if (!bForcedCentersOnly && bCanGenerateStandalone && LakeCenters.Num() < Settings->MaxLakeCount)
	{
		TArray<FIntPoint> Found = FindLakeLocations(Heightmap, Settings);
		for (const FIntPoint& F : Found)
		{
			if (!LakeCenters.Contains(F))
			{
				LakeCenters.Add(F);
				LakeCenterWorldZ.Add(-FLT_MAX); // derive Z from heightmap
			}
		}
	}

	// Clamp and sanitize the radius range from settings
	const int32 RadiusMinCells = FMath::Max(2, Settings->LakeRadiusMinCells);
	const int32 RadiusMaxCells = FMath::Max(RadiusMinCells, Settings->LakeRadiusMaxCells);

	// Minimum shore-to-shore gap in world units.
	// MinLakeDistance is expressed in cells, so convert to world space here.
	const float MinShoreGap = MinLakeDistance * CellSize;

	// Track already-placed lakes so we can reject any new lake whose shores
	// would overlap (or come closer than MinShoreGap) with an existing one.
	struct FPlacedLake { FVector2D Center; float Radius; };
	TArray<FPlacedLake> PlacedLakes;

	// Pre-populate with lakes already placed in previous passes (e.g. terminus lakes
	// from Pass 2) so that standalone lakes in Pass 3 don't overlap them.
	for (const FWaterBodyDefinition& ExistingDef : WaterBodyDefinitions)
	{
		if (ExistingDef.WaterType != EWaterBodyType::Lake || ExistingDef.SplinePoints.IsEmpty())
		{
			continue;
		}

		FVector2D Centroid(0.f, 0.f);
		for (const FVector& Pt : ExistingDef.SplinePoints)
		{
			Centroid.X += Pt.X;
			Centroid.Y += Pt.Y;
		}
		Centroid /= static_cast<float>(ExistingDef.SplinePoints.Num());

		float AvgRadius = 0.f;
		for (const FVector& Pt : ExistingDef.SplinePoints)
		{
			AvgRadius += FVector2D::Distance(Centroid, FVector2D(Pt.X, Pt.Y));
		}
		AvgRadius /= static_cast<float>(ExistingDef.SplinePoints.Num());

		PlacedLakes.Add({ Centroid, AvgRadius });
	}

	for (int32 ci = 0; ci < LakeCenters.Num(); ++ci)
	{
		const FIntPoint& Center = LakeCenters[ci];

		// Determine whether this is a forced terminus lake (ForcedZ != -FLT_MAX).
		// Terminus lakes have already been sea-level-validated in AppendRiverDefinitions;
		// only standalone auto-discovered lakes need the sea-level check here.
		const float ForcedZEarly = LakeCenterWorldZ.IsValidIndex(ci) ? LakeCenterWorldZ[ci] : -FLT_MAX;
		const bool  bIsTerminus  = (ForcedZEarly > -FLT_MAX);

		const float CenterHCheck = Heightmap->GetHeightAt(Center.X, Center.Y);
		if (!bIsTerminus && CenterHCheck <= Settings->SeaLevel)
		{
			UE_LOG(LogWaterSystem, Display,
				TEXT("AppendLakeDefinitions -- skipped standalone lake at cell (%d, %d): height %.3f <= sea level %.3f."),
				Center.X, Center.Y, CenterHCheck, Settings->SeaLevel);
			continue;
		}

		FWaterBodyDefinition Lake;
		Lake.WaterType = EWaterBodyType::Lake;
		Lake.Depth     = 150.0f;

		// Each lake gets a unique random radius within [LakeRadiusMinCells, LakeRadiusMaxCells].
		// Terminus lakes additionally enforce a minimum radius of 2× the river mouth width
		// (MouthWidth = 4000 cm) so the lake is always visually wider than the river feeding it.
		const int32   RadiusCells    = Stream.RandRange(RadiusMinCells, RadiusMaxCells);
		float         LakeRadius     = static_cast<float>(RadiusCells) * CellSize;
		if (bIsTerminus)
		{
			constexpr float MouthWidth      = 4000.0f; // must match AppendRiverDefinitions
			constexpr float MinTerminusMult = 2.0f;
			const float     MinTerminusRadius = MouthWidth * MinTerminusMult;
			if (LakeRadius < MinTerminusRadius)
			{
				LakeRadius = MinTerminusRadius;
			}
		}

		// Scale Width proportionally to radius so shore visuals stay consistent.
		Lake.Width = 400.0f * (LakeRadius / (20.0f * CellSize));

		const FVector2D CP = GridToWorld(Center, MapOrigin, CellSize);

		// Z elevation: for river-terminus lakes use the river's lowest point Z so
		// the lake surface is never higher than the river that feeds it.
		// For regular lakes derive Z from the heightmap at the lake centre cell.
		const float ForcedZ = LakeCenterWorldZ.IsValidIndex(ci) ? LakeCenterWorldZ[ci] : -FLT_MAX;
		const float CenterZ = (ForcedZ > -FLT_MAX)
			? ForcedZ
			: (Heightmap->GetHeightAt(Center.X, Center.Y) - 0.5f) * 12800.0f;

		if (ForcedZ > -FLT_MAX)
		{
			UE_LOG(LogWaterSystem, Display,
				TEXT("AppendLakeDefinitions -- terminus lake at (%d,%d) using river Z=%.0f cm."),
				Center.X, Center.Y, CenterZ);
		}

		// Radius-aware overlap check: reject standalone lake candidates that would
		// overlap an existing lake.  Terminus lakes (bIsTerminus) are never rejected
		// by this check — a river must always have a destination lake regardless of
		// proximity to other lakes; the WaterZone merge pass handles the zone overlap.
		bool bOverlaps = false;
		if (!bIsTerminus)
		{
			for (const FPlacedLake& Placed : PlacedLakes)
			{
				const float CenterDist = FVector2D::Distance(CP, Placed.Center);
				if (CenterDist < Placed.Radius + LakeRadius + MinShoreGap)
				{
					bOverlaps = true;
					break;
				}
			}
			if (bOverlaps)
			{
				UE_LOG(LogWaterSystem, Display,
					TEXT("AppendLakeDefinitions -- skipped overlapping standalone lake at cell (%d, %d): would overlap an existing lake."),
					Center.X, Center.Y);
				continue;
			}
		}
		else if (!PlacedLakes.IsEmpty())
		{
			// Terminus lake: log overlap but continue regardless.
			for (const FPlacedLake& Placed : PlacedLakes)
			{
				const float CenterDist = FVector2D::Distance(CP, Placed.Center);
				if (CenterDist < Placed.Radius + LakeRadius + MinShoreGap)
				{
					UE_LOG(LogWaterSystem, Display,
						TEXT("AppendLakeDefinitions -- terminus lake at cell (%d, %d) overlaps an existing lake but is placed anyway (river must have destination)."),
						Center.X, Center.Y);
					break;
				}
			}
		}

		// Candidate accepted -- record it before building geometry.
		PlacedLakes.Add({ CP, LakeRadius });

		UE_LOG(LogWaterSystem, Display,
			TEXT("AppendLakeDefinitions -- lake center cell (%d, %d) world=(%.0f, %.0f, %.0f) radius=%.0f cm (%d cells)."),
			Center.X, Center.Y, CP.X, CP.Y, CenterZ, LakeRadius, RadiusCells);

		// WaterBodyLake uses a closed spline loop (SetClosedLoop(true) is called automatically
		// by UpdateSplineComponent for lake/ocean types). Do NOT repeat the first point --
		// a duplicate closing point causes a degenerate segment that fails Delaunay triangulation.
		constexpr int32 NumPoints = 16;
		const float Irregularity = FMath::Clamp(Settings->LakeShapeIrregularity, 0.0f, 1.0f);

		// Step 1 -- generate per-point raw radii with random perturbation.
		// Each radius is offset by a uniform random value in [-Irregularity, +Irregularity]
		// relative to the base radius, so a perfectly circular lake is preserved when
		// Irregularity == 0.
		TArray<float> Radii;
		Radii.SetNum(NumPoints);
		for (int32 i = 0; i < NumPoints; ++i)
		{
			const float Perturbation = Stream.FRandRange(-Irregularity, Irregularity);
			Radii[i] = LakeRadius * (1.0f + Perturbation);
		}

		// Step 2 -- circular 3-point moving average to smooth sharp spikes while
		// preserving the overall irregular silhouette.
		// Without smoothing, adjacent large/small offsets create unrealistically
		// jagged inlets that look artificial and can break Delaunay triangulation.
		if (Irregularity > 0.0f)
		{
			TArray<float> Smoothed;
			Smoothed.SetNum(NumPoints);
			for (int32 i = 0; i < NumPoints; ++i)
			{
				const int32 Prev = (i - 1 + NumPoints) % NumPoints;
				const int32 Next = (i + 1) % NumPoints;
				Smoothed[i] = (Radii[Prev] + Radii[i] + Radii[Next]) / 3.0f;
			}
			Radii = Smoothed;
		}

		// Step 3 -- emit the final spline points using the smoothed radii.
		for (int32 i = 0; i < NumPoints; ++i)
		{
			const float Angle = (static_cast<float>(i) / NumPoints) * 2.0f * PI;
			Lake.SplinePoints.Add(FVector(
				CP.X + Radii[i] * FMath::Cos(Angle),
				CP.Y + Radii[i] * FMath::Sin(Angle),
				CenterZ));
		}
		// No closing point: the closed loop is handled by WaterSplineComponent::SetClosedLoop(true).

		WaterBodyDefinitions.Add(Lake);
	}

	return WaterBodyDefinitions.Num() - BeforeCount;
}

void UWaterSystemBuilder::SpawnWaterBodiesRuntime(const UHeightmapGenerator* Heightmap, float CellSize)
{
	UWorld* World = GetWorld();
	if (!World)
		return;

	SpawnedWaterBodies.Empty();
	SpawnedWaterZones.Empty();

	// Per-type counters for human-readable Outliner labels.
	int32 LakeIndex  = 0;
	int32 RiverIndex = 0;

	for (int32 i = 0; i < WaterBodyDefinitions.Num(); ++i)
	{
		const FWaterBodyDefinition& Def = WaterBodyDefinitions[i];

		// ---- Outliner labels ----
		FString BodyLabel;
		switch (Def.WaterType)
		{
			case EWaterBodyType::Lake:
				BodyLabel = FString::Printf(TEXT("WaterBodyLake%d"),  ++LakeIndex);  break;
			case EWaterBodyType::River:
				BodyLabel = FString::Printf(TEXT("WaterBodyRiver%d"), ++RiverIndex); break;
			default:
				BodyLabel = FString::Printf(TEXT("WaterBody%d"), i);                 break;
		}
		const FString ZoneLabel = FString::Printf(TEXT("WaterZone_%s"), *BodyLabel);

		// ----------------------------------------------------------------
		// Zone selection / creation
		//
		// Terminus lakes (LinkedBodyIndex >= 0) do NOT get their own zone.
		// They share the parent river's zone, which was already sized to
		// include the lake's spline points (see ZoneDef below).
		// ----------------------------------------------------------------
		AActor* Zone = nullptr;
		const bool bIsTerminusLake =
			(Def.WaterType == EWaterBodyType::Lake && Def.LinkedBodyIndex >= 0);

		if (bIsTerminusLake)
		{
			// Reuse the parent river's zone.
			const int32 RiverIdx = Def.LinkedBodyIndex;
			Zone = SpawnedWaterZones.IsValidIndex(RiverIdx) ? SpawnedWaterZones[RiverIdx] : nullptr;

			UE_LOG(LogWaterSystem, Display,
				TEXT("SpawnWaterBodiesRuntime -- '%s' is a terminus lake; sharing zone of river[%d]."),
				*BodyLabel, RiverIdx);

			// Keep SpawnedWaterZones index-aligned with WaterBodyDefinitions.
			SpawnedWaterZones.Add(Zone);
		}
		else
		{
			// For rivers: build a combined definition whose SplinePoints include
			// the linked terminus lake's perimeter points so the zone is large
			// enough to cover both the river and its lake in one bounding box.
			FWaterBodyDefinition ZoneDef = Def;
			if (Def.WaterType == EWaterBodyType::River)
			{
				for (const FWaterBodyDefinition& Candidate : WaterBodyDefinitions)
				{
					if (Candidate.WaterType == EWaterBodyType::Lake
						&& Candidate.LinkedBodyIndex == i)
					{
						ZoneDef.SplinePoints.Append(Candidate.SplinePoints);
					}
				}
			}

			Zone = SpawnWaterZone(World, ZoneDef, CellSize, i);
			SpawnedWaterZones.Add(Zone);

			if (IsValid(Zone))
			{
#if WITH_EDITOR
				if (GIsEditor)
				{
					Zone->SetActorLabel(ZoneLabel);
					Zone->SetFolderPath(FName("Waters"));
				}
#endif
				UE_LOG(LogWaterSystem, Display,
					TEXT("SpawnWaterBodiesRuntime -- zone '%s' spawned."), *ZoneLabel);
			}
			else
			{
				UE_LOG(LogWaterSystem, Warning,
					TEXT("SpawnWaterBodiesRuntime -- zone '%s' spawn failed."), *ZoneLabel);
			}
		}

		// ---- Spawn water body, linked to its zone ----
		AActor* Spawned = SpawnWaterBody(World, Def, Zone);
		if (Spawned)
		{
#if WITH_EDITOR
			if (GIsEditor)
			{
				Spawned->SetActorLabel(BodyLabel);
				Spawned->SetFolderPath(FName("Waters"));
			}
#endif
			SpawnedWaterBodies.Add(Spawned);
			UE_LOG(LogWaterSystem, Display,
				TEXT("SpawnWaterBodiesRuntime -- body '%s' spawned."), *BodyLabel);
		}

		// ---- Rebuild zone after each body registration ----
		// ForceUpdateWaterInfoTexture populates the zone's WaterInfoTexture
		// with the just-registered body's silhouette.  Called per body so the
		// zone accumulates all registered bodies progressively.
		if (IsValid(Zone) && IsValid(Spawned))
		{
#if WITH_EDITOR
			Zone->RerunConstructionScripts();
#endif
			WaterZoneSpawnUtils::MakeActorComponentsMovable(Zone);

			UFunction* ForceRebuildFn = Zone->FindFunction(TEXT("ForceUpdateWaterInfoTexture"));
			if (ForceRebuildFn)
			{
				Zone->ProcessEvent(ForceRebuildFn, nullptr);
				UE_LOG(LogWaterSystem, Display,
					TEXT("SpawnWaterBodiesRuntime -- ForceUpdateWaterInfoTexture called on zone of '%s'."),
					*BodyLabel);
			}
			else
			{
				UE_LOG(LogWaterSystem, Warning,
					TEXT("SpawnWaterBodiesRuntime -- ForceUpdateWaterInfoTexture not found; zone of '%s' may not render."),
					*BodyLabel);
			}

#if WITH_EDITOR
			if (GIsEditor && !bIsTerminusLake)
			{
				// Terminus lakes share the river's zone; don't overwrite its label.
				Zone->SetActorLabel(ZoneLabel);
				Zone->SetFolderPath(FName("Waters"));
				Zone->PostEditChange();
			}
#endif
		}
	}

	UE_LOG(LogWaterSystem, Display,
		TEXT("SpawnWaterBodiesRuntime -- done. Bodies: %d, Zones: %d."),
		SpawnedWaterBodies.Num(), SpawnedWaterZones.Num());

	// Merge any overlapping WaterZones before the final rebuild so that every
	// water body ends up inside exactly one non-overlapping zone.
	MergeOverlappingWaterZones(World);

	// Final pass: one more rebuild of all zones after every body is in place.
	RebuildWaterZone(World);
}

void UWaterSystemBuilder::MergeOverlappingWaterZones(UWorld* World)
{
	if (!World) return;

	// -------------------------------------------------------------------------
	// Build a deduplicated list of zone records.
	// SpawnedWaterZones is indexed by water body definition and may contain the
	// same zone actor multiple times (terminus lakes share the river's zone).
	// -------------------------------------------------------------------------
	struct FZoneRecord
	{
		AActor*   Actor;
		FVector2D Center;     // world XY
		FVector2D FullExtent; // full width × height (as stored in ZoneExtent property)

		FVector2D Min() const { return Center - FullExtent * 0.5f; }
		FVector2D Max() const { return Center + FullExtent * 0.5f; }

		bool Overlaps(const FZoneRecord& O) const
		{
			return Min().X < O.Max().X && Max().X > O.Min().X
				&& Min().Y < O.Max().Y && Max().Y > O.Min().Y;
		}
	};

	TArray<FZoneRecord> Zones;
	for (AActor* Zone : SpawnedWaterZones)
	{
		if (!IsValid(Zone)) continue;
		bool bAlreadyAdded = false;
		for (const FZoneRecord& R : Zones)
			if (R.Actor == Zone) { bAlreadyAdded = true; break; }
		if (bAlreadyAdded) continue;

		const FVector Loc = Zone->GetActorLocation();
		FZoneRecord Rec;
		Rec.Actor      = Zone;
		Rec.Center     = FVector2D(Loc.X, Loc.Y);
		Rec.FullExtent = ReadZoneExtent(Zone);
		Zones.Add(Rec);
	}

	UE_LOG(LogWaterSystem, Display,
		TEXT("MergeOverlappingWaterZones -- start: %d unique zones."), Zones.Num());

	// -------------------------------------------------------------------------
	// Iteratively find the first overlapping pair and merge it.
	// Repeat until no overlapping pairs remain.
	// -------------------------------------------------------------------------
	bool bMerged;
	int32 MergeCount = 0;
	do
	{
		bMerged = false;
		for (int32 i = 0; i < Zones.Num() && !bMerged; ++i)
		{
			for (int32 j = i + 1; j < Zones.Num(); ++j)
			{
				if (!Zones[i].Overlaps(Zones[j])) continue;

				// ----- Compute union AABB -----
				const FVector2D NewMin(
					FMath::Min(Zones[i].Min().X, Zones[j].Min().X),
					FMath::Min(Zones[i].Min().Y, Zones[j].Min().Y));
				const FVector2D NewMax(
					FMath::Max(Zones[i].Max().X, Zones[j].Max().X),
					FMath::Max(Zones[i].Max().Y, Zones[j].Max().Y));
				const FVector2D NewCenter = (NewMin + NewMax) * 0.5f;
				const FVector2D NewExtent = NewMax - NewMin;

				// ----- Update surviving zone (i) geometry -----
				Zones[i].Center     = NewCenter;
				Zones[i].FullExtent = NewExtent;
				Zones[i].Actor->SetActorLocation(FVector(NewCenter.X, NewCenter.Y, 0.f));
				WaterZoneSpawnUtils::MakeActorComponentsMovable(Zones[i].Actor);
				WaterZoneSpawnUtils::ApplyZoneExtent(Zones[i].Actor, NewExtent);
				WaterZoneSpawnUtils::ApplyBoundsBoxExtent(Zones[i].Actor, i, NewExtent);

				// Recalculate tile mesh sizing for the new (larger) extent.
				const float MaxDim = FMath::Max(NewExtent.X, NewExtent.Y);
				const int32 NewTiles = FMath::Clamp(
					FMath::CeilToInt((MaxDim * 2.0f) / 2400.0f) + 2, 16, 240);
				const float NewTileSize = FMath::Max((MaxDim * 2.0f) / static_cast<float>(NewTiles), 800.0f);
				WaterZoneSpawnUtils::ConfigureWaterMeshTiling(Zones[i].Actor, i, NewTiles, NewTileSize);

				// ----- Relink every water body that pointed to the dying zone (j) -----
				AActor* DyingZone    = Zones[j].Actor;
				AActor* SurvivingZone = Zones[i].Actor;
				for (int32 k = 0; k < SpawnedWaterZones.Num(); ++k)
				{
					if (SpawnedWaterZones[k] != DyingZone) continue;
					SpawnedWaterZones[k] = SurvivingZone;

					if (SpawnedWaterBodies.IsValidIndex(k) && IsValid(SpawnedWaterBodies[k]))
					{
						AActor* Body = SpawnedWaterBodies[k];
						LinkZoneOnObject(Body, SurvivingZone);
						if (UWaterBodyComponent* WComp = Body->FindComponentByClass<UWaterBodyComponent>())
						{
							LinkWaterBodyToZone(WComp, SurvivingZone);
							LinkZoneOnObject(WComp, SurvivingZone);
						}
					}
				}

				UE_LOG(LogWaterSystem, Display,
					TEXT("MergeOverlappingWaterZones -- merged '%s' into '%s'. New extent=(%.0f, %.0f) cm."),
					*DyingZone->GetActorNameOrLabel(), *SurvivingZone->GetActorNameOrLabel(),
					NewExtent.X, NewExtent.Y);

				// ----- Destroy the dying zone and remove from local list -----
				DyingZone->Destroy();
				Zones.RemoveAt(j);

				++MergeCount;
				bMerged = true;
				break;
			}
		}
	} while (bMerged);

	UE_LOG(LogWaterSystem, Display,
		TEXT("MergeOverlappingWaterZones -- done. %d merge(s) performed, %d zone(s) remain."),
		MergeCount, Zones.Num());

	if (MergeCount == 0) return; // nothing changed, no rebuild needed

	// -------------------------------------------------------------------------
	// Post-merge rebuild pass.
	//
	// After merging, zones have new bounds but their internal water body registry
	// is stale (some bodies were reassigned from a destroyed zone).  Manually
	// tweaking ZoneExtent in the editor triggers PostEditChangeProperty →
	// RerunConstructionScripts → body re-registration → ForceUpdateWaterInfoTexture.
	// We replicate that chain here:
	//
	//   1. RerunConstructionScripts on each surviving zone   (clears + rebuilds registry)
	//   2. For every water body: relink zone ref, RerunConstructionScripts
	//      (body's OnConstruction re-registers itself with the zone)
	//   3. OnWaterBodyChanged on each body   (rebuilds render mesh)
	//   4. ForceUpdateWaterInfoTexture on each surviving zone
	//   5. PostEditChange on zone (editor only – triggers property-change notification)
	// -------------------------------------------------------------------------

	// Collect unique surviving zones.
	TArray<AActor*> SurvivingZones;
	for (AActor* Zone : SpawnedWaterZones)
	{
		if (!IsValid(Zone)) continue;
		if (!SurvivingZones.Contains(Zone)) SurvivingZones.Add(Zone);
	}

	// Step 1 -- reset zone internal state.
	for (AActor* Zone : SurvivingZones)
	{
#if WITH_EDITOR
		Zone->RerunConstructionScripts();
#endif
		WaterZoneSpawnUtils::MakeActorComponentsMovable(Zone);
	}

	// Step 2 & 3 -- re-register every water body with its (possibly new) zone.
	for (int32 k = 0; k < SpawnedWaterBodies.Num(); ++k)
	{
		AActor* Body = SpawnedWaterBodies.IsValidIndex(k) ? SpawnedWaterBodies[k].Get() : nullptr;
		if (!IsValid(Body)) continue;

		AActor* Zone = SpawnedWaterZones.IsValidIndex(k) ? SpawnedWaterZones[k].Get() : nullptr;

		// Relink zone reference before construction so OnConstruction sees the zone.
		LinkZoneOnObject(Body, Zone);
		if (UWaterBodyComponent* WComp = Body->FindComponentByClass<UWaterBodyComponent>())
		{
			LinkWaterBodyToZone(WComp, Zone);
			LinkZoneOnObject(WComp, Zone);
		}

#if WITH_EDITOR
		Body->RerunConstructionScripts();
#endif
		WaterZoneSpawnUtils::MakeActorComponentsMovable(Body);

		if (UWaterBodyComponent* WComp = Body->FindComponentByClass<UWaterBodyComponent>())
		{
			LinkWaterBodyToZone(WComp, Zone);
			LinkZoneOnObject(WComp, Zone);
			WaterZoneSpawnUtils::MakeSplineMeshComponentsMovable(Body);

			FOnWaterBodyChangedParams Params;
			Params.bShapeOrPositionChanged = true;
			Params.bUserTriggered          = true;
			WComp->OnWaterBodyChanged(Params);
		}
	}

	// Step 4 & 5 -- force zone to regenerate its water info texture.
	for (AActor* Zone : SurvivingZones)
	{
#if WITH_EDITOR
		Zone->RerunConstructionScripts();
#endif
		WaterZoneSpawnUtils::MakeActorComponentsMovable(Zone);

		UFunction* ForceRebuildFn = Zone->FindFunction(TEXT("ForceUpdateWaterInfoTexture"));
		if (ForceRebuildFn)
		{
			Zone->ProcessEvent(ForceRebuildFn, nullptr);
		}

#if WITH_EDITOR
		if (GIsEditor)
		{
			Zone->PostEditChange();
		}
#endif
	}

	UE_LOG(LogWaterSystem, Display,
		TEXT("MergeOverlappingWaterZones -- post-merge rebuild complete for %d zone(s)."),
		SurvivingZones.Num());
}

bool UWaterSystemBuilder::BuildWaterBodies(
	const UHeightmapGenerator* Heightmap,
	const UMapGeneratorSettings* Settings,
	float CellSize,
	bool bSpawnActors)
{
	WaterBodyDefinitions.Empty();

	if (!Heightmap || !Heightmap->IsInitialized())
	{
		UE_LOG(LogWaterSystem, Warning,
			TEXT("BuildWaterBodies -- Heightmap is null or uninitialized."));
		return false;
	}

	if (!Settings)
	{
		UE_LOG(LogWaterSystem, Warning,
			TEXT("BuildWaterBodies -- Settings is null."));
		return false;
	}

	const FIntPoint Res = Heightmap->GetResolution();
	const FVector2D MapOrigin(
		-(Res.X - 1) * CellSize * 0.5f,
		-(Res.Y - 1) * CellSize * 0.5f);

	// Seeded stream for deterministic lake radius randomisation.
	// Same Seed value always produces the same lake sizes.
	const int32 SeedValue = Settings->Seed != 0 ? Settings->Seed : FMath::Rand();
	FRandomStream Stream(SeedValue);

	// Pass 1 -- Rivers.
	// Collect flat-basin termini (rivers that stopped above sea level inside the map).
	TArray<FIntPoint> FlatTermini;
	TArray<float>     FlatTerminiWorldZ;
	const int32 RiverCount = AppendRiverDefinitions(
		Heightmap, Settings, MapOrigin, CellSize, FlatTermini, FlatTerminiWorldZ);
	UE_LOG(LogWaterSystem, Display,
		TEXT("BuildWaterBodies -- %d rivers generated, %d flat-basin termini collected."),
		RiverCount, FlatTermini.Num());

	// Pass 2 -- Terminus lakes (one per river that ended in a flat basin).
	// bForcedCentersOnly=true: only FlatTermini are placed, FindLakeLocations is skipped
	// so that standalone lake discovery happens in the separate pass below.
	const int32 TerminusLakeCount = AppendLakeDefinitions(
		Heightmap, Settings, MapOrigin, CellSize, Stream,
		FlatTermini, FlatTerminiWorldZ, /*bForcedCentersOnly=*/true);
	UE_LOG(LogWaterSystem, Display,
		TEXT("BuildWaterBodies -- %d terminus lake(s) generated from river flat-basin termini."),
		TerminusLakeCount);

	// Pass 3 -- Standalone lakes (auto-discovered depressions, no forced centers).
	// Fills remaining MaxLakeCount capacity after terminus lakes.
	const int32 StandaloneLakeCount = AppendLakeDefinitions(
		Heightmap, Settings, MapOrigin, CellSize, Stream,
		{}, {}, /*bForcedCentersOnly=*/false);
	UE_LOG(LogWaterSystem, Display,
		TEXT("BuildWaterBodies -- %d standalone lake(s) generated."), StandaloneLakeCount);

	const int32 LakeCount = TerminusLakeCount + StandaloneLakeCount;

	// -------------------------------------------------------------------------
	// Pass 4 -- Trim river tails that run inside lakes.
	//
	// Rule: while BOTH the last and second-to-last spline points are inside any
	// lake, remove the last point (the river is already "in" the lake there).
	// Stop when only the final point is inside a lake — that is the natural
	// mouth where the river meets the lake shore.
	// If every point of a river is inside a lake the river is discarded entirely.
	// -------------------------------------------------------------------------
	{
		// Build a flat list of lake circles (centroid + average radius) for
		// fast "is this point inside any lake?" queries.
		struct FLakeCircle { FVector2D Center; float Radius; };
		TArray<FLakeCircle> LakeCircles;
		for (const FWaterBodyDefinition& Def : WaterBodyDefinitions)
		{
			if (Def.WaterType != EWaterBodyType::Lake || Def.SplinePoints.IsEmpty())
				continue;

			FVector2D Centroid(0.f, 0.f);
			for (const FVector& Pt : Def.SplinePoints)
			{ Centroid.X += Pt.X; Centroid.Y += Pt.Y; }
			Centroid /= static_cast<float>(Def.SplinePoints.Num());

			float AvgR = 0.f;
			for (const FVector& Pt : Def.SplinePoints)
				AvgR += FVector2D::Distance(Centroid, FVector2D(Pt.X, Pt.Y));
			AvgR /= static_cast<float>(Def.SplinePoints.Num());

			LakeCircles.Add({ Centroid, AvgR });
		}

		auto IsInsideLake = [&](const FVector& Pt) -> bool
		{
			const FVector2D P2D(Pt.X, Pt.Y);
			for (const FLakeCircle& LC : LakeCircles)
			{
				if (FVector2D::Distance(P2D, LC.Center) <= LC.Radius)
					return true;
			}
			return false;
		};

		// Iterate backwards so RemoveAt doesn't shift indices of earlier entries.
		for (int32 ri = WaterBodyDefinitions.Num() - 1; ri >= 0; --ri)
		{
			FWaterBodyDefinition& River = WaterBodyDefinitions[ri];
			if (River.WaterType != EWaterBodyType::River || River.SplinePoints.IsEmpty())
				continue;

			const int32 PointsBefore = River.SplinePoints.Num();

			// Trim: remove the last point while the last THREE points are all
			// inside a lake (so exactly 2 tail points remain inside the lake).
			while (River.SplinePoints.Num() >= 3
				&& IsInsideLake(River.SplinePoints.Last())
				&& IsInsideLake(River.SplinePoints[River.SplinePoints.Num() - 2])
				&& IsInsideLake(River.SplinePoints[River.SplinePoints.Num() - 3]))
			{
				River.SplinePoints.RemoveAt(River.SplinePoints.Num() - 1);
			}

			// Keep PointWidths in sync with SplinePoints after trimming.
			if (River.PointWidths.Num() > River.SplinePoints.Num())
			{
				River.PointWidths.SetNum(River.SplinePoints.Num());
			}

			// If every remaining point is still inside a lake the river is
			// completely submerged — discard it.
			bool bAllInLake = true;
			for (const FVector& Pt : River.SplinePoints)
			{
				if (!IsInsideLake(Pt))
				{
					bAllInLake = false;
					break;
				}
			}

			if (bAllInLake)
			{
				UE_LOG(LogWaterSystem, Display,
					TEXT("BuildWaterBodies -- river[%d] is fully inside lake(s); discarded."), ri);
				WaterBodyDefinitions.RemoveAt(ri);
			}
			else if (River.SplinePoints.Num() != PointsBefore)
			{
				UE_LOG(LogWaterSystem, Display,
					TEXT("BuildWaterBodies -- river[%d] trimmed from %d to %d spline points."),
					ri, PointsBefore, River.SplinePoints.Num());
			}
		}
	}

	// -------------------------------------------------------------------------
	// Link terminus lakes to their parent rivers.
	//
	// A terminus lake's centroid is offset from the river's last spline point by
	// approximately MeanRadius * CellSize in the flow direction.  We search for
	// the nearest un-linked lake within 2× that distance and tag it with the
	// river's definition index so the spawner can share the zone.
	// -------------------------------------------------------------------------
	{
		const int32 MeanRadiusCells =
			(FMath::Max(2, Settings->LakeRadiusMinCells) +
			 FMath::Max(2, Settings->LakeRadiusMaxCells)) / 2;
		const float MaxLinkDist = static_cast<float>(MeanRadiusCells) * CellSize * 2.0f;

		for (int32 ri = 0; ri < WaterBodyDefinitions.Num(); ++ri)
		{
			FWaterBodyDefinition& River = WaterBodyDefinitions[ri];
			if (River.WaterType != EWaterBodyType::River || River.SplinePoints.IsEmpty())
				continue;

			const FVector2D RiverEnd(River.SplinePoints.Last().X, River.SplinePoints.Last().Y);

			for (int32 li = 0; li < WaterBodyDefinitions.Num(); ++li)
			{
				FWaterBodyDefinition& Lake = WaterBodyDefinitions[li];
				if (Lake.WaterType != EWaterBodyType::Lake)   continue;
				if (Lake.LinkedBodyIndex >= 0)                continue; // already owned
				if (Lake.SplinePoints.IsEmpty())              continue;

				FVector2D Centroid(0.f, 0.f);
				for (const FVector& Pt : Lake.SplinePoints)
				{ Centroid.X += Pt.X; Centroid.Y += Pt.Y; }
				Centroid /= static_cast<float>(Lake.SplinePoints.Num());

				if (FVector2D::Distance(RiverEnd, Centroid) <= MaxLinkDist)
				{
					Lake.LinkedBodyIndex = ri;
					UE_LOG(LogWaterSystem, Display,
						TEXT("BuildWaterBodies -- linked lake[%d] to river[%d] (dist=%.0f cm, max=%.0f cm)."),
						li, ri, FVector2D::Distance(RiverEnd, Centroid), MaxLinkDist);
					break;
				}
			}
		}
	}

	UE_LOG(LogWaterSystem, Display,
		TEXT("BuildWaterBodies -- done. Total definitions: %d."),
		WaterBodyDefinitions.Num());

	if (bSpawnActors)
	{
		SpawnGeneratedWaterBodies(Heightmap, CellSize);
	}

	return WaterBodyDefinitions.Num() > 0;
}

void UWaterSystemBuilder::SpawnGeneratedWaterBodies(const UHeightmapGenerator* Heightmap, float CellSize)
{
	if (WaterBodyDefinitions.IsEmpty())
	{
		UE_LOG(LogWaterSystem, Warning,
			TEXT("SpawnGeneratedWaterBodies -- skipped because no water definitions were generated yet."));
		return;
	}

	SpawnWaterBodiesRuntime(Heightmap, CellSize);
}

bool UWaterSystemBuilder::IsWaterAt(FVector2D WorldPos, float Radius) const
{
	const float RadiusSq = Radius * Radius;
	for (const FWaterBodyDefinition& Def : WaterBodyDefinitions)
	{
		for (const FVector& SP : Def.SplinePoints)
		{
			const float DX = SP.X - WorldPos.X;
			const float DY = SP.Y - WorldPos.Y;
			if (DX * DX + DY * DY <= RadiusSq)
				return true;
		}
	}
	return false;
}

float UWaterSystemBuilder::GetNearestWaterDistance(FVector2D WorldPos) const
{
	if (WaterBodyDefinitions.IsEmpty())
		return -1.0f;

	float MinDist = FLT_MAX;

	for (const FWaterBodyDefinition& Def : WaterBodyDefinitions)
	{
		if (Def.SplinePoints.IsEmpty()) continue;

		if (Def.WaterType == EWaterBodyType::Lake)
		{
			// Lakes are approximately circular. SplinePoints are perimeter points,
			// so the "interior" of the lake has NO spline point nearby — naively
			// measuring distance to the nearest spline point would return ~LakeRadius
			// for a point at the lake center, wrongly passing the MinWaterDistance filter.
			//
			// Correct approach: compute centroid + average radius, then
			//   dist_to_lake = max(0, dist_to_centroid - avg_radius)
			// A point inside the lake returns 0 (it is IN the water).
			FVector2D Centroid(0.f, 0.f);
			for (const FVector& SP : Def.SplinePoints)
			{
				Centroid.X += SP.X;
				Centroid.Y += SP.Y;
			}
			Centroid /= static_cast<float>(Def.SplinePoints.Num());

			float AvgRadius = 0.f;
			for (const FVector& SP : Def.SplinePoints)
				AvgRadius += FVector2D::Distance(Centroid, FVector2D(SP.X, SP.Y));
			AvgRadius /= static_cast<float>(Def.SplinePoints.Num());

			// 0 if inside or on the edge, positive if outside
			const float DistToEdge = FMath::Max(0.f, FVector2D::Distance(WorldPos, Centroid) - AvgRadius);
			MinDist = FMath::Min(MinDist, DistToEdge);
		}
		else
		{
			// Rivers / Ocean: distance to nearest spline point along the centerline.
			// River Width is accounted for by MinWaterDistance being set >= Width/2.
			for (const FVector& SP : Def.SplinePoints)
			{
				const float DX   = SP.X - WorldPos.X;
				const float DY   = SP.Y - WorldPos.Y;
				const float Dist = FMath::Sqrt(DX * DX + DY * DY);
				MinDist = FMath::Min(MinDist, Dist);
			}
		}
	}

	return (MinDist < FLT_MAX) ? MinDist : -1.0f;
}

// ----------------------------------------------------------------------------
// SpawnWaterBody -- spawns an AWaterBody* actor via reflection
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// SpawnWaterZone -- spawns one dedicated WaterZone for a specific water body
// ----------------------------------------------------------------------------

AActor* UWaterSystemBuilder::SpawnWaterZone(UWorld* World, const FWaterBodyDefinition& Def, float CellSize, int32 ZoneIndex) const
{
	if (!World || Def.SplinePoints.IsEmpty())
	{
		UE_LOG(LogWaterSystem, Warning, TEXT("SpawnWaterZone -- invalid inputs (World or SplinePoints missing)."));
		return nullptr;
	}

	UClass* ZoneClass = UClass::TryFindTypeSlow<UClass>(TEXT("/Script/Water.WaterZone"));
	if (!ZoneClass)
	{
		UE_LOG(LogWaterSystem, Warning, TEXT("SpawnWaterZone -- AWaterZone class not found."));
		return nullptr;
	}

	WaterZoneSpawnUtils::FWaterZoneLayout Layout;
	if (!WaterZoneSpawnUtils::TryBuildWaterZoneLayout(Def, ZoneIndex, CellSize, Layout))
	{
		return nullptr;
	}

	AActor* Zone = WaterZoneSpawnUtils::SpawnZoneActor(World, ZoneClass, Layout.ZoneLocation);

	if (!Zone)
	{
		UE_LOG(LogWaterSystem, Warning, TEXT("SpawnWaterZone -- spawn failed."));
		return nullptr;
	}

	WaterZoneSpawnUtils::MakeActorComponentsMovable(Zone);
	Zone->SetActorLocation(Layout.ZoneLocation);
	WaterZoneSpawnUtils::ApplyZoneExtent(Zone, Layout.ZoneExtent);

	// Configure WaterMesh tiling to cover the full configured zone without center clipping.
	// Keep tile count below the engine cap (256) with some headroom.
	const int32 ConfiguredTileComponents = WaterZoneSpawnUtils::ConfigureWaterMeshTiling(
		Zone,
		ZoneIndex,
		Layout.TargetTilesPerAxis,
		Layout.DesiredTileSize);

	if (ConfiguredTileComponents == 0)
	{
		UE_LOG(LogWaterSystem, Warning,
			TEXT("SpawnWaterZone[%d] -- no component with TileSize found on WaterZone."), ZoneIndex);
	}

	WaterZoneSpawnUtils::ApplyBoundsBoxExtent(Zone, ZoneIndex, Layout.ZoneExtent);

	UE_LOG(LogWaterSystem, Display,
		TEXT("SpawnWaterZone[%d] -- spawned at (%.0f, %.0f, %.0f), extent=(%.0f, %.0f) cm, tiles=%d, tileSize=%.0f cm."),
		ZoneIndex,
		Layout.ZoneLocation.X, Layout.ZoneLocation.Y, Layout.ZoneLocation.Z,
		Layout.ZoneExtent.X, Layout.ZoneExtent.Y,
		Layout.TargetTilesPerAxis, Layout.DesiredTileSize);

	return Zone;
}

// ----------------------------------------------------------------------------
// RebuildWaterZone -- forces the WaterZone to regenerate info meshes
// ----------------------------------------------------------------------------

void UWaterSystemBuilder::RebuildWaterZone(UWorld* World) const
{
	if (!World) return;

	UClass* ZoneClass = UClass::TryFindTypeSlow<UClass>(TEXT("/Script/Water.WaterZone"));
	if (!ZoneClass)
	{
		UE_LOG(LogWaterSystem, Verbose, TEXT("RebuildWaterZone -- WaterZone class not found."));
		return;
	}

	int32 RebuiltZoneCount = 0;
	for (TActorIterator<AActor> It(World, ZoneClass); It; ++It)
	{
		AActor* Zone = *It;

		// In UE5.6, AWaterZone exposes ForceUpdateWaterInfoTexture as a WITH_EDITOR UFUNCTION.
		// This regenerates the WaterInfoMesh / DilatedWaterInfoMesh for all registered water bodies.
		UFunction* RebuildFn = Zone->FindFunction(TEXT("ForceUpdateWaterInfoTexture"));
		if (RebuildFn)
		{
			Zone->ProcessEvent(RebuildFn, nullptr);
			UE_LOG(LogWaterSystem, Display,
				TEXT("RebuildWaterZone -- ForceUpdateWaterInfoTexture called on WaterZone %s."),
				*Zone->GetName());
		}
		else
		{
			// Fallback: rerun construction scripts (less reliable but better than nothing)
#if WITH_EDITOR
			Zone->RerunConstructionScripts();
#endif
			UE_LOG(LogWaterSystem, Warning,
				TEXT("RebuildWaterZone -- ForceUpdateWaterInfoTexture not found on %s, fell back to RerunConstructionScripts."),
				*Zone->GetName());
		}

		++RebuiltZoneCount;
	}

	if (RebuiltZoneCount == 0)
	{
		UE_LOG(LogWaterSystem, Warning, TEXT("RebuildWaterZone -- no WaterZone actors found."));
	}
}

// ----------------------------------------------------------------------------
// SpawnWaterBody -- spawns an AWaterBody* actor via reflection
// ----------------------------------------------------------------------------

AActor* UWaterSystemBuilder::SpawnWaterBody(UWorld* World, const FWaterBodyDefinition& Def, AActor* ZoneActor) const
{
	if (!World || Def.SplinePoints.Num() < 2)
		return nullptr;

	// Prefer Water plugin Blueprint classes because they carry default material setup.
	// Fallback to native classes when BP assets are unavailable.
	const TCHAR* NativeClassName = nullptr;
	const TCHAR* BlueprintClassPath = nullptr;
	switch (Def.WaterType)
	{
		case EWaterBodyType::River:
			NativeClassName = TEXT("/Script/Water.WaterBodyRiver");
			BlueprintClassPath = TEXT("/Water/Blueprints/BP_WaterBodyRiver.BP_WaterBodyRiver_C");
			break;
		case EWaterBodyType::Lake:
			NativeClassName = TEXT("/Script/Water.WaterBodyLake");
			BlueprintClassPath = TEXT("/Water/Blueprints/BP_WaterBodyLake.BP_WaterBodyLake_C");
			break;
		default:
			UE_LOG(LogWaterSystem, Warning, TEXT("SpawnWaterBody -- unsupported WaterType (ocean not generated by pipeline)."));
			return nullptr;
	}

	UClass* WaterClass = nullptr;
	if (BlueprintClassPath)
	{
		WaterClass = LoadClass<AActor>(nullptr, BlueprintClassPath);
		if (WaterClass)
		{
			UE_LOG(LogWaterSystem, Display,
				TEXT("SpawnWaterBody -- resolved BP class: %s"), BlueprintClassPath);
		}
	}

	if (!WaterClass && NativeClassName)
	{
		WaterClass = UClass::TryFindTypeSlow<UClass>(NativeClassName);
		if (WaterClass)
		{
			UE_LOG(LogWaterSystem, Display,
				TEXT("SpawnWaterBody -- using native class fallback: %s"), NativeClassName);
		}
	}

	if (!WaterClass)
	{
		UE_LOG(LogWaterSystem, Warning,
			TEXT("SpawnWaterBody -- class not found. BP='%s' Native='%s'. Is the Water plugin enabled?"),
			BlueprintClassPath ? BlueprintClassPath : TEXT("(null)"),
			NativeClassName ? NativeClassName : TEXT("(null)"));
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transactional;

	// Spawn at the first spline point
	const FVector SpawnLoc = Def.SplinePoints[0];
	AActor* Actor = nullptr;

	#if WITH_EDITOR
	if (GIsEditor
		&& (World->WorldType == EWorldType::Editor || World->WorldType == EWorldType::EditorPreview)
		&& GEditor)
	{
		if (UEditorActorSubsystem* ActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>())
		{
			Actor = ActorSubsystem->SpawnActorFromClass(WaterClass, SpawnLoc, FRotator::ZeroRotator);
			if (Actor)
			{
				UE_LOG(LogWaterSystem, Display,
					TEXT("SpawnWaterBody -- spawned via EditorActorSubsystem (%s)."), *WaterClass->GetPathName());
			}
		}
	}
	#endif

	if (!Actor)
	{
		Actor = World->SpawnActor<AActor>(WaterClass, SpawnLoc, FRotator::ZeroRotator, Params);
	}
	if (!Actor)
	{
		UE_LOG(LogWaterSystem, Warning, TEXT("SpawnWaterBody -- SpawnActor failed for %s."), *WaterClass->GetPathName());
		return nullptr;
	}

	WaterZoneSpawnUtils::MakeActorComponentsMovable(Actor);

	const bool bRuntimeWorld =
		(World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Game || World->WorldType == EWorldType::GamePreview);

	// Set spline points on the water body's spline component
	USplineComponent* Spline = Actor->FindComponentByClass<USplineComponent>();
	if (Spline)
	{
		// Small positive offset so water surface sits on top of the terrain, not inside it
		constexpr float ZOffset = 10.0f;
		FBox2D XYBounds(EForceInit::ForceInit);

		Spline->ClearSplinePoints(false);
		for (const FVector& Pt : Def.SplinePoints)
		{
			Spline->AddSplinePoint(FVector(Pt.X, Pt.Y, Pt.Z + ZOffset), ESplineCoordinateSpace::World, false);
			XYBounds += FVector2D(Pt.X, Pt.Y);
		}

		const bool bClosedLoop = (Def.WaterType == EWaterBodyType::Lake);
		Spline->SetClosedLoop(bClosedLoop, false);
		Spline->UpdateSpline();

		// Apply per-point river widths via UWaterSplineMetadata so the Water plugin
		// renders the river narrow at the source and wide at the mouth.
		if (Def.WaterType == EWaterBodyType::River && !Def.PointWidths.IsEmpty())
		{
			if (UWaterSplineMetadata* WaterMeta = Cast<UWaterSplineMetadata>(Spline->GetSplinePointsMetadata()))
			{
				WaterMeta->RiverWidth.Points.Reset(Def.PointWidths.Num());
				for (int32 i = 0; i < Def.PointWidths.Num(); ++i)
				{
					FInterpCurvePoint<float> Pt;
					Pt.InVal          = static_cast<float>(i);
					Pt.OutVal         = Def.PointWidths[i];
					Pt.ArriveTangent  = 0.f;
					Pt.LeaveTangent   = 0.f;
					Pt.InterpMode     = CIM_Linear;
					WaterMeta->RiverWidth.Points.Add(Pt);
				}
				UE_LOG(LogWaterSystem, Display,
					TEXT("SpawnWaterBody -- applied %d per-point widths (%.0f cm → %.0f cm)."),
					Def.PointWidths.Num(),
					Def.PointWidths[0],
					Def.PointWidths.Last());
			}
			else
			{
				UE_LOG(LogWaterSystem, Warning,
					TEXT("SpawnWaterBody -- UWaterSplineMetadata not found on spline; river width uniform."));
			}
		}

		if (XYBounds.bIsValid)
		{
			UE_LOG(LogWaterSystem, Display,
				TEXT("SpawnWaterBody -- %s XY bounds min=(%.0f, %.0f) max=(%.0f, %.0f)."),
				*WaterClass->GetPathName(),
				XYBounds.Min.X, XYBounds.Min.Y,
				XYBounds.Max.X, XYBounds.Max.Y);
		}
	}

	// Trigger the water body to rebuild its mesh with the updated spline.
	// OnWaterBodyChanged() is the correct entry point: it calls UpdateAll() (WaterZone
	// registration + collision) AND UpdateWaterBodyRenderData() (builds WaterInfoMesh /
	// DilatedWaterInfoMesh from the spline geometry). UpdateAll() alone skips the render
	// data step, which is why the WaterInfoMesh would otherwise remain empty.
	if (UWaterBodyComponent* WaterComp = Actor->FindComponentByClass<UWaterBodyComponent>())
	{
		LinkZoneOnObject(Actor, ZoneActor);
		LinkWaterBodyToZone(WaterComp, ZoneActor);
		LinkZoneOnObject(WaterComp, ZoneActor);

		WaterZoneSpawnUtils::MakeSplineMeshComponentsMovable(Actor);
		ConfigureWaterLandscapeLayer(WaterComp);

		if (Def.WaterType == EWaterBodyType::Lake)
		{
			ApplyLakeFalloffSettings(WaterComp);
		}
		else if (Def.WaterType == EWaterBodyType::River)
		{
			ApplyRiverBlurringRadius(WaterComp, 10.0f);
		}

		FOnWaterBodyChangedParams WaterChangedParams;
		WaterChangedParams.bShapeOrPositionChanged = true;
		WaterChangedParams.bUserTriggered          = true;
		WaterComp->OnWaterBodyChanged(WaterChangedParams);

		UE_LOG(LogWaterSystem, Display,
			TEXT("SpawnWaterBody -- OnWaterBodyChanged called on WaterBodyComponent."));
	}
	else
	{
#if WITH_EDITOR
		Actor->RerunConstructionScripts();
#endif
		UE_LOG(LogWaterSystem, Warning,
			TEXT("SpawnWaterBody -- WaterBodyComponent not found, falling back to RerunConstructionScripts."));
	}

	// Lakes ALWAYS need a second construction pass because:
	//   - WaterBodyLake rendering depends on AWaterZone::WaterInfoTexture
	//   - WaterInfoMeshComponent lives on the zone, NOT on the lake body
	//   - Construction scripts must run a second time with the zone ALREADY linked
	//     so the body can register itself with the zone at OnConstruction time
	//
	// Rivers only need the retry when their WaterInfoMeshComponent has no mesh yet
	// (programmatic spawn occasionally leaves the mesh null).
	auto HasMissingWaterInfoMesh = [Actor]() -> bool
	{
		for (UActorComponent* Comp : Actor->GetComponents())
		{
			UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(Comp);
			if (!StaticMeshComp)
				continue;

			const FName Name = StaticMeshComp->GetFName();
			if ((Name == TEXT("WaterInfoMeshComponent") || Name == TEXT("DilatedWaterInfoMeshComponent"))
				&& StaticMeshComp->GetStaticMesh() == nullptr)
			{
				return true;
			}
		}
		return false;
	};

	const bool bNeedsConstructionRetry =
		(Def.WaterType == EWaterBodyType::Lake) || HasMissingWaterInfoMesh();

	if (bNeedsConstructionRetry)
	{
		// Re-run construction scripts so OnConstruction fires again with the
		// zone override already set → the water body registers itself with the zone.
#if WITH_EDITOR
		Actor->RerunConstructionScripts();
#endif
		WaterZoneSpawnUtils::MakeActorComponentsMovable(Actor);

		if (UWaterBodyComponent* WaterComp = Actor->FindComponentByClass<UWaterBodyComponent>())
		{
			LinkZoneOnObject(Actor, ZoneActor);
			LinkWaterBodyToZone(WaterComp, ZoneActor);
			LinkZoneOnObject(WaterComp, ZoneActor);

			WaterZoneSpawnUtils::MakeSplineMeshComponentsMovable(Actor);
			ConfigureWaterLandscapeLayer(WaterComp);

			if (Def.WaterType == EWaterBodyType::Lake)
			{
				ApplyLakeFalloffSettings(WaterComp);
			}
			else if (Def.WaterType == EWaterBodyType::River)
			{
				ApplyRiverBlurringRadius(WaterComp, 10.0f);
			}

			FOnWaterBodyChangedParams WaterChangedParams;
			WaterChangedParams.bShapeOrPositionChanged = true;
			WaterChangedParams.bUserTriggered          = true;
			WaterComp->OnWaterBodyChanged(WaterChangedParams);
		}

		#if WITH_EDITOR
		if (GIsEditor)
		{
			Actor->PostEditMove(true);
			Actor->PostEditChange();
		}
		#endif

		if (Def.WaterType == EWaterBodyType::Lake)
		{
			UE_LOG(LogWaterSystem, Display,
				TEXT("SpawnWaterBody -- lake construction retry done (zone registration pass)."));
		}
		else
		{
			UE_LOG(LogWaterSystem, Warning,
				TEXT("SpawnWaterBody -- river WaterInfo mesh was missing, forced construction retry."));
		}
	}

	UE_LOG(LogWaterSystem, Verbose,
		TEXT("SpawnWaterBody -- spawned %s at Z=%.1f cm"),
		*Actor->GetActorLocation().ToString(),
		Actor->GetActorLocation().Z);

	return Actor;
}
