// Copyright Priordium. All Rights Reserved.
//
// UResourceDistributor.cpp
// Resource placement implementation.

#include "UResourceDistributor.h"
#include "UHeightmapGenerator.h"
#include "UBiomeManager.h"
#include "UClimateZoneManager.h"
#include "UWaterSystemBuilder.h"
#include "UMapGeneratorSettings.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "LandscapeProxy.h"

DEFINE_LOG_CATEGORY_STATIC(LogResourceDistributor, Log, All);

// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------

UResourceDistributor::UResourceDistributor()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UResourceDistributor::BeginPlay()
{
	Super::BeginPlay();
}

// ----------------------------------------------------------------------------
// Private helper
// ----------------------------------------------------------------------------

FVector2D UResourceDistributor::GridToWorld(const FIntPoint& Cell, const FVector2D& MapOrigin, float CellSize)
{
	return FVector2D(
		MapOrigin.X + Cell.X * CellSize,
		MapOrigin.Y + Cell.Y * CellSize);
}

bool UResourceDistributor::GetBiomeFilterResult(const FResourceSpawnRule& Rule, EBiomeType Biome) const
{
	// Empty list = all biomes allowed
	if (Rule.AllowedBiomes.IsEmpty()) return true;
	return Rule.AllowedBiomes.Contains(Biome);
}

bool UResourceDistributor::GetClimateFilterResult(const FResourceSpawnRule& Rule, EClimateZone Climate) const
{
	if (Rule.AllowedClimateZones.IsEmpty()) return true;
	return Rule.AllowedClimateZones.Contains(Climate);
}

bool UResourceDistributor::GetHeightFilterResult(const FResourceSpawnRule& Rule, float Height) const
{
	return Height >= Rule.MinHeight && Height <= Rule.MaxHeight;
}

bool UResourceDistributor::CheckFilters(
	const FResourceSpawnRule& Rule,
	EBiomeType   Biome,
	EClimateZone Climate,
	float        Height) const
{
	if (!GetBiomeFilterResult(Rule, Biome))   return false;
	if (!GetClimateFilterResult(Rule, Climate)) return false;
	if (!GetHeightFilterResult(Rule, Height)) return false;
	return true;
}

bool UResourceDistributor::CheckWaterDistanceFilter(
	const FResourceSpawnRule&  Rule,
	const UWaterSystemBuilder* WaterSystem,
	const FVector2D&           WorldPos) const
{
	// No water system or not initialized: always pass
	if (!WaterSystem || !WaterSystem->IsInitialized()) return true;

	const float Dist = WaterSystem->GetNearestWaterDistance(WorldPos);
	if (Dist < 0.0f) return true; // No water bodies

	// Minimum distance check
	if (Rule.MinWaterDistance > 0.0f && Dist < Rule.MinWaterDistance) return false;

	// Maximum distance check (negative = no maximum)
	if (Rule.MaxWaterDistance >= 0.0f && Dist > Rule.MaxWaterDistance) return false;

	return true;
}

TArray<FIntPoint> UResourceDistributor::CalculatePlacementAreas(
	const FResourceSpawnRule&   Rule,
	const UHeightmapGenerator*  Heightmap,
	const UBiomeManager*        BiomeMgr,
	const UClimateZoneManager*  ClimateMgr,
	const UWaterSystemBuilder*  WaterSystem,
	float                       CellSize) const
{
	TArray<FIntPoint> ValidCells;

	if (!Heightmap || !Heightmap->IsInitialized())
	{
		UE_LOG(LogResourceDistributor, Warning,
			TEXT("CalculatePlacementAreas -- Heightmap is null or uninitialized."));
		return ValidCells;
	}

	if (!BiomeMgr || !BiomeMgr->IsInitialized())
	{
		UE_LOG(LogResourceDistributor, Warning,
			TEXT("CalculatePlacementAreas -- BiomeManager is null or uninitialized."));
		return ValidCells;
	}

	const FIntPoint Res = Heightmap->GetResolution();
	const FVector2D MapOrigin(
		-(Res.X - 1) * CellSize * 0.5f,
		-(Res.Y - 1) * CellSize * 0.5f);

	for (int32 Y = 0; Y < Res.Y; ++Y)
	{
		for (int32 X = 0; X < Res.X; ++X)
		{
			const float        Height  = Heightmap->GetHeightAt(X, Y);
			const EBiomeType   Biome   = BiomeMgr->GetBiomeTypeAt(X, Y);

			EClimateZone Climate = EClimateZone::Temperate;
			if (ClimateMgr && ClimateMgr->IsInitialized())
			{
				const int32 Idx = Y * Res.X + X;
				Climate = ClimateMgr->GetClimateZoneAt(Idx);
			}

			if (!CheckFilters(Rule, Biome, Climate, Height)) continue;

			const FVector2D WorldPos = GridToWorld(FIntPoint(X, Y), MapOrigin, CellSize);
			if (!CheckWaterDistanceFilter(Rule, WaterSystem, WorldPos)) continue;

			ValidCells.Add(FIntPoint(X, Y));
		}
	}

	UE_LOG(LogResourceDistributor, Verbose,
		TEXT("CalculatePlacementAreas -- %d valid cells (res %dx%d)."),
		ValidCells.Num(), Res.X, Res.Y);

	return ValidCells;
}

AActor* UResourceDistributor::SpawnResource(
	UWorld*              World,
	TSubclassOf<AActor>  LoadedClass,
	const FGameplayTag&  ResourceTag,
	const FVector&       Location)
{
	if (!World)
	{
		UE_LOG(LogResourceDistributor, Warning,
			TEXT("SpawnResource -- World is null."));
		return nullptr;
	}

	if (!LoadedClass)
	{
		UE_LOG(LogResourceDistributor, Warning,
			TEXT("SpawnResource -- LoadedClass is null."));
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* Actor = World->SpawnActor<AActor>(LoadedClass, FTransform(Location), Params);
	if (Actor)
	{
		// Marker tag used by ClearResources fallback to find all generated resources
		// even after a Hot Reload or editor restart (when SpawnedResources array is lost).
		Actor->Tags.Add(TEXT("MapGen_Resource"));

		if (ResourceTag.IsValid())
			Actor->Tags.Add(ResourceTag.GetTagName());

#if WITH_EDITOR
		// Group all generated resources under a "Resources" folder in the World Outliner.
		Actor->SetFolderPath(FName("Resources"));
#endif

		SpawnedResources.Add(Actor);
	}

	return Actor;
}

int32 UResourceDistributor::CalculateResourceCount(const FResourceSpawnRule& Rule, int32 AreaSize) const
{
	if (AreaSize <= 0) return 0;
	return FMath::Max(1, FMath::RoundToInt(static_cast<float>(AreaSize) * Rule.Density * 0.01f));
}

TArray<AActor*> UResourceDistributor::SpawnCluster(
	UWorld*                     World,
	TArray<TSubclassOf<AActor>> LoadedClasses,
	const FResourceSpawnRule&   Rule,
	const FVector&              Center,
	const UWaterSystemBuilder*  WaterSystem)
{
	TArray<AActor*> Cluster;

	if (!World || LoadedClasses.IsEmpty()) return Cluster;

	// Shuffle the class list once per cluster so that within a single cluster
	// every class appears at most once before any class repeats.
	// The shuffle uses FMath::FRand() which is seeded globally per map generation.
	const int32 NumClasses = LoadedClasses.Num();
	for (int32 i = NumClasses - 1; i > 0; --i)
	{
		const int32 j = FMath::RandRange(0, i);
		LoadedClasses.Swap(i, j);
	}

	const int32 Count = FMath::RandRange(Rule.ClusterSizeMin, Rule.ClusterSizeMax);
	for (int32 i = 0; i < Count; ++i)
	{
		// Cycle through the shuffled class list: index wraps only after all classes
		// have been used once, ensuring no same-mesh repetition within the first pass.
		TSubclassOf<AActor> ClassToSpawn = LoadedClasses[i % NumClasses];
		if (!ClassToSpawn) continue;

		const float Angle  = FMath::FRandRange(0.0f, 2.0f * PI);
		const float Dist   = FMath::FRandRange(0.0f, Rule.ClusterRadius);
		const FVector Offset(Dist * FMath::Cos(Angle), Dist * FMath::Sin(Angle), 0.0f);
		const FVector SpawnPos = Center + Offset;

		// Validate the offset position: cluster members must also satisfy the
		// water distance filter, otherwise they could land inside a lake/river.
		if (!CheckWaterDistanceFilter(Rule, WaterSystem, FVector2D(SpawnPos.X, SpawnPos.Y)))
			continue;

		AActor* Actor = SpawnResource(World, ClassToSpawn, Rule.ResourceTag, SpawnPos);
		if (Actor) Cluster.Add(Actor);
	}

	return Cluster;
}

int32 UResourceDistributor::ProcessSpawnRule(
	const FResourceSpawnRule& Rule,
	const UHeightmapGenerator* Heightmap,
	const UBiomeManager* BiomeMgr,
	const UClimateZoneManager* ClimateMgr,
	const UWaterSystemBuilder* WaterSystem,
	float CellSize,
	const FVector2D& MapOrigin,
	FRandomStream& Stream,
	UWorld* World,
	ALandscapeProxy* Landscape)
{
	if (!Rule.ResourceTag.IsValid())
	{
		UE_LOG(LogResourceDistributor, Verbose,
			TEXT("DistributeResources -- Skipping rule with invalid ResourceTag."));
		return 0;
	}

	TArray<FIntPoint> PlacementCells = CalculatePlacementAreas(
		Rule, Heightmap, BiomeMgr, ClimateMgr, WaterSystem, CellSize);

	if (PlacementCells.IsEmpty())
		return 0;

	// Load all actor classes for this rule. Null/empty soft references are skipped.
	TArray<TSubclassOf<AActor>> LoadedClasses;
	for (const TSoftClassPtr<AActor>& SoftRef : Rule.ActorClasses)
	{
		if (SoftRef.IsNull()) continue;
		UClass* Cls = SoftRef.LoadSynchronous();
		if (Cls) LoadedClasses.Add(Cls);
	}

	if (LoadedClasses.IsEmpty())
	{
		UE_LOG(LogResourceDistributor, Verbose,
			TEXT("DistributeResources -- No valid ActorClasses for tag '%s', skipping."),
			*Rule.ResourceTag.ToString());
		return 0;
	}

	// Fisher-Yates shuffle for deterministic, varied placement
	for (int32 i = PlacementCells.Num() - 1; i > 0; --i)
	{
		const int32 j = Stream.RandRange(0, i);
		PlacementCells.Swap(i, j);
	}

	const int32 ClusterCount = CalculateResourceCount(Rule, PlacementCells.Num());
	int32 SpawnedCount = 0;

	for (int32 i = 0; i < ClusterCount && i < PlacementCells.Num(); ++i)
	{
		const FIntPoint& Cell = PlacementCells[i];
		const FVector2D  WP   = GridToWorld(Cell, MapOrigin, CellSize);

		// Determine the actual terrain Z by querying the landscape heightfield directly.
		// EHeightfieldSource::Simple samples the heightfield data without a physics trace,
		// so it cannot be misled by sky sphere or atmosphere actors.
		// EHeightfieldSource::Complex (the default) internally performs an ECC_WorldStatic
		// line trace which hits high-altitude scene objects before reaching the landscape.
		// Fallback to the heightmap formula if the landscape query misses (e.g. the
		// cell is outside the landscape bounds).
		float WorldZ = 0.f;
		{
			bool bGotHeight = false;
			if (Landscape)
			{
				const TOptional<float> Height =
					Landscape->GetHeightAtLocation(FVector(WP.X, WP.Y, 0.f), EHeightfieldSource::Simple);
				if (Height.IsSet())
				{
					WorldZ     = Height.GetValue();
					bGotHeight = true;
				}
			}
			if (!bGotHeight)
			{
				// Fallback: derive from heightmap (mirrors ULandscapeBuilder formula)
				const float NormH = Heightmap->GetHeightAt(Cell.X, Cell.Y);
				WorldZ = (NormH * 32767.5f - 16384.0f) * (50.0f / 128.0f);
			}
		}
		const FVector Center(WP.X, WP.Y, WorldZ);

		TArray<AActor*> Cluster = SpawnCluster(World, LoadedClasses, Rule, Center, WaterSystem);
		SpawnedCount += Cluster.Num();
	}

	UE_LOG(LogResourceDistributor, Verbose,
		TEXT("DistributeResources -- Tag '%s': %d clusters across %d valid cells."),
		*Rule.ResourceTag.ToString(), ClusterCount, PlacementCells.Num());

	return SpawnedCount;
}

bool UResourceDistributor::DistributeResources(
	const UMapGeneratorSettings* Settings,
	const UHeightmapGenerator*   Heightmap,
	const UBiomeManager*         BiomeMgr,
	const UClimateZoneManager*   ClimateMgr,
	const UWaterSystemBuilder*   WaterSystem,
	float                        CellSize)
{
	ClearResources();

	if (!Settings)
	{
		UE_LOG(LogResourceDistributor, Warning,
			TEXT("DistributeResources -- Settings is null."));
		return false;
	}

	if (!Heightmap || !Heightmap->IsInitialized())
	{
		UE_LOG(LogResourceDistributor, Warning,
			TEXT("DistributeResources -- Heightmap is null or uninitialized."));
		return false;
	}

	if (!BiomeMgr || !BiomeMgr->IsInitialized())
	{
		UE_LOG(LogResourceDistributor, Warning,
			TEXT("DistributeResources -- BiomeManager is null or uninitialized."));
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogResourceDistributor, Warning,
			TEXT("DistributeResources -- GetWorld() returned null (component not attached to actor in world?)."));
		return false;
	}
	if (Settings->ResourceSpawnRules.IsEmpty())
	{
		UE_LOG(LogResourceDistributor, Display,
			TEXT("DistributeResources -- ResourceSpawnRules is empty, nothing to distribute."));
		return false;
	}

	const FIntPoint Res = Heightmap->GetResolution();
	const FVector2D MapOrigin(
		-(Res.X - 1) * CellSize * 0.5f,
		-(Res.Y - 1) * CellSize * 0.5f);

	// Find the landscape actor once so every ProcessSpawnRule call can use it for
	// accurate Z lookup via GetHeightAtLocation (bypasses sky/atmosphere colliders).
	ALandscapeProxy* Landscape = nullptr;
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		Landscape = *It;
		break;
	}
	if (!Landscape)
	{
		UE_LOG(LogResourceDistributor, Warning,
			TEXT("DistributeResources -- No ALandscapeProxy found in world; Z will use heightmap fallback."));
	}

	const int32 SeedValue = Settings->Seed != 0 ? Settings->Seed : FMath::Rand();
	FRandomStream Stream(SeedValue);

	int32 TotalSpawned = 0;

	for (const FResourceSpawnRule& Rule : Settings->ResourceSpawnRules)
	{
		TotalSpawned += ProcessSpawnRule(
			Rule,
			Heightmap,
			BiomeMgr,
			ClimateMgr,
			WaterSystem,
			CellSize,
			MapOrigin,
			Stream,
			World,
			Landscape);
	}

	UE_LOG(LogResourceDistributor, Display,
		TEXT("DistributeResources -- done. Total spawned: %d."), TotalSpawned);

	return SpawnedResources.Num() > 0;
}

TArray<AActor*> UResourceDistributor::GetResourcesInArea(const FBox& Area, FGameplayTag Tag) const
{
	TArray<AActor*> Result;
	for (AActor* Actor : SpawnedResources)
	{
		if (!IsValid(Actor)) continue;
		if (!Area.IsInsideOrOn(Actor->GetActorLocation())) continue;
		if (Tag.IsValid() && !Actor->Tags.Contains(Tag.GetTagName())) continue;
		Result.Add(Actor);
	}
	return Result;
}

void UResourceDistributor::ClearResources()
{
	// Destroy tracked resources
	int32 Destroyed = 0;
	for (AActor* Actor : SpawnedResources)
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
			++Destroyed;
		}
	}
	SpawnedResources.Empty();

	// Fallback: destroy any remaining world actors tagged "MapGen_Resource".
	// Needed after Hot Reload or editor restart when SpawnedResources is empty
	// but previously generated actors are still in the world.
	UWorld* World = GetWorld();
	if (World)
	{
		TArray<AActor*> Tagged;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->Tags.Contains(TEXT("MapGen_Resource")))
				Tagged.Add(*It);
		}
		for (AActor* Actor : Tagged)
		{
			if (IsValid(Actor))
			{
				Actor->Destroy();
				++Destroyed;
			}
		}
	}

	UE_LOG(LogResourceDistributor, Display, TEXT("ClearResources -- %d resource actor(s) destroyed."), Destroyed);
}
