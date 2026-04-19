// Copyright Priordium. All Rights Reserved.
//
// FNoiseWrapper.cpp
// Implements FNoiseWrapper methods via FastNoiseLite delegation.

#include "FNoiseWrapper.h"

float FNoiseWrapper::SimplexNoise2D(float X, float Y, int32 Seed)
{
#ifdef WITH_FASTNOISE
	FastNoiseLite Noise;
	ConfigureNoise(Noise, Seed);
	return Noise.GetNoise(X, Y);
#else
	UE_LOG(LogTemp, Error, TEXT("FNoiseWrapper: FastNoiseLite not available. Run DownloadFastNoiseLite.py."));
	return 0.0f;
#endif
}

#ifdef WITH_FASTNOISE
void FNoiseWrapper::ConfigureNoise(FastNoiseLite& OutNoise, int32 Seed)
{
	OutNoise.SetSeed(Seed);
	OutNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
	OutNoise.SetFractalType(FastNoiseLite::FractalType_None);
}
#endif

