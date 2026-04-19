// Copyright Priordium. All Rights Reserved.
//
// Test_NoiseWrapper.cpp
// Tests the FNoiseWrapper / FastNoiseLite integration functionality.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Math/UnrealMathUtility.h"

#include "FNoiseWrapper.h"

// -----------------------------------------------------------------------
// Test: SimplexNoise2D returns values in the [-1, 1] range
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_NoiseWrapper_OutputRange,
	"Priordium.MapGenerator.HeightmapGenerator.NoiseWrapper.OutputRange",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_NoiseWrapper_OutputRange::RunTest(const FString& Parameters)
{
	const int32 Seed = 42;
	const int32 SampleCount = 100;

	bool bAllInRange = true;
	for (int32 i = 0; i < SampleCount; ++i)
	{
		float X = static_cast<float>(i) * 13.7f;
		float Y = static_cast<float>(i) * 7.3f;
		float Value = FNoiseWrapper::SimplexNoise2D(X, Y, Seed);

		if (Value < -1.0f || Value > 1.0f)
		{
			AddError(FString::Printf(TEXT("SimplexNoise2D(%.2f, %.2f) = %.4f -- outside [-1, 1] range"), X, Y, Value));
			bAllInRange = false;
		}
	}

	TestTrue(TEXT("All samples are in the [-1, 1] range"), bAllInRange);
	return bAllInRange;
}

// -----------------------------------------------------------------------
// Test: Determinism -- same input produces same output
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_NoiseWrapper_Determinism,
	"Priordium.MapGenerator.HeightmapGenerator.NoiseWrapper.Determinism",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_NoiseWrapper_Determinism::RunTest(const FString& Parameters)
{
	const float X = 123.456f;
	const float Y = 789.012f;
	const int32 Seed = 99;

	float Value1 = FNoiseWrapper::SimplexNoise2D(X, Y, Seed);
	float Value2 = FNoiseWrapper::SimplexNoise2D(X, Y, Seed);
	float Value3 = FNoiseWrapper::SimplexNoise2D(X, Y, Seed);

	TestEqual(TEXT("First and second call match"), Value1, Value2);
	TestEqual(TEXT("Second and third call match"), Value2, Value3);

	return true;
}

// -----------------------------------------------------------------------
// Test: Changing seed changes the output
// Tests multiple coordinate pairs so that at least one pair produces different values.
// On a single coordinate pair, hash collisions can occur (rare but possible).
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_NoiseWrapper_SeedAffectsOutput,
	"Priordium.MapGenerator.HeightmapGenerator.NoiseWrapper.SeedAffectsOutput",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_NoiseWrapper_SeedAffectsOutput::RunTest(const FString& Parameters)
{
	// Test points: seed 1 vs seed 2, and seed 1 vs seed 12345
	// Checked at multiple points -- if at least one differs, seed affects output
	const TArray<TPair<float,float>> TestPoints = {
		{ 50.0f,   50.0f   },
		{ 137.3f,  291.7f  },
		{ 512.0f,  256.0f  },
		{ 1000.0f, 333.0f  },
		{ 77.77f,  88.88f  }
	};

	bool bSeed1VsSeed2Different    = false;
	bool bSeed1VsSeed12345Different = false;

	for (const auto& Pt : TestPoints)
	{
		float V1     = FNoiseWrapper::SimplexNoise2D(Pt.Key, Pt.Value, 1);
		float V2     = FNoiseWrapper::SimplexNoise2D(Pt.Key, Pt.Value, 2);
		float V12345 = FNoiseWrapper::SimplexNoise2D(Pt.Key, Pt.Value, 12345);

		if (!FMath::IsNearlyEqual(V1, V2,     1e-5f)) bSeed1VsSeed2Different     = true;
		if (!FMath::IsNearlyEqual(V1, V12345, 1e-5f)) bSeed1VsSeed12345Different  = true;

		if (bSeed1VsSeed2Different && bSeed1VsSeed12345Different) break;
	}

	TestTrue(TEXT("Seed 1 and Seed 2 produce different values on at least one point"),    bSeed1VsSeed2Different);
	TestTrue(TEXT("Seed 1 and Seed 12345 produce different values on at least one point"), bSeed1VsSeed12345Different);

	return true;
}

// -----------------------------------------------------------------------
// Test: Large coordinate values do not cause a crash
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_NoiseWrapper_LargeCoordinates,
	"Priordium.MapGenerator.HeightmapGenerator.NoiseWrapper.LargeCoordinates",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_NoiseWrapper_LargeCoordinates::RunTest(const FString& Parameters)
{
	const int32 Seed = 0;

	float V1 = FNoiseWrapper::SimplexNoise2D(1e6f,  1e6f,  Seed);
	float V2 = FNoiseWrapper::SimplexNoise2D(-1e6f, -1e6f, Seed);
	float V3 = FNoiseWrapper::SimplexNoise2D(1e4f,  -1e4f, Seed);

	TestTrue(TEXT("Value at 1e6, 1e6 is in [-1,1] range"),   V1 >= -1.0f && V1 <= 1.0f);
	TestTrue(TEXT("Value at -1e6, -1e6 is in [-1,1] range"), V2 >= -1.0f && V2 <= 1.0f);
	TestTrue(TEXT("Value at 1e4, -1e4 is in [-1,1] range"),  V3 >= -1.0f && V3 <= 1.0f);

	return true;
}

// -----------------------------------------------------------------------
// Test: Also returns a value at zero coordinates
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_NoiseWrapper_ZeroCoordinates,
	"Priordium.MapGenerator.HeightmapGenerator.NoiseWrapper.ZeroCoordinates",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_NoiseWrapper_ZeroCoordinates::RunTest(const FString& Parameters)
{
	float Value = FNoiseWrapper::SimplexNoise2D(0.0f, 0.0f, 42);
	TestTrue(TEXT("Value at zero coordinates is in [-1,1] range"), Value >= -1.0f && Value <= 1.0f);
	return true;
}

// -----------------------------------------------------------------------
// Test: Different coordinates produce different values (not a constant function)
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_NoiseWrapper_VariesWithCoords,
	"Priordium.MapGenerator.HeightmapGenerator.NoiseWrapper.VariesWithCoords",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_NoiseWrapper_VariesWithCoords::RunTest(const FString& Parameters)
{
	const int32 Seed = 7;
	const int32 Samples = 20;

	float FirstValue = FNoiseWrapper::SimplexNoise2D(0.0f, 0.0f, Seed);
	bool bFoundDifferent = false;

	for (int32 i = 1; i < Samples; ++i)
	{
		float V = FNoiseWrapper::SimplexNoise2D(static_cast<float>(i) * 100.0f, 0.0f, Seed);
		if (!FMath::IsNearlyEqual(V, FirstValue, 1e-5f))
		{
			bFoundDifferent = true;
			break;
		}
	}

	TestTrue(TEXT("Noise is not constant: different coordinates produce different values"), bFoundDifferent);
	return true;
}

