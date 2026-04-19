// Copyright Priordium. All Rights Reserved.
//
// UBiomeManager.h
// ActorComponent that generates a biome map based on the heightmap.
// SOLID: Single Responsibility -- exclusively manages and generates biome data.
//        Open/Closed -- new generation steps can be added without modifying the existing interface.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FBiomeCell.h"
#include "EBiomeType.h"
#include "UBiomeManager.generated.h"

class UMapGeneratorSettings;
class UHeightmapGenerator;

/**
 * UBiomeManager
 * ActorComponent that:
 */
UCLASS(ClassGroup = (MapGenerator), meta = (BlueprintSpawnableComponent))
class PRIORDIUM_API UBiomeManager : public UActorComponent
{
	GENERATED_BODY()

public:

	UBiomeManager();

	// -------------------------------------------------------------------------
	// -------------------------------------------------------------------------

	/**
	 * Initializes the BiomeMap array with the given size.
	 * Every cell receives a default FBiomeCell value.
	 */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Biome")
	void InitializeBiomeMap(int32 SizeX, int32 SizeY);

	/** Returns the FBiomeCell at the given coordinate. */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Biome")
	FBiomeCell GetBiomeAt(int32 X, int32 Y) const;

	/** Returns the EBiomeType value at the given coordinate. */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Biome")
	EBiomeType GetBiomeTypeAt(int32 X, int32 Y) const;

	/** Returns the full BiomeMap data array (read-only). */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Biome")
	const TArray<FBiomeCell>& GetBiomeMap() const { return BiomeMap; }

	/** Returns the BiomeMap resolution. */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Biome")
	FIntPoint GetResolution() const { return Resolution; }

	/** Returns whether the BiomeMap is initialized. */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Biome")
	bool IsInitialized() const { return BiomeMap.Num() > 0; }

	// -------------------------------------------------------------------------
	// -------------------------------------------------------------------------

	/**
	 * Entry point of the full biome generation pipeline.
	 *
	 * @param Settings   Generation parameters (Resolution, Seed, SeaLevel, etc.)
	 * @param Heightmap  The already-generated heightmap component
	 * @return           true if successful, false if any input is null/invalid
	 */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Biome")
	bool AssignBiomes(const UMapGeneratorSettings* Settings, UHeightmapGenerator* Heightmap);

	/**
	 * Returns the temperature value at the given coordinate [0, 1].
	 * 0 = cold (north/mountain), 1 = warm (south/coast).
	 */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Biome")
	float GetTemperatureAt(int32 X, int32 Y) const;

	/** Returns the full TemperatureMap data array (read-only). */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Biome")
	const TArray<float>& GetTemperatureMap() const { return TemperatureMap; }

	// -------------------------------------------------------------------------
	// -------------------------------------------------------------------------

	/**
	 * Returns the moisture value at the given coordinate [0, 1].
	 * 0 = dry, 1 = wet/humid.
	 */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Biome")
	float GetMoistureAt(int32 X, int32 Y) const;

	/** Returns the full MoistureMap data array (read-only). */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Biome")
	const TArray<float>& GetMoistureMap() const { return MoistureMap; }

	// -------------------------------------------------------------------------
	// -------------------------------------------------------------------------

	/**
	 * Returns the BiomeBlendWeights TMap of the cell at the given coordinate.
	 * Returns an empty TMap for invalid coordinates or if not yet calculated
	 * (before AssignBiomes()).
	 */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Biome")
	TMap<EBiomeType, float> GetBlendWeightsAt(int32 X, int32 Y) const;

protected:

	virtual void BeginPlay() override;

private:

	/** Array of biome cells. Index = Y * Resolution.X + X */
	TArray<FBiomeCell> BiomeMap;

	/** Temperature values array [0, 1]. Index = Y * Resolution.X + X */
	TArray<float> TemperatureMap;

	/** Moisture values array [0, 1]. Index = Y * Resolution.X + X */
	TArray<float> MoistureMap;

	/** Resolution of the biome map */
	FIntPoint Resolution;

	/** Bounds check helper function */
	bool IsValidCoord(int32 X, int32 Y) const;

	// -------------------------------------------------------------------------
	// -------------------------------------------------------------------------

	/**
	 * Temperature map generation based on latitude and height.
	 * - BaseTemp = 1.0 - (Y / ResY)   -> warmer in the south
	 * - HeightPenalty = Height * 0.5  -> higher terrain is colder
	 * - Noise variation: 0.1x amplitude
	 * - Normalization to [0, 1] range
	 *
	 * @param Heightmap  Component containing the heightmap data
	 * @param Seed       Deterministic seed for noise variation
	 */
	void GenerateTemperatureMap(UHeightmapGenerator* Heightmap, int32 Seed);

	// -------------------------------------------------------------------------
	// -------------------------------------------------------------------------

	/**
	 * Moisture map generation using Perlin noise and ocean proximity effect.
	 * - Base moisture: SimplexNoise2D (scale 0.01)
	 * - Ocean proximity: higher moisture near coasts
	 * - Normalization to [0, 1] range
	 */
	void GenerateMoistureMap(UHeightmapGenerator* Heightmap, int32 Seed);

	// -------------------------------------------------------------------------
	// -------------------------------------------------------------------------

	/**
	 * Determines the biome type based on Height, Temperature, Moisture.
	 * Decision tree: Ocean -> Beach -> Mountain -> Hills -> Swamp -> Forest -> Plains
	 */
	EBiomeType DetermineBiomeType(float Height, float Temperature, float Moisture, float SeaLevel) const;

	/**
	 * Calculates the BiomeType of all cells and populates the BiomeMap.
	 */
	void CalculateBiomes(const UMapGeneratorSettings* Settings);

	// -------------------------------------------------------------------------
	// -------------------------------------------------------------------------

	/**
	 * Calculates the BiomeBlendWeights of all cells based on neighboring biomes.
	 * BlendRadius = 3: calculates neighbor cell weights based on distance,
	 * then normalizes so that the sum of all weights is ~1.0.
	 * Called by: AssignBiomes() after CalculateBiomes().
	 */
	void CalculateBlendWeights();
};


