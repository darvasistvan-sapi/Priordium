// Copyright Priordium. All Rights Reserved.
//
// FHeightmapConfig.h
// Configuration structure for heightmap generation.
// SOLID: Single Responsibility -- only this one structure is in this file.

#pragma once

#include "CoreMinimal.h"
#include "FHeightmapConfig.generated.h"

/**
 * FHeightmapConfig
 * Stores the Perlin noise parameters and erosion settings for heightmap generation.
 * All fields are equipped with editor-friendly ClampMin/ClampMax meta specifiers.
 */
USTRUCT(BlueprintType)
struct PRIORDIUM_API FHeightmapConfig
{
	GENERATED_BODY()

	// -------------------------------------------------------------------------
	// -------------------------------------------------------------------------

	/** The noise frequency. Lower value = larger, smoother terrain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heightmap",
		meta = (ToolTip = "Noise frequency. Lower values produce larger, smoother terrain features.",
				ClampMin = "0.0001", ClampMax = "1.0"))
	float Frequency = 0.01f;

	/** The noise amplitude, scaler of maximum height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heightmap",
		meta = (ToolTip = "Noise amplitude. Controls the maximum height scale.",
				ClampMin = "0.0", ClampMax = "10.0"))
	float Amplitude = 1.0f;

	/** Number of octaves. Higher value = more detailed terrain, but slower generation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heightmap",
		meta = (ToolTip = "Number of noise octaves. Higher values add more terrain detail but are slower.",
				ClampMin = "1", ClampMax = "10"))
	int32 Octaves = 6;

	/** Persistence: the reduction ratio of each octave's amplitude. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heightmap",
		meta = (ToolTip = "Persistence controls how much each octave contributes relative to the previous.",
				ClampMin = "0.0", ClampMax = "1.0"))
	float Persistence = 0.5f;

	/** Lacunarity: the growth ratio of each octave's frequency. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heightmap",
		meta = (ToolTip = "Lacunarity controls how frequency increases with each octave.",
				ClampMin = "1.0", ClampMax = "4.0"))
	float Lacunarity = 2.0f;

	// -------------------------------------------------------------------------
	// -------------------------------------------------------------------------

	/**
	 * Enable hydraulic erosion.
	 * If true, after generation a particle-based erosion simulation runs,
	 * which creates more natural-looking valleys and riverbeds.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Erosion",
		meta = (ToolTip = "Enable hydraulic erosion simulation after FBM generation. Produces more natural-looking valleys and riverbeds."))
	bool bEnableErosion = false;

	/**
	 * Number of raindrop (particle) iterations run in the erosion simulation.
	 * More iterations = deeper valleys, longer generation time.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Erosion",
		meta = (ToolTip = "Number of raindrop simulation iterations. More iterations create deeper valleys but take longer.",
				ClampMin = "100", ClampMax = "200000",
				EditCondition = "bEnableErosion"))
	int32 ErosionIterations = 50000;

	/**
	 * Erosion rate: how much sediment the drop picks up from the ground.
	 * Higher value = more intense, faster erosion.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Erosion",
		meta = (ToolTip = "Erosion rate: how much sediment each raindrop picks up per step.",
				ClampMin = "0.001", ClampMax = "1.0",
				EditCondition = "bEnableErosion"))
	float ErosionRate = 0.3f;

	/**
	 * Deposition rate: how much sediment the drop deposits when it slows down.
	 * Lower value = sediment is transported farther.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Erosion",
		meta = (ToolTip = "Deposition rate: how much sediment each raindrop deposits when slowing down.",
				ClampMin = "0.001", ClampMax = "1.0",
				EditCondition = "bEnableErosion"))
	float DepositionRate = 0.3f;

	/**
	 * The drop's evaporation rate per step.
	 * When the drop loses its water, it deposits the remaining sediment.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Erosion",
		meta = (ToolTip = "Evaporation rate per simulation step. Drops evaporate and deposit remaining sediment.",
				ClampMin = "0.001", ClampMax = "0.1",
				EditCondition = "bEnableErosion"))
	float EvaporationRate = 0.02f;
};


