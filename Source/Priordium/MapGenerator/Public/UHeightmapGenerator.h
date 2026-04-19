// Copyright Priordium. All Rights Reserved.
//
// UHeightmapGenerator.h
// ActorComponent that stores and generates heightmap data.
// SOLID: Single Responsibility -- exclusively heightmap storage and generation.
//        Open/Closed -- new generation steps can be added without modifying the existing interface.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UHeightmapGenerator.generated.h"

class UMapGeneratorSettings;
struct FHeightmapConfig;

/**
 * UHeightmapGenerator
 * ActorComponent that:
 */
UCLASS(ClassGroup = (MapGenerator), meta = (BlueprintSpawnableComponent))
class PRIORDIUM_API UHeightmapGenerator : public UActorComponent
{
	GENERATED_BODY()

public:

	UHeightmapGenerator();

	// -------------------------------------------------------------------------
	// -------------------------------------------------------------------------

	/** Initializes the heightmap data array with the given size, every cell is 0.5. */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Heightmap")
	void InitializeHeightmap(int32 SizeX, int32 SizeY);

	/** Returns the normalized [0,1] height value at the given coordinate, or 0.0f if invalid. */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Heightmap")
	float GetHeightAt(int32 X, int32 Y) const;

	/** Returns the full heightmap data array (read-only). */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Heightmap")
	const TArray<float>& GetHeightmapData() const { return HeightmapData; }

	/** Returns the heightmap resolution. */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Heightmap")
	FIntPoint GetResolution() const { return Resolution; }

	/** Returns whether the heightmap is initialized. */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Heightmap")
	bool IsInitialized() const { return HeightmapData.Num() > 0; }

	// -------------------------------------------------------------------------
	// -------------------------------------------------------------------------

	/**
	 * Full heightmap generation pipeline:
	 *   2. Normalization to [0, 1]
	 *
	 * @param Settings  Data Asset containing the generation parameters (cannot be null)
	 * @return          true if successful, false if Settings is null or invalid
	 */
	UFUNCTION(BlueprintCallable, Category = "Map Generator|Heightmap")
	bool Generate(const UMapGeneratorSettings* Settings);

protected:

	virtual void BeginPlay() override;

private:

	/** Heightmap data, normalized [0, 1] float values. Index = Y * Resolution.X + X */
	TArray<float> HeightmapData;

	/** Current resolution of the heightmap */
	FIntPoint Resolution;

	/** Computes the FBM noise value of a single pixel. Return value in [-1, 1]. */
	float GenerateNoiseValue(int32 X, int32 Y, const FHeightmapConfig& Config, int32 Seed) const;

	/** Normalizes all values in the HeightmapData array to the [0, 1] range using min-max. */
	void NormalizeHeightmap();

	/** Smoothstep interpolation: smooth [0,1] transition. Formula: 3t^2 - 2t^3 */
	static float Smoothstep(float T);

	// -------------------------------------------------------------------------
	// -------------------------------------------------------------------------

	/** Smoothstep-based continent mask: pulls height toward 0 at map edges. */
	void ApplyContinentMask(float EdgeFalloffDistance);

	// -------------------------------------------------------------------------
	// -------------------------------------------------------------------------

	/**
	 * Particle-based hydraulic erosion simulation.
	 * In each iteration a "raindrop" travels along the slope,
	 * eroding high areas and depositing sediment in valleys.
	 *
	 * @param Config  The HeightmapConfig erosion parameters (ErosionIterations, ErosionRate, etc.)
	 * @param Seed    Deterministic seed for raindrop spawn positions
	 */
	void ApplyHydraulicErosion(const FHeightmapConfig& Config, int32 Seed);

	/**
	 * Computes the height gradient at the given coordinate (based on neighbors).
	 * @param X, Y   The pixel coordinates
	 * @param OutGradX, OutGradY  The X and Y components of the gradient
	 */
	void GetGradient(float X, float Y, float& OutGradX, float& OutGradY) const;

	/**
	 * Reads out height at a floating-point coordinate using bilinear interpolation.
	 * @param X, Y  Floating-point coordinate
	 * @return      Interpolated height [0, 1]
	 */
	float GetHeightBilinear(float X, float Y) const;

	/**
	 * Modifies the heightmap at the given coordinate using bilinear interpolation with the given weight.
	 * Distributes the delta value across the 4 pixels around (X, Y) using bilinear weights.
	 */
	void AddHeightBilinear(float X, float Y, float Delta);
};


