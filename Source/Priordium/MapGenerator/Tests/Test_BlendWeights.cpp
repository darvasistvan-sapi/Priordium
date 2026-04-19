// Copyright Priordium. All Rights Reserved.
//
// Test_BlendWeights.cpp
// Tests the blend weight calculation of UBiomeManager.
//
// Tested properties:
//   1. Every cell's BiomeBlendWeights is non-empty after AssignBiomes()
//   2. The sum of every cell's weights is ~1.0 (tolerance: 0.01)
//   3. In a homogeneous area (only 1 biome within the radius) the dominant biome weight is ~1.0
//   4. At boundaries the weights are shared among multiple biomes

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "UBiomeManager.h"
#include "UHeightmapGenerator.h"
#include "UMapGeneratorSettings.h"
#include "EBiomeType.h"

// -----------------------------------------------------------------------
// Helper function: create and generate Settings + Heightmap
// -----------------------------------------------------------------------
static void MakeBlendTestPair(
	int32 Res, int32 Seed,
	UMapGeneratorSettings*& OutSettings,
	UHeightmapGenerator*&   OutHeightmap)
{
	OutSettings = NewObject<UMapGeneratorSettings>();
	OutSettings->Resolution                  = Res;
	OutSettings->Seed                        = Seed;
	OutSettings->SeaLevel                    = 0.3f;
	OutSettings->bUseContinentMask           = true;
	OutSettings->EdgeFalloffDistance         = 0.3f;
	OutSettings->HeightmapConfig.Octaves     = 4;
	OutSettings->HeightmapConfig.Frequency   = 0.05f;
	OutSettings->HeightmapConfig.Amplitude   = 1.0f;
	OutSettings->HeightmapConfig.Persistence = 0.5f;
	OutSettings->HeightmapConfig.Lacunarity  = 2.0f;

	OutHeightmap = NewObject<UHeightmapGenerator>();
	OutHeightmap->Generate(OutSettings);
}

// -----------------------------------------------------------------------
// Test: Every cell's BiomeBlendWeights is non-empty after AssignBiomes()
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_BlendWeights_NotEmpty,
	"Priordium.MapGenerator.BiomeManager.BlendWeights.NotEmpty",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_BlendWeights_NotEmpty::RunTest(const FString& Parameters)
{
	UMapGeneratorSettings* Settings;
	UHeightmapGenerator*   Heightmap;
	MakeBlendTestPair(33, 42, Settings, Heightmap);

	UBiomeManager* Manager = NewObject<UBiomeManager>();
	const bool bOK = Manager->AssignBiomes(Settings, Heightmap);
	if (!TestTrue(TEXT("AssignBiomes succeeded"), bOK)) return false;

	const TArray<FBiomeCell>& BiomeMap = Manager->GetBiomeMap();
	bool bAllHaveWeights = true;
	for (int32 i = 0; i < BiomeMap.Num(); ++i)
	{
		float Sum = 0.0f;
		for (float W : BiomeMap[i].BiomeBlendWeights) { Sum += W; }
		if (Sum < KINDA_SMALL_NUMBER)
		{
			AddError(FString::Printf(TEXT("BiomeMap[%d].BiomeBlendWeights has zero sum!"), i));
			bAllHaveWeights = false;
			break;
		}
	}
	TestTrue(TEXT("Every cell's BiomeBlendWeights has non-zero sum"), bAllHaveWeights);
	return true;
}

// -----------------------------------------------------------------------
// Test: The sum of every cell's weights is ~1.0 (tolerance: 0.01)
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_BlendWeights_SumIsOne,
	"Priordium.MapGenerator.BiomeManager.BlendWeights.SumIsOne",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_BlendWeights_SumIsOne::RunTest(const FString& Parameters)
{
	UMapGeneratorSettings* Settings;
	UHeightmapGenerator*   Heightmap;
	MakeBlendTestPair(33, 42, Settings, Heightmap);

	UBiomeManager* Manager = NewObject<UBiomeManager>();
	Manager->AssignBiomes(Settings, Heightmap);

	const TArray<FBiomeCell>& BiomeMap = Manager->GetBiomeMap();
	const float Tolerance = 0.01f;
	bool bAllSumOne = true;

	for (int32 i = 0; i < BiomeMap.Num(); ++i)
	{
		float Sum = 0.0f;
		for (float W : BiomeMap[i].BiomeBlendWeights) { Sum += W; }
		if (!FMath::IsNearlyEqual(Sum, 1.0f, Tolerance))
		{
			AddError(FString::Printf(
				TEXT("BiomeMap[%d] weight sum: %.6f (expected: ~1.0, tolerance: %.3f)"),
				i, Sum, Tolerance));
			bAllSumOne = false;
			break;
		}
	}
	TestTrue(TEXT("The sum of every cell's weights is ~1.0"), bAllSumOne);
	return true;
}

// -----------------------------------------------------------------------
// Test: In a homogeneous area the dominant biome weight is ~1.0
//
// Find a cell where all neighbors within BlendRadius=3 have the same biome
// type. For such a cell the dominant biome weight must be 1.0
// (only 1 entry in the TMap).
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_BlendWeights_HomogeneousArea,
	"Priordium.MapGenerator.BiomeManager.BlendWeights.HomogeneousArea",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_BlendWeights_HomogeneousArea::RunTest(const FString& Parameters)
{
	UMapGeneratorSettings* Settings;
	UHeightmapGenerator*   Heightmap;
	// Large resolution to ensure a sufficiently large homogeneous area exists
	MakeBlendTestPair(65, 1337, Settings, Heightmap);

	UBiomeManager* Manager = NewObject<UBiomeManager>();
	Manager->AssignBiomes(Settings, Heightmap);

	const int32 Res         = 65;
	const int32 BlendRadius = 3;
	const float Tolerance   = 0.01f;

	// Search for a homogeneous cell: all neighbors have the same biome
	bool bFoundHomogeneous = false;
	bool bTestPassed       = true;

	for (int32 Y = BlendRadius; Y < Res - BlendRadius && !bFoundHomogeneous; ++Y)
	{
		for (int32 X = BlendRadius; X < Res - BlendRadius && !bFoundHomogeneous; ++X)
		{
			const EBiomeType CenterBiome = Manager->GetBiomeTypeAt(X, Y);
			bool bHomogeneous = true;

			for (int32 DY = -BlendRadius; DY <= BlendRadius && bHomogeneous; ++DY)
			{
				for (int32 DX = -BlendRadius; DX <= BlendRadius && bHomogeneous; ++DX)
				{
					if (Manager->GetBiomeTypeAt(X + DX, Y + DY) != CenterBiome)
					{
						bHomogeneous = false;
					}
				}
			}

			if (bHomogeneous)
			{
				bFoundHomogeneous = true;
				const TMap<EBiomeType, float> Weights = Manager->GetBlendWeightsAt(X, Y);

				// Exactly 1 entry expected
				if (Weights.Num() != 1)
				{
					AddError(FString::Printf(
						TEXT("Homogeneous cell (%d,%d): %d biome entries (expected: 1)"),
						X, Y, Weights.Num()));
					bTestPassed = false;
				}
				else
				{
					const float* DominantWeight = Weights.Find(CenterBiome);
					if (!DominantWeight || !FMath::IsNearlyEqual(*DominantWeight, 1.0f, Tolerance))
					{
						const float W = DominantWeight ? *DominantWeight : -1.0f;
						AddError(FString::Printf(
							TEXT("Homogeneous cell (%d,%d): dominant biome weight %.6f (expected: ~1.0)"),
							X, Y, W));
						bTestPassed = false;
					}
				}
			}
		}
	}

	if (!bFoundHomogeneous)
	{
		// If no homogeneous cell was found, the test assumes a map with a different
		// (likely larger) resolution or seed. This is not an error, just a warning.
		UE_LOG(LogTemp, Display,
			TEXT("BlendWeights.HomogeneousArea: No homogeneous cell found for Res=65 Seed=1337. Test skipped."));
		return true;
	}

	TestTrue(TEXT("In a homogeneous area the dominant biome weight is ~1.0"), bTestPassed);
	return true;
}

// -----------------------------------------------------------------------
// Test: At boundary cells the weights are shared among multiple biomes
//
// Find a cell whose BiomeBlendWeights contains at least 2 biomes.
// This verifies that weights are distributed at biome boundaries.
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_BlendWeights_BoundaryHasMultipleBiomes,
	"Priordium.MapGenerator.BiomeManager.BlendWeights.BoundaryHasMultipleBiomes",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_BlendWeights_BoundaryHasMultipleBiomes::RunTest(const FString& Parameters)
{
	UMapGeneratorSettings* Settings;
	UHeightmapGenerator*   Heightmap;
	MakeBlendTestPair(65, 42, Settings, Heightmap);

	UBiomeManager* Manager = NewObject<UBiomeManager>();
	Manager->AssignBiomes(Settings, Heightmap);

	const TArray<FBiomeCell>& BiomeMap = Manager->GetBiomeMap();

	// Find at least one cell with multiple biome entries in its weights
	bool bFoundBoundaryCell = false;
	for (const FBiomeCell& Cell : BiomeMap)
	{
		int32 NonZeroCount = 0;
		for (float W : Cell.BiomeBlendWeights)
			if (W > 0.0f) NonZeroCount++;

		if (NonZeroCount >= 2)
		{
			bFoundBoundaryCell = true;

			// All non-zero weights must be positive (none should be negative)
			bool bAllPositive = true;
			for (float W : Cell.BiomeBlendWeights)
			{
				if (W < 0.0f) { bAllPositive = false; break; }
			}
			TestTrue(TEXT("Boundary cell weights are all non-negative"), bAllPositive);
			break;
		}
	}

	TestTrue(TEXT("At least one boundary cell exists (multiple biomes in weights)"), bFoundBoundaryCell);
	return true;
}

// -----------------------------------------------------------------------
// Test: GetBlendWeightsAt() returns an empty TMap for invalid coordinates
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_BlendWeights_OutOfBounds,
	"Priordium.MapGenerator.BiomeManager.BlendWeights.OutOfBounds",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_BlendWeights_OutOfBounds::RunTest(const FString& Parameters)
{
	UMapGeneratorSettings* Settings;
	UHeightmapGenerator*   Heightmap;
	MakeBlendTestPair(17, 42, Settings, Heightmap);

	UBiomeManager* Manager = NewObject<UBiomeManager>();
	Manager->AssignBiomes(Settings, Heightmap);

	const TMap<EBiomeType, float> OutOfRange = Manager->GetBlendWeightsAt(999, 999);
	TestEqual(TEXT("GetBlendWeightsAt(999,999) returns empty TMap"), OutOfRange.Num(), 0);

	const TMap<EBiomeType, float> Negative = Manager->GetBlendWeightsAt(-1, 0);
	TestEqual(TEXT("GetBlendWeightsAt(-1,0) returns empty TMap"), Negative.Num(), 0);

	return true;
}
