// Copyright Priordium. All Rights Reserved.
//
// UWaterSystemBuilder.h
// ActorComponent that generates rivers, lakes, and ocean based on the heightmap.
// SOLID: Single Responsibility -- exclusively water body definition generation.
//        Open/Closed -- new water body types can be added without modifying existing logic.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FWaterBodyDefinition.h"
#include "UWaterSystemBuilder.generated.h"

class UHeightmapGenerator;
class UMapGeneratorSettings;
class AActor;

/**
 * UWaterSystemBuilder
 * ActorComponent that:
 *          IsWaterAt()             -- world-space water query
 *          GetNearestWaterDistance() -- nearest water distance query
 */
UCLASS(ClassGroup = (MapGenerator), meta = (BlueprintSpawnableComponent))
class PRIORDIUM_API UWaterSystemBuilder : public UActorComponent
{
	GENERATED_BODY()

public:

	UWaterSystemBuilder();

	// -------------------------------------------------------------------------
	// -------------------------------------------------------------------------

	/**
	 * Finds candidate river source cells from the heightmap.
	 * Sources are local maxima above SeaLevel, selected greedily in height-descending
	 * order so that no two sources are closer than MinRiverSourceDistance cells.
	 * Returns at most Settings->MaxRiverCount sources.
	 */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Water")
	TArray<FIntPoint> FindRiverSources(
		const UHeightmapGenerator* Heightmap,
		const UMapGeneratorSettings* Settings) const;

	// -------------------------------------------------------------------------
	// -------------------------------------------------------------------------

	/**
	 * Traces a river path from a source cell by following the steepest descent.
	 * Stops when the path reaches or crosses sea level, gets stuck at a local
	 * minimum, or exceeds the maximum step budget (ResX * ResY).
	 *
	 * @param Heightmap  Generated heightmap
	 * @param Settings   Generation parameters (SeaLevel)
	 * @param Source     Starting grid cell (should be a local maximum above sea level)
	 * @return           Ordered list of grid cells from source to terminus
	 */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Water")
	TArray<FIntPoint> TraceRiverPath(
		const UHeightmapGenerator* Heightmap,
		const UMapGeneratorSettings* Settings,
		const FIntPoint& Source) const;

	// -------------------------------------------------------------------------
	// -------------------------------------------------------------------------

	/**
	 * Finds candidate lake locations from the heightmap.
	 * Lakes are local minima (all 8 neighbors are higher), selected greedily
	 * by depression depth, up to Settings->MaxLakeCount, with minimum distance.
	 */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Water")
	TArray<FIntPoint> FindLakeLocations(
		const UHeightmapGenerator* Heightmap,
		const UMapGeneratorSettings* Settings) const;

	// -------------------------------------------------------------------------
	// -------------------------------------------------------------------------

	/**
	 * Full water generation pipeline:
	 *   1. FindRiverSources + TraceRiverPath for each river (if bGenerateRivers)
	 *   2. FindLakeLocations for lakes (if bGenerateLakes)
	 * Populates WaterBodyDefinitions. SplinePoints are in world space (XY, Z=0).
	 *
	 * @param Heightmap  Generated heightmap
	 * @param Settings   Generation parameters
	 * @param CellSize   World units per heightmap cell (cm), default 100
	 * @return           true if any definitions were generated
	 */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Water")
	bool BuildWaterBodies(
		const UHeightmapGenerator* Heightmap,
		const UMapGeneratorSettings* Settings,
		float CellSize = 100.0f,
		bool bSpawnActors = true);

	/** Spawns runtime water actors from previously generated definitions. */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Water")
	void SpawnGeneratedWaterBodies(const UHeightmapGenerator* Heightmap, float CellSize = 100.0f);

	/**
	 * Returns true if WorldPos is within Radius of any water body SplinePoint (XY only).
	 * Requires BuildWaterBodies() to have been called first.
	 *
	 * @param WorldPos  World XY position to test
	 * @param Radius    Search radius in world units (cm)
	 */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Water")
	bool IsWaterAt(FVector2D WorldPos, float Radius = 200.0f) const;

	/**
	 * Returns the XY distance to the nearest water body SplinePoint.
	 * Returns -1.0 if no water bodies have been generated.
	 *
	 * @param WorldPos  World XY position to query
	 */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Water")
	float GetNearestWaterDistance(FVector2D WorldPos) const;

	/**
	 * Returns the generated water body definitions.
	 * Populated by BuildWaterBodies().
	 */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Water")
	const TArray<FWaterBodyDefinition>& GetWaterBodyDefinitions() const { return WaterBodyDefinitions; }

	/** Returns whether any water bodies have been generated. */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Water")
	bool IsInitialized() const { return WaterBodyDefinitions.Num() > 0; }

	/** Returns the list of spawned water body actors (only valid during PIE/game). */
	const TArray<TObjectPtr<AActor>>& GetWaterBodies() const { return SpawnedWaterBodies; }

	/** Empties the spawned water body actor list (call after destroying the actors). */
	void ClearSpawnedWaterBodies() { SpawnedWaterBodies.Empty(); SpawnedWaterZones.Empty(); }

	// -------------------------------------------------------------------------
	// Configuration
	// -------------------------------------------------------------------------

	/** Minimum distance between river sources (cells). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generator|Water",
		meta = (ToolTip = "Minimum cell distance between river sources.", ClampMin = "1.0", ClampMax = "256.0"))
	float MinRiverSourceDistance = 8.0f;

	/** Minimum distance between lake locations (cells). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generator|Water",
		meta = (ToolTip = "Minimum cell distance between lake locations.", ClampMin = "1.0", ClampMax = "256.0"))
	float MinLakeDistance = 6.0f;

protected:

	virtual void BeginPlay() override;

private:

	/** Accumulated water body definitions. Populated by BuildWaterBodies(). */
	TArray<FWaterBodyDefinition> WaterBodyDefinitions;

	/** Actors spawned during PIE/game by BuildWaterBodies(). */
	UPROPERTY()
	TArray<TObjectPtr<AActor>> SpawnedWaterBodies;

	/** Dedicated WaterZone actors matched by index to SpawnedWaterBodies. */
	UPROPERTY()
	TArray<TObjectPtr<AActor>> SpawnedWaterZones;

	/** Converts a grid cell to world XY space given map origin and cell size. */
	static FVector2D GridToWorld(const FIntPoint& Cell, const FVector2D& MapOrigin, float CellSize);

	/**
	 * Samples the normalised downhill slope from cell (X, Y) toward
	 * (X + DirX*Radius, Y + DirY*Radius).
	 *
	 * Returns (H_from - H_to) / CellDistance where CellDistance accounts for
	 * diagonal directions (cardinal = Radius, diagonal = Radius * √2).
	 * Positive = downhill. Returns 0.0f if the target is outside the heightmap.
	 *
	 * @param DirX, DirY  Unit direction in {-1, 0, 1} each
	 * @param Radius      Sampling distance in cells
	 */
	static float SampleDirectionalSlope(
		const UHeightmapGenerator* Heightmap,
		int32 X, int32 Y,
		int32 DirX, int32 DirY,
		int32 Radius);

	/**
	 * Appends river definitions and returns generated river count.
	 * OutFlatTermini       receives the (offset) lake centre cell for each river
	 *                      that ended in a flat basin above sea level.
	 * OutFlatTerminiWorldZ receives the world-Z (cm) of the river's actual terminus
	 *                      cell — used so the terminus lake sits at the same height
	 *                      as the river's lowest point.
	 */
	int32 AppendRiverDefinitions(
		const UHeightmapGenerator* Heightmap,
		const UMapGeneratorSettings* Settings,
		const FVector2D& MapOrigin,
		float CellSize,
		TArray<FIntPoint>& OutFlatTermini,
		TArray<float>&    OutFlatTerminiWorldZ);

	/**
	 * Appends lake definitions and returns generated lake count.
	 * ForcedCenters      are created first (river-terminus lakes).
	 * ForcedCenterWorldZ parallel array: world-Z override for each forced centre.
	 *                    If provided, the lake's Z is set to this value instead of
	 *                    being derived from the heightmap at the lake centre cell,
	 *                    ensuring the terminus lake is never higher than its river.
	 * Remaining capacity up to MaxLakeCount is filled from FindLakeLocations().
	 */
	/**
	 * @param bForcedCentersOnly  If true, only ForcedCenters are created;
	 *                            FindLakeLocations() is skipped entirely.
	 *                            Use for the terminus-lake pass so standalone
	 *                            lakes are generated in a separate, later pass.
	 */
	int32 AppendLakeDefinitions(
		const UHeightmapGenerator* Heightmap,
		const UMapGeneratorSettings* Settings,
		const FVector2D& MapOrigin,
		float CellSize,
		FRandomStream& Stream,
		const TArray<FIntPoint>& ForcedCenters,
		const TArray<float>&     ForcedCenterWorldZ,
		bool bForcedCentersOnly = false);

	/** Spawns runtime actors from generated definitions in game worlds. */
	void SpawnWaterBodiesRuntime(const UHeightmapGenerator* Heightmap, float CellSize);

	/**
	 * After all WaterZones are spawned, finds every pair of overlapping zones and
	 * merges them into one (expanding the surviving zone's bounding box to cover both).
	 * Water bodies from the destroyed zone are relinked to the surviving zone.
	 * The process repeats until no overlapping zone pairs remain.
	 */
	void MergeOverlappingWaterZones(UWorld* World);

	/** Spawns a dedicated WaterZone for a specific water body definition. */
	AActor* SpawnWaterZone(UWorld* World, const FWaterBodyDefinition& Def, float CellSize, int32 ZoneIndex) const;

	/**
	 * Forces the WaterZone to rebuild its info meshes after programmatic water body spawning.
	 * Calls AWaterZone::ForceRebuild() via UFUNCTION reflection so the WaterZone generates
	 * WaterInfoMesh / DilatedWaterInfoMesh tiles for all newly spawned water bodies.
	 */
	void RebuildWaterZone(UWorld* World) const;

	/** Spawns a single water body actor from a definition. Returns nullptr on failure. */
	AActor* SpawnWaterBody(UWorld* World, const FWaterBodyDefinition& Def, AActor* ZoneActor) const;
};
