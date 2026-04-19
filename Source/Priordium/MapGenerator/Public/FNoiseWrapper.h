// Copyright Priordium. All Rights Reserved.
//
// FNoiseWrapper.h
// Thin wrapper around FastNoiseLite with a UE-compatible API.
// SOLID: Single Responsibility -- exclusively the abstraction of noise generation.
//        Dependency Inversion -- other modules access noise through this interface.

#pragma once

#include "CoreMinimal.h"

#ifdef WITH_FASTNOISE
#include "FastNoiseLite.h"
#endif

/**
 * FNoiseWrapper
 * Stateless wrapper around FastNoiseLite that:
 * - Accepts UE float coordinates
 * - Provides seed management
 * - Returns a value normalized to the [-1, 1] range
 */
class PRIORDIUM_API FNoiseWrapper
{
public:
	/**
	 * 2D OpenSimplex2 noise value at the given position.
	 * @param X      Horizontal coordinate
	 * @param Y      Vertical coordinate
	 * @param Seed   Deterministic seed
	 * @return Noise value in the [-1.0f, 1.0f] range
	 */
	static float SimplexNoise2D(float X, float Y, int32 Seed = 0);

#ifdef WITH_FASTNOISE
	/**
	 * Initializes a FastNoiseLite instance with the given Seed and type.
	 * @param OutNoise  The FastNoiseLite instance to configure (output)
	 * @param Seed      Seed value
	 */
	static void ConfigureNoise(FastNoiseLite& OutNoise, int32 Seed);
#endif
};


