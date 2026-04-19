// Copyright Priordium. All Rights Reserved.
//
// FBiomeCell.h
// Structure storing the data of a single biome cell.
// SOLID: Single Responsibility -- stores exclusively the biome data of one cell.

#pragma once

#include "CoreMinimal.h"
#include "EBiomeType.h"
#include "FBiomeCell.generated.h"

/**
 * FBiomeCell
 * Contains the biome-relevant data of a single map cell:
 * - Primary biome type
 * - Height, temperature, moisture values [0, 1]
 * - Blend weights for smooth transitions between neighboring biomes
 */
USTRUCT(BlueprintType)
struct PRIORDIUM_API FBiomeCell
{
	GENERATED_BODY()

	/** The primary biome type on this cell. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome",
		meta = (ToolTip = "Primary biome type assigned to this cell."))
	EBiomeType BiomeType = EBiomeType::Plains;

	/** Normalized height [0, 1] -- value received from the HeightmapGenerator. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome",
		meta = (ToolTip = "Normalized height value [0, 1] from the heightmap.",
				ClampMin = "0.0", ClampMax = "1.0"))
	float Height = 0.5f;

	/** Normalized temperature [0, 1] -- 0 = cold (north/mountain), 1 = warm (south/coast). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome",
		meta = (ToolTip = "Normalized temperature [0, 1]. 0 = cold (north/mountain), 1 = hot (south/coast).",
				ClampMin = "0.0", ClampMax = "1.0"))
	float Temperature = 0.5f;

	/** Normalized moisture [0, 1] -- 0 = dry, 1 = wet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome",
		meta = (ToolTip = "Normalized moisture/precipitation [0, 1]. 0 = dry, 1 = wet.",
				ClampMin = "0.0", ClampMax = "1.0"))
	float Moisture = 0.5f;

	/**
	 * Biome blend weights for smooth transitions between neighboring biomes.
	 * Index = static_cast<int32>(EBiomeType). The sum of all weights is ~1.0.
	 * Fixed array (9 entries = EBiomeType count) eliminates per-cell heap allocations.
	 * Note: static arrays are not Blueprint-accessible; use UBiomeManager::GetBlendWeightsAt().
	 */
	static constexpr int32 BiomeTypeCount = 9;

	UPROPERTY(EditAnywhere, Category = "Biome",
		meta = (ToolTip = "Blend weights for neighboring biomes. Indexed by EBiomeType. Sum ~1.0."))
	float BiomeBlendWeights[9] = {};
};


