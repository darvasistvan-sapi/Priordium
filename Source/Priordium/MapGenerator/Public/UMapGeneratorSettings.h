// Copyright Priordium. All Rights Reserved.
//
// UMapGeneratorSettings.h
// Central configuration Data Asset for all map generation parameters.
// SOLID: Single Responsibility -- stores exclusively the generation settings.
//        Open/Closed -- new categories can be added without modifying existing code.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MapGeneratorTypes.h"
#include "UMapGeneratorSettings.generated.h"

/**
 * UMapGeneratorSettings
 * Central configuration Data Asset that stores all map generation parameters.
 * Organized into editor-friendly categories, equipped with tooltips, supports deterministic generation.
 *
 * Usage: Right-click in Content Browser -> Miscellaneous -> Data Asset -> UMapGeneratorSettings
 */
UCLASS(BlueprintType)
class PRIORDIUM_API UMapGeneratorSettings : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Returns true if all required generation fields are valid. */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Validation")
	bool IsValidForGeneration() const;

	/**
	 * Validates generation settings and optionally provides a failure reason.
	 * @param OutError Optional output error text when validation fails.
	 */
	bool Validate(FString* OutError = nullptr) const;

	// -------------------------------------------------------------------------
	// General
	// -------------------------------------------------------------------------

	/** Random number generator seed value. Same seed = same map. 0 = random. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "General",
		meta = (ToolTip = "Random seed for deterministic map generation. Same seed always produces the same map. 0 means random seed."))
	int32 Seed = 0;

	/** Map width (X axis) measured in vertices. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "General",
		meta = (ToolTip = "Map width in vertices along the X axis.",
				ClampMin = "64", ClampMax = "4096"))
	int32 MapSizeX = 512;

	/** Map height (Y axis) measured in vertices. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "General",
		meta = (ToolTip = "Map height in vertices along the Y axis.",
				ClampMin = "64", ClampMax = "4096"))
	int32 MapSizeY = 512;

	/**
	 * Heightmap resolution. Recommended to specify in 2^n + 1 format (e.g. 129, 257, 513, 1025).
	 * This is compatible with the Unreal Landscape system.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "General",
		meta = (ToolTip = "Heightmap resolution. Recommended values: 129, 257, 513, 1025 (2^n + 1 format for Landscape compatibility).",
				ClampMin = "33", ClampMax = "8193"))
	int32 Resolution = 513;

	/**
	 * Landscape quad size in centimeters. Determines the distance between vertices.
	 * Typical value: 100 cm (1 meter/vertex).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "General",
		meta = (ToolTip = "Distance between landscape vertices in centimeters.",
				ClampMin = "1", ClampMax = "10000"))
	int32 QuadSize = 100;


	// -------------------------------------------------------------------------
	// Heightmap
	// -------------------------------------------------------------------------

	/** Perlin noise parameters for heightmap generation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heightmap",
		meta = (ToolTip = "Perlin noise configuration for heightmap generation."))
	FHeightmapConfig HeightmapConfig;

	// -------------------------------------------------------------------------
	// -------------------------------------------------------------------------

	/**
	 * Enable continent mask.
	 * If true, a gradual height reduction is applied at map edges,
	 * so the map forms an island-like continent.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Continent Mask",
		meta = (ToolTip = "Enable continent mask: gradually reduces height near map edges to create island-like landmasses."))
	bool bUseContinentMask = true;

	/**
	 * The continent mask falloff distance in normalized [0.0, 0.5] range,
	 * relative to the map size. E.g. 0.3 = height decreases within 30% from map edges.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Continent Mask",
		meta = (ToolTip = "Falloff distance as fraction of map size [0.0, 0.5]. E.g. 0.3 = height fades within 30% of edges.",
				ClampMin = "0.05", ClampMax = "0.5",
				EditCondition = "bUseContinentMask"))
	float EdgeFalloffDistance = 0.3f;

	// -------------------------------------------------------------------------
	// Biomes
	// -------------------------------------------------------------------------

	/** The climate zone that determines biome distribution. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biomes",
		meta = (ToolTip = "Climate zone determining the distribution of biome types across the map."))
	EClimateZone ClimateZone = EClimateZone::Temperate;

	/**
	 * Sea level height in normalized [0.0, 1.0] range.
	 * Below this, water terrain (ocean/lake/river) is generated.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biomes",
		meta = (ToolTip = "Normalized sea level in [0.0, 1.0] range. Terrain below this value becomes water.",
				ClampMin = "0.0", ClampMax = "1.0"))
	float SeaLevel = 0.3f;

	/**
	 * Height threshold for mountainous areas in normalized [0.0, 1.0] range.
	 * Above this, Mountain biome is generated.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biomes",
		meta = (ToolTip = "Normalized height threshold for mountain biome. Terrain above this value becomes Mountain.",
				ClampMin = "0.0", ClampMax = "1.0"))
	float MountainThreshold = 0.75f;

	// -------------------------------------------------------------------------
	// Water
	// -------------------------------------------------------------------------

	/** Enable river generation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water",
		meta = (ToolTip = "Enable procedural river generation on the map."))
	bool bGenerateRivers = true;

	/** Maximum number of rivers generated on the map. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water",
		meta = (ToolTip = "Maximum number of rivers generated on the map.",
				ClampMin = "0", ClampMax = "50",
				EditCondition = "bGenerateRivers"))
	int32 MaxRiverCount = 5;

	/**
	 * Distance in heightmap cells between consecutive river path control points.
	 * Also the radius used for all slope measurements (source scoring and
	 * per-step direction selection in TraceRiverPath).
	 * Larger values produce smoother, more regional slope estimates.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water",
		meta = (ToolTip = "Cell distance between river path points and slope sample radius.",
				ClampMin = "1", ClampMax = "256",
				EditCondition = "bGenerateRivers"))
	int32 RiverSlopeRadius = 40;

	/**
	 * Maximum random angular deviation (degrees) added to the steepest-descent
	 * direction at each river path step. Higher values produce more winding rivers;
	 * 0 = perfectly straight steepest-descent path.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water",
		meta = (ToolTip = "Maximum random angle offset (degrees) from the steepest direction at each river step. 0 = straight, higher = more winding.",
				ClampMin = "0.0", ClampMax = "44.0",
				EditCondition = "bGenerateRivers"))
	float RiverMeanderAngle = 10.0f;

	/** Enable lake generation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water",
		meta = (ToolTip = "Enable procedural lake generation on the map."))
	bool bGenerateLakes = true;

	/** Maximum number of lakes generated on the map. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water",
		meta = (ToolTip = "Maximum number of lakes generated on the map.",
				ClampMin = "0", ClampMax = "100",
				EditCondition = "bGenerateLakes"))
	int32 MaxLakeCount = 10;

	/** Minimum lake radius in heightmap cells. Each lake gets a random radius between Min and Max. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water",
		meta = (ToolTip = "Minimum lake radius in heightmap cells. Each lake is randomised between Min and Max.",
				ClampMin = "2", ClampMax = "200",
				EditCondition = "bGenerateLakes"))
	int32 LakeRadiusMinCells = 10;

	/** Maximum lake radius in heightmap cells. Each lake gets a random radius between Min and Max. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water",
		meta = (ToolTip = "Maximum lake radius in heightmap cells. Each lake is randomised between Min and Max.",
				ClampMin = "2", ClampMax = "200",
				EditCondition = "bGenerateLakes"))
	int32 LakeRadiusMaxCells = 35;

	/**
	 * Controls how irregular (non-circular) lake shores are. [0.0 = perfect circle, 1.0 = maximum irregularity]
	 * Each perimeter point's radius is randomly offset by up to ±(LakeShapeIrregularity * BaseRadius),
	 * then smoothed so the shore stays continuous without sharp spikes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water",
		meta = (ToolTip = "Lake shore irregularity. 0 = perfect circle, 1 = maximum organic shape. Perimeter radii are perturbed then smoothed.",
				ClampMin = "0.0", ClampMax = "1.0",
				EditCondition = "bGenerateLakes"))
	float LakeShapeIrregularity = 0.35f;

	// -------------------------------------------------------------------------
	// Resources
	// -------------------------------------------------------------------------

	/** List of resource spawn rules. Each entry describes the placement configuration of a specific resource type. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resources",
		meta = (ToolTip = "List of resource spawn rules. Each entry defines how a specific resource type is placed on the map."))
	TArray<FResourceSpawnRule> ResourceSpawnRules;
};


