// Copyright Priordium. All Rights Reserved.
//
// UResourceDistributor.h
// ActorComponent that places natural resources on the map based on biome,
// climate zone, height and water proximity.
// SOLID: Single Responsibility -- exclusively resource placement logic.
//        Open/Closed -- new filter types can be added without modifying existing filters.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FResourceSpawnRule.h"
#include "GameplayTagContainer.h"
#include "UResourceDistributor.generated.h"

class UHeightmapGenerator;
class UBiomeManager;
class UClimateZoneManager;
class UWaterSystemBuilder;
class UMapGeneratorSettings;
class ALandscapeProxy;

/**
 * UResourceDistributor
 * ActorComponent that:
 *          CalculatePlacementAreas()  -- list of valid placement cells
 *          CalculateResourceCount()   -- density-based count calculation
 *          GetResourcesInArea()       -- box query with optional tag filter
 *          ClearResources()           -- destroy all spawned actors
 */
UCLASS(ClassGroup = (MapGenerator), meta = (BlueprintSpawnableComponent))
class PRIORDIUM_API UResourceDistributor : public UActorComponent
{
	GENERATED_BODY()

public:

	UResourceDistributor();

	// -------------------------------------------------------------------------
	// -------------------------------------------------------------------------

	/**
	 * Returns true if the given cell properties pass all filters defined in Rule.
	 * Empty AllowedBiomes or AllowedClimateZones means no restriction for that filter.
	 *
	 * @param Rule    Spawn rule to check
	 * @param Biome   Biome type at the candidate cell
	 * @param Climate Climate zone at the candidate cell
	 * @param Height  Normalized height [0, 1] at the candidate cell
	 */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Resources")
	bool CheckFilters(
		const FResourceSpawnRule& Rule,
		EBiomeType  Biome,
		EClimateZone Climate,
		float        Height) const;

	// -------------------------------------------------------------------------
	// -------------------------------------------------------------------------

	/**
	 * Returns true if WorldPos satisfies the water distance constraints in Rule.
	 * If WaterSystem is null or not initialized, the check always passes.
	 *
	 * @param Rule        Spawn rule containing MinWaterDistance / MaxWaterDistance
	 * @param WaterSystem Water system to query; may be null
	 * @param WorldPos    World XY position of the candidate cell
	 */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Resources")
	bool CheckWaterDistanceFilter(
		const FResourceSpawnRule&  Rule,
		const UWaterSystemBuilder* WaterSystem,
		const FVector2D&           WorldPos) const;

	/**
	 * Iterates over all heightmap cells and returns those that pass all filters
	 * (biome, climate, height, water distance) defined in Rule.
	 * Returns an empty array if any required input is null/uninitialized.
	 *
	 * @param Rule          Spawn rule
	 * @param Heightmap     Generated heightmap
	 * @param BiomeMgr      Assigned biome map (may be null)
	 * @param ClimateMgr    Calculated climate zone map (may be null)
	 * @param WaterSystem   Water system for proximity check (may be null)
	 * @param CellSize      World units per heightmap cell
	 */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Resources")
	TArray<FIntPoint> CalculatePlacementAreas(
		const FResourceSpawnRule&   Rule,
		const UHeightmapGenerator*  Heightmap,
		const UBiomeManager*        BiomeMgr,
		const UClimateZoneManager*  ClimateMgr,
		const UWaterSystemBuilder*  WaterSystem,
		float                       CellSize = 100.0f) const;

	// -------------------------------------------------------------------------
	// -------------------------------------------------------------------------

	/**
	 * Spawns a single actor of LoadedClass at Location.
	 * Tags the actor with ResourceTag for later querying.
	 * Adds the actor to SpawnedResources.
	 * Returns nullptr if World or LoadedClass is null.
	 *
	 * @param World        Target world
	 * @param LoadedClass  Already-loaded UClass to spawn
	 * @param ResourceTag  Tag to apply to the spawned actor
	 * @param Location     World-space spawn location
	 */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Resources")
	AActor* SpawnResource(
		UWorld*               World,
		TSubclassOf<AActor>   LoadedClass,
		const FGameplayTag&   ResourceTag,
		const FVector&        Location);

	/**
	 * Returns the number of cluster centers to place for a given rule and valid area size.
	 * Formula: max(1, round(AreaSize * Density * 0.01))
	 */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Resources")
	int32 CalculateResourceCount(const FResourceSpawnRule& Rule, int32 AreaSize) const;

	// -------------------------------------------------------------------------
	// -------------------------------------------------------------------------

	/**
	 * Spawns ClusterSizeMin..ClusterSizeMax actors within ClusterRadius of Center.
	 * The LoadedClasses array is shuffled once per cluster and then cycled through
	 * sequentially, so each class appears at most once before any class repeats.
	 * Each offset position is validated against the water distance filter so that
	 * cluster members never land inside or too close to a water body.
	 * Returns the spawned actors (may be fewer than ClusterSizeMax on filter failures).
	 *
	 * @param World          Target world
	 * @param LoadedClasses  Already-loaded UClasses to pick from (shuffled each call)
	 * @param Rule           Spawn rule (ClusterSizeMin, ClusterSizeMax, ClusterRadius, ResourceTag)
	 * @param Center         World-space cluster center
	 * @param WaterSystem    Water system used to validate offset positions (may be null)
	 */
	TArray<AActor*> SpawnCluster(
		UWorld*                          World,
		TArray<TSubclassOf<AActor>>      LoadedClasses,
		const FResourceSpawnRule&        Rule,
		const FVector&                   Center,
		const UWaterSystemBuilder*       WaterSystem);

	// -------------------------------------------------------------------------
	// -------------------------------------------------------------------------

	/**
	 * Full resource distribution pipeline:
	 *  For each FResourceSpawnRule in Settings->ResourceSpawnRules:
	 *   1. CalculatePlacementAreas()
	 *   2. Load ActorClasses synchronously
	 *   3. SpawnCluster() at CalculateResourceCount() center points
	 * Requires a valid world (GetWorld() must not be null).
	 * Clears previous SpawnedResources before running.
	 *
	 * @param Settings     Generation parameters (ResourceSpawnRules, Seed)
	 * @param Heightmap    Generated heightmap
	 * @param BiomeMgr     Assigned biome map
	 * @param ClimateMgr   Calculated climate zones
	 * @param WaterSystem  Water system for proximity checks
	 * @param CellSize     World units per heightmap cell
	 * @return             true if any actors were spawned
	 */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Resources")
	bool DistributeResources(
		const UMapGeneratorSettings* Settings,
		const UHeightmapGenerator*   Heightmap,
		const UBiomeManager*         BiomeMgr,
		const UClimateZoneManager*   ClimateMgr,
		const UWaterSystemBuilder*   WaterSystem,
		float                        CellSize = 100.0f);

	/**
	 * Returns all valid spawned actors within Area whose ResourceTag matches Tag.
	 * If Tag is not valid, returns all actors inside Area regardless of tag.
	 *
	 * @param Area  World-space bounding box
	 * @param Tag   Optional tag filter; pass FGameplayTag() to match all
	 */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Resources")
	TArray<AActor*> GetResourcesInArea(const FBox& Area, FGameplayTag Tag) const;

	/** Destroys all spawned resource actors and empties SpawnedResources. */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Resources")
	void ClearResources();

	/** Returns true if any resources have been spawned. */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Resources")
	bool IsInitialized() const { return SpawnedResources.Num() > 0; }

	/** Returns the spawned actors array (read-only). */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Resources")
	const TArray<AActor*>& GetSpawnedResources() const { return SpawnedResources; }

protected:

	virtual void BeginPlay() override;

private:

	/** All actors spawned by DistributeResources(). */
	TArray<AActor*> SpawnedResources;

	// --- Filter helpers ---
	bool GetBiomeFilterResult(const FResourceSpawnRule& Rule, EBiomeType Biome) const;
	bool GetClimateFilterResult(const FResourceSpawnRule& Rule, EClimateZone Climate) const;
	bool GetHeightFilterResult(const FResourceSpawnRule& Rule, float Height) const;

	/** Converts a grid cell to world XY space. */
	static FVector2D GridToWorld(const FIntPoint& Cell, const FVector2D& MapOrigin, float CellSize);

	/** Processes a single spawn rule and returns number of spawned actors. */
	int32 ProcessSpawnRule(
		const FResourceSpawnRule& Rule,
		const UHeightmapGenerator* Heightmap,
		const UBiomeManager* BiomeMgr,
		const UClimateZoneManager* ClimateMgr,
		const UWaterSystemBuilder* WaterSystem,
		float CellSize,
		const FVector2D& MapOrigin,
		FRandomStream& Stream,
		UWorld* World,
		ALandscapeProxy* Landscape);
};
