#include "WaterZoneSpawnUtils.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Engine/World.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Subsystems/EditorActorSubsystem.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWaterZoneSpawnUtils, Log, All);

namespace WaterZoneSpawnUtils
{
	void MakeActorComponentsMovable(AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return;
		}

		if (USceneComponent* Root = Actor->GetRootComponent())
		{
			Root->SetMobility(EComponentMobility::Movable);
		}

		for (UActorComponent* Comp : Actor->GetComponents())
		{
			if (USceneComponent* SceneComp = Cast<USceneComponent>(Comp))
			{
				SceneComp->SetMobility(EComponentMobility::Movable);
			}
		}
	}

	void MakeSplineMeshComponentsMovable(AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return;
		}

		for (UActorComponent* Comp : Actor->GetComponents())
		{
			if (USplineMeshComponent* SplineMeshComp = Cast<USplineMeshComponent>(Comp))
			{
				SplineMeshComp->SetMobility(EComponentMobility::Movable);
			}
		}
	}

	bool TryBuildWaterZoneLayout(
		const FWaterBodyDefinition& Def,
		int32 ZoneIndex,
		float CellSize,
		FWaterZoneLayout& OutLayout)
	{
		// Accumulate the axis-aligned bounding box of all spline points.
		// For lakes these are the perimeter points, so:
		//   XYBounds.Min.X = westernmost point
		//   XYBounds.Max.X = easternmost point
		//   XYBounds.Min.Y = southernmost point
		//   XYBounds.Max.Y = northernmost point
		FBox2D XYBounds(EForceInit::ForceInit);
		for (const FVector& Point : Def.SplinePoints)
		{
			XYBounds += FVector2D(Point.X, Point.Y);
		}

		if (!XYBounds.bIsValid)
		{
			UE_LOG(LogWaterZoneSpawnUtils, Warning,
				TEXT("TryBuildWaterZoneLayout -- invalid spline bounds for zone index %d."), ZoneIndex);
			return false;
		}

		OutLayout.Center   = (XYBounds.Min + XYBounds.Max) * 0.5f;
		OutLayout.HalfSize = (XYBounds.Max - XYBounds.Min) * 0.5f;
		OutLayout.ZoneLocation = FVector(OutLayout.Center.X, OutLayout.Center.Y, 0.0f);

		if (Def.WaterType == EWaterBodyType::Lake)
		{
			// Lakes: compute zone edges directly from the lake's cardinal extremes.
			//
			// Cardinal extremes from the spline bounding box:
			//   West  = XYBounds.Min.X   East  = XYBounds.Max.X
			//   South = XYBounds.Min.Y   North = XYBounds.Max.Y
			//
			// Each edge is then pushed outward by 10% of the lake's dimension in
			// that axis, so the zone is never clipped regardless of lake shape:
			//   ZoneWest  = West  - (East  - West)  * 0.1
			//   ZoneEast  = East  + (East  - West)  * 0.1
			//   ZoneSouth = South - (North - South) * 0.1
			//   ZoneNorth = North + (North - South) * 0.1
			constexpr float EdgePadding = 0.1f;

			const float LakeWidth  = XYBounds.Max.X - XYBounds.Min.X;
			const float LakeHeight = XYBounds.Max.Y - XYBounds.Min.Y;

			const float ZoneWest  = XYBounds.Min.X - LakeWidth  * EdgePadding;
			const float ZoneEast  = XYBounds.Max.X + LakeWidth  * EdgePadding;
			const float ZoneSouth = XYBounds.Min.Y - LakeHeight * EdgePadding;
			const float ZoneNorth = XYBounds.Max.Y + LakeHeight * EdgePadding;

			// WaterZone expects a center position and a half-extent.
			OutLayout.Center     = FVector2D((ZoneWest + ZoneEast)   * 0.5f,
			                                (ZoneSouth + ZoneNorth)  * 0.5f);
			OutLayout.ZoneExtent = FVector2D((ZoneEast  - ZoneWest),
			                                (ZoneNorth - ZoneSouth));
			OutLayout.ZoneLocation = FVector(OutLayout.Center.X, OutLayout.Center.Y, 0.0f);

			UE_LOG(LogWaterZoneSpawnUtils, Display,
				TEXT("TryBuildWaterZoneLayout[%d] (Lake) -- "
				     "lake: W=%.0f E=%.0f S=%.0f N=%.0f  |  "
				     "zone: W=%.0f E=%.0f S=%.0f N=%.0f  |  "
				     "center=(%.0f,%.0f)  extent=(%.0f,%.0f) cm."),
				ZoneIndex,
				XYBounds.Min.X, XYBounds.Max.X, XYBounds.Min.Y, XYBounds.Max.Y,
				ZoneWest, ZoneEast, ZoneSouth, ZoneNorth,
				OutLayout.Center.X, OutLayout.Center.Y,
				OutLayout.ZoneExtent.X, OutLayout.ZoneExtent.Y);
		}
		else
		{
			// Rivers: the spline points are the river CENTRELINE, not the shore.
			// Def.Width stores the maximum (mouth) half-width of the river body, so the
			// actual river surface extends Width/2 beyond every centreline point in all
			// four cardinal directions.  Ignoring this caused the zone boundary to clip
			// the river edges when the river ran near the edge of the spline bounding box.
			//
			// Zone edges:
			//   ZoneWest  = Centreline.Min.X - HalfWidth - PathWidth  * EdgePadding
			//   ZoneEast  = Centreline.Max.X + HalfWidth + PathWidth  * EdgePadding
			//   ZoneSouth = Centreline.Min.Y - HalfWidth - PathHeight * EdgePadding
			//   ZoneNorth = Centreline.Max.Y + HalfWidth + PathHeight * EdgePadding
			constexpr float EdgePadding = 0.1f;

			// HalfWidth: the river is symmetric around the centreline.
			// Def.Width is the mouth (maximum) width in cm.
			const float HalfWidth   = Def.Width * 0.5f;

			const float PathWidth   = XYBounds.Max.X - XYBounds.Min.X;
			const float PathHeight  = XYBounds.Max.Y - XYBounds.Min.Y;

			const float ZoneWest  = XYBounds.Min.X - HalfWidth - PathWidth  * EdgePadding;
			const float ZoneEast  = XYBounds.Max.X + HalfWidth + PathWidth  * EdgePadding;
			const float ZoneSouth = XYBounds.Min.Y - HalfWidth - PathHeight * EdgePadding;
			const float ZoneNorth = XYBounds.Max.Y + HalfWidth + PathHeight * EdgePadding;

			OutLayout.Center     = FVector2D((ZoneWest + ZoneEast)  * 0.5f,
			                                (ZoneSouth + ZoneNorth) * 0.5f);
			OutLayout.ZoneExtent = FVector2D(ZoneEast  - ZoneWest,
			                                ZoneNorth - ZoneSouth);
			OutLayout.ZoneLocation = FVector(OutLayout.Center.X, OutLayout.Center.Y, 0.0f);

			UE_LOG(LogWaterZoneSpawnUtils, Display,
				TEXT("TryBuildWaterZoneLayout[%d] (River) -- "
				     "centreline: W=%.0f E=%.0f S=%.0f N=%.0f  halfWidth=%.0f cm  |  "
				     "zone: W=%.0f E=%.0f S=%.0f N=%.0f  |  "
				     "center=(%.0f,%.0f)  extent=(%.0f,%.0f) cm."),
				ZoneIndex,
				XYBounds.Min.X, XYBounds.Max.X, XYBounds.Min.Y, XYBounds.Max.Y, HalfWidth,
				ZoneWest, ZoneEast, ZoneSouth, ZoneNorth,
				OutLayout.Center.X, OutLayout.Center.Y,
				OutLayout.ZoneExtent.X, OutLayout.ZoneExtent.Y);
		}

		// Tile mesh sizing is shared: cover the full extent with square tiles of at
		// least 800 cm, staying well below the engine cap of 256 tiles per axis.
		OutLayout.TargetTilesPerAxis = FMath::Clamp(
			FMath::CeilToInt(
				(FMath::Max(OutLayout.ZoneExtent.X, OutLayout.ZoneExtent.Y) * 2.0f) / 2400.0f) + 2,
			16,
			240);
		OutLayout.DesiredTileSize = FMath::Max(
			(FMath::Max(OutLayout.ZoneExtent.X, OutLayout.ZoneExtent.Y) * 2.0f)
				/ static_cast<float>(OutLayout.TargetTilesPerAxis),
			800.0f);

		return true;
	}

	AActor* SpawnZoneActor(UWorld* World, UClass* ZoneClass, const FVector& ZoneLocation)
	{
		AActor* Zone = nullptr;

		#if WITH_EDITOR
		if (GIsEditor
			&& (World->WorldType == EWorldType::Editor || World->WorldType == EWorldType::EditorPreview)
			&& GEditor)
		{
			if (UEditorActorSubsystem* ActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>())
			{
				Zone = ActorSubsystem->SpawnActorFromClass(ZoneClass, ZoneLocation, FRotator::ZeroRotator);
			}
		}
		#endif

		if (!Zone)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Zone = World->SpawnActor<AActor>(ZoneClass, ZoneLocation, FRotator::ZeroRotator, Params);
		}

		return Zone;
	}

	void ApplyZoneExtent(AActor* Zone, const FVector2D& ZoneExtent)
	{
		if (FStructProperty* ZoneExtentProp = FindFProperty<FStructProperty>(Zone->GetClass(), TEXT("ZoneExtent")))
		{
			if (ZoneExtentProp->Struct == TBaseStructure<FVector2D>::Get())
			{
				if (FVector2D* ExtentPtr = ZoneExtentProp->ContainerPtrToValuePtr<FVector2D>(Zone))
				{
					*ExtentPtr = ZoneExtent;
					UE_LOG(LogWaterZoneSpawnUtils, Display,
						TEXT("SpawnWaterZone -- ZoneExtent set to (%.0f, %.0f) cm."), ZoneExtent.X, ZoneExtent.Y);
				}
			}
		}
	}

	int32 ConfigureWaterMeshTiling(
		AActor* Zone,
		int32 ZoneIndex,
		int32 TargetTilesPerAxis,
		float DesiredTileSize)
	{
		int32 ConfiguredTileComponents = 0;

		for (UActorComponent* Comp : Zone->GetComponents())
		{
			FProperty* TileSizeProp = Comp->GetClass()->FindPropertyByName(TEXT("TileSize"));
			if (!TileSizeProp)
			{
				continue;
			}

			bool bConfiguredThisComp = false;
			if (float* ValPtr = TileSizeProp->ContainerPtrToValuePtr<float>(Comp))
			{
				*ValPtr = DesiredTileSize;
				bConfiguredThisComp = true;
			}

			if (FProperty* ExtentInTilesProp = Comp->GetClass()->FindPropertyByName(TEXT("ExtentInTiles")))
			{
				if (FStructProperty* ExtentStructProp = CastField<FStructProperty>(ExtentInTilesProp))
				{
					if (ExtentStructProp->Struct == TBaseStructure<FIntPoint>::Get())
					{
						if (FIntPoint* ExtentTilesPtr = ExtentStructProp->ContainerPtrToValuePtr<FIntPoint>(Comp))
						{
							*ExtentTilesPtr = FIntPoint(TargetTilesPerAxis, TargetTilesPerAxis);
							bConfiguredThisComp = true;
						}
					}
				}
				else if (FIntProperty* ExtentIntProp = CastField<FIntProperty>(ExtentInTilesProp))
				{
					if (int32* ExtentTilesPtr = ExtentIntProp->ContainerPtrToValuePtr<int32>(Comp))
					{
						*ExtentTilesPtr = TargetTilesPerAxis;
						bConfiguredThisComp = true;
					}
				}
			}

			if (bConfiguredThisComp)
			{
				++ConfiguredTileComponents;
				UE_LOG(LogWaterZoneSpawnUtils, Display,
					TEXT("SpawnWaterZone[%d] -- configured %s (%s): TileSize=%.0f, TargetTilesPerAxis=%d."),
					ZoneIndex,
					*Comp->GetName(),
					*Comp->GetClass()->GetName(),
					DesiredTileSize,
					TargetTilesPerAxis);
			}
		}

		return ConfiguredTileComponents;
	}

	void ApplyBoundsBoxExtent(AActor* Zone, int32 ZoneIndex, const FVector2D& ZoneExtent)
	{
		for (UActorComponent* Comp : Zone->GetComponents())
		{
			if (UBoxComponent* BoundsBox = Cast<UBoxComponent>(Comp))
			{
				BoundsBox->SetBoxExtent(FVector(ZoneExtent.X, ZoneExtent.Y, 8192.0f), true);
				UE_LOG(LogWaterZoneSpawnUtils, Display,
					TEXT("SpawnWaterZone[%d] -- BoundsComponent.BoxExtent set to (%.0f, %.0f, %.0f) cm."),
					ZoneIndex,
					ZoneExtent.X, ZoneExtent.Y, 8192.0f);
				break;
			}
		}
	}
}
