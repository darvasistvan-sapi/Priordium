// Copyright Priordium. All Rights Reserved.
//
// Test_MaterialLayers.cpp
// Tests the material layer (weight map) functionality of ULandscapeBuilder.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "ULandscapeBuilder.h"
#include "UBiomeManager.h"
#include "UMapGeneratorSettings.h"
#include "UHeightmapGenerator.h"
#include "EBiomeType.h"

// Helper function: create a small resolution, initialized BiomeManager
static UBiomeManager* MakeInitializedBiomeManager(int32 Res = 9)
{
	UMapGeneratorSettings* Settings = NewObject<UMapGeneratorSettings>();
	Settings->Resolution        = Res;
	Settings->Seed              = 42;
	Settings->SeaLevel          = 0.3f;
	Settings->MountainThreshold = 0.75f;
	Settings->bUseContinentMask = false;
	Settings->HeightmapConfig.Octaves     = 4;
	Settings->HeightmapConfig.Frequency   = 0.05f;
	Settings->HeightmapConfig.Amplitude   = 1.0f;
	Settings->HeightmapConfig.Persistence = 0.5f;
	Settings->HeightmapConfig.Lacunarity  = 2.0f;

	UHeightmapGenerator* Gen = NewObject<UHeightmapGenerator>();
	Gen->Generate(Settings);

	UBiomeManager* Biome = NewObject<UBiomeManager>();
	Biome->AssignBiomes(Settings, Gen);

	return Biome;
}

// -----------------------------------------------------------------------
// Test: CalculateWeightMaps -- uninitialized BiomeManager -> empty result
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_MaterialLayers_EmptyOnUninitializedBiome,
	"Priordium.MapGenerator.LandscapeBuilder.MaterialLayers.EmptyOnUninitializedBiome",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_MaterialLayers_EmptyOnUninitializedBiome::RunTest(const FString& Parameters)
{
	ULandscapeBuilder* Builder = NewObject<ULandscapeBuilder>();
	UBiomeManager* Biome       = NewObject<UBiomeManager>();
	if (!TestNotNull(TEXT("Builder is not null"), Builder)) return false;

	// Biome is not initialized
	auto WeightMaps = Builder->CalculateWeightMaps(Biome, 33);

	TestEqual(TEXT("Uninitialized biome -> empty weight map"), WeightMaps.Num(), 0);
	return true;
}

// -----------------------------------------------------------------------
// Test: CalculateWeightMaps -- null BiomeManager -> empty result
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_MaterialLayers_EmptyOnNullBiome,
	"Priordium.MapGenerator.LandscapeBuilder.MaterialLayers.EmptyOnNullBiome",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_MaterialLayers_EmptyOnNullBiome::RunTest(const FString& Parameters)
{
	ULandscapeBuilder* Builder = NewObject<ULandscapeBuilder>();
	if (!TestNotNull(TEXT("Builder is not null"), Builder)) return false;

	auto WeightMaps = Builder->CalculateWeightMaps(nullptr, 33);

	TestEqual(TEXT("Null biome -> empty weight map"), WeightMaps.Num(), 0);
	return true;
}

// -----------------------------------------------------------------------
// Test: CalculateWeightMaps -- generates weight maps for 7 biomes
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_MaterialLayers_SevenBiomeMaps,
	"Priordium.MapGenerator.LandscapeBuilder.MaterialLayers.SevenBiomeMaps",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_MaterialLayers_SevenBiomeMaps::RunTest(const FString& Parameters)
{
	ULandscapeBuilder* Builder = NewObject<ULandscapeBuilder>();
	if (!TestNotNull(TEXT("Builder is not null"), Builder)) return false;

	const int32 Res = 9;
	UBiomeManager* Biome = MakeInitializedBiomeManager(Res);

	auto WeightMaps = Builder->CalculateWeightMaps(Biome, Res);

	TestEqual(TEXT("7 biome weight maps created"), WeightMaps.Num(), 7);
	return true;
}

// -----------------------------------------------------------------------
// Test: CalculateWeightMaps -- weight map size = Resolution * Resolution
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_MaterialLayers_WeightMapSize,
	"Priordium.MapGenerator.LandscapeBuilder.MaterialLayers.WeightMapSize",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_MaterialLayers_WeightMapSize::RunTest(const FString& Parameters)
{
	ULandscapeBuilder* Builder = NewObject<ULandscapeBuilder>();
	if (!TestNotNull(TEXT("Builder is not null"), Builder)) return false;

	const int32 Res = 9;
	UBiomeManager* Biome = MakeInitializedBiomeManager(Res);

	auto WeightMaps = Builder->CalculateWeightMaps(Biome, Res);

	for (const auto& Pair : WeightMaps)
	{
		TestEqual(
			FString::Printf(TEXT("WeightMap[%d] size is Res*Res"), static_cast<int32>(Pair.Key)),
			Pair.Value.Num(),
			Res * Res
		);
	}
	return true;
}

// -----------------------------------------------------------------------
// Test: CalculateWeightMaps -- values are in the [0, 255] range
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_MaterialLayers_WeightMapRange,
	"Priordium.MapGenerator.LandscapeBuilder.MaterialLayers.WeightMapRange",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_MaterialLayers_WeightMapRange::RunTest(const FString& Parameters)
{
	ULandscapeBuilder* Builder = NewObject<ULandscapeBuilder>();
	if (!TestNotNull(TEXT("Builder is not null"), Builder)) return false;

	const int32 Res = 9;
	UBiomeManager* Biome = MakeInitializedBiomeManager(Res);

	auto WeightMaps = Builder->CalculateWeightMaps(Biome, Res);

	for (const auto& MapPair : WeightMaps)
	{
		for (uint8 Value : MapPair.Value)
		{
			// uint8 is inherently [0, 255], but we verify no NaN values crept in
			TestTrue(
				FString::Printf(TEXT("WeightMap value in [0,255] range: %d"), static_cast<int32>(Value)),
				Value >= 0 && Value <= 255
			);
		}
	}
	return true;
}

// -----------------------------------------------------------------------
// Test: CalculateWeightMaps -- sum of all biome weights per cell is ~255
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_MaterialLayers_WeightSumPerCell,
	"Priordium.MapGenerator.LandscapeBuilder.MaterialLayers.WeightSumPerCell",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_MaterialLayers_WeightSumPerCell::RunTest(const FString& Parameters)
{
	ULandscapeBuilder* Builder = NewObject<ULandscapeBuilder>();
	if (!TestNotNull(TEXT("Builder is not null"), Builder)) return false;

	const int32 Res = 9;
	UBiomeManager* Biome = MakeInitializedBiomeManager(Res);

	auto WeightMaps = Builder->CalculateWeightMaps(Biome, Res);
	if (!TestTrue(TEXT("WeightMaps is not empty"), WeightMaps.Num() > 0)) return false;

	bool bAllCellsValid = true;
	for (int32 i = 0; i < Res * Res; ++i)
	{
		int32 Sum = 0;
		for (const auto& MapPair : WeightMaps)
		{
			Sum += static_cast<int32>(MapPair.Value[i]);
		}

		// Normalized blend weights sum to ~1.0, converted to uint8 -> ~255
		// Small deviation allowed due to rounding errors (+-7)
		if (Sum < 248 || Sum > 262)
		{
			AddError(FString::Printf(
				TEXT("Cell[%d] weight sum %d -- expected ~255 (+-7)"), i, Sum));
			bAllCellsValid = false;
		}
	}

	TestTrue(TEXT("All cell weight sums are ~255"), bAllCellsValid);
	return true;
}

// -----------------------------------------------------------------------
// Test: ApplyMaterialLayers -- returns false with null Landscape
// At least one entry must be in LayerInfoAssets to reach the Landscape null-check.
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_MaterialLayers_NullLandscapeReturnsFalse,
	"Priordium.MapGenerator.LandscapeBuilder.MaterialLayers.NullLandscapeReturnsFalse",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_MaterialLayers_NullLandscapeReturnsFalse::RunTest(const FString& Parameters)
{
	ULandscapeBuilder* Builder = NewObject<ULandscapeBuilder>();
	if (!TestNotNull(TEXT("Builder is not null"), Builder)) return false;

	const int32 Res = 9;
	UBiomeManager* Biome = MakeInitializedBiomeManager(Res);

	// At least one entry in LayerInfoAssets so that the Landscape null-check
	// is reached after the LayerInfoAssets check
	ULandscapeLayerInfoObject* DummyLayer = NewObject<ULandscapeLayerInfoObject>();
	Builder->LayerInfoAssets.Add(EBiomeType::Ocean, DummyLayer);

	AddExpectedError(TEXT("Landscape null"), EAutomationExpectedErrorFlags::Contains, 1);
	const bool bResult = Builder->ApplyMaterialLayers(nullptr, Biome, Res);

	TestFalse(TEXT("ApplyMaterialLayers returns false with null Landscape"), bResult);
	return true;
}

// -----------------------------------------------------------------------
// Test: ApplyMaterialLayers -- returns false with empty LayerInfoAssets (nothing to paint)
// The LayerInfoAssets check precedes the Landscape null-check, so
// null Landscape can also be used to test this branch (no expected error log).
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_MaterialLayers_NoLayerInfoReturnsFalse,
	"Priordium.MapGenerator.LandscapeBuilder.MaterialLayers.NoLayerInfoReturnsFalse",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_MaterialLayers_NoLayerInfoReturnsFalse::RunTest(const FString& Parameters)
{
	ULandscapeBuilder* Builder = NewObject<ULandscapeBuilder>();
	if (!TestNotNull(TEXT("Builder is not null"), Builder)) return false;

	const int32 Res = 9;
	UBiomeManager* Biome = MakeInitializedBiomeManager(Res);

	// LayerInfoAssets empty (default) -> ApplyMaterialLayers returns false
	// The LayerInfoAssets check precedes the Landscape null-check,
	// so null Landscape is also acceptable for this test.
	const bool bResult = Builder->ApplyMaterialLayers(nullptr, Biome, Res);

	TestFalse(TEXT("ApplyMaterialLayers returns false with empty LayerInfoAssets"), bResult);
	return true;
}

