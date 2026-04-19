// Copyright Priordium. All Rights Reserved.
//
// Test_PlacementAreas.cpp
// Tests CalculatePlacementAreas() of UResourceDistributor.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UResourceDistributor.h"
#include "UHeightmapGenerator.h"
#include "UBiomeManager.h"
#include "UMapGeneratorSettings.h"
#include "FResourceSpawnRule.h"

// Helper: build Settings + Heightmap + BiomeManager
static void MakeHeightmapAndBiomes(
	int32 Res, int32 Seed,
	UMapGeneratorSettings*& OutSettings,
	UHeightmapGenerator*&   OutGen,
	UBiomeManager*&         OutBiomes)
{
	OutSettings = NewObject<UMapGeneratorSettings>(GetTransientPackage(), NAME_None, RF_Transient);
	OutSettings->Resolution = Res; OutSettings->Seed = Seed;
	OutSettings->bUseContinentMask = true;
	OutSettings->EdgeFalloffDistance = 0.3f;
	OutSettings->HeightmapConfig.Octaves = 4;
	OutSettings->HeightmapConfig.Frequency = 0.05f;
	OutSettings->HeightmapConfig.Amplitude = 1.0f;
	OutSettings->HeightmapConfig.Persistence = 0.5f;
	OutSettings->HeightmapConfig.Lacunarity = 2.0f;
	OutSettings->SeaLevel = 0.3f;

	OutGen = NewObject<UHeightmapGenerator>(GetTransientPackage(), NAME_None, RF_Transient);
	OutGen->Generate(OutSettings);

	OutBiomes = NewObject<UBiomeManager>(GetTransientPackage(), NAME_None, RF_Transient);
	OutBiomes->AssignBiomes(OutSettings, OutGen);
}

// -----------------------------------------------------------------------
// Test: Null Heightmap â†’ empty result
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_PlacementAreas_NullHeightmap,
	"Priordium.MapGenerator.ResourceDistributor.PlacementAreas.NullHeightmap",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_PlacementAreas_NullHeightmap::RunTest(const FString& Parameters)
{
	UResourceDistributor* Dist = NewObject<UResourceDistributor>(GetTransientPackage(), NAME_None, RF_Transient);
	UBiomeManager* Biomes = NewObject<UBiomeManager>(GetTransientPackage(), NAME_None, RF_Transient);

	AddExpectedError(TEXT("Heightmap is null"), EAutomationExpectedErrorFlags::Contains, 1);
	const TArray<FIntPoint> Result = Dist->CalculatePlacementAreas({}, nullptr, Biomes, nullptr, nullptr);
	TestEqual(TEXT("Null Heightmap returns empty array"), Result.Num(), 0);
	return true;
}

// -----------------------------------------------------------------------
// Test: Null BiomeManager â†’ empty result
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_PlacementAreas_NullBiomeManager,
	"Priordium.MapGenerator.ResourceDistributor.PlacementAreas.NullBiomeManager",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_PlacementAreas_NullBiomeManager::RunTest(const FString& Parameters)
{
	UMapGeneratorSettings* S;
	UHeightmapGenerator*   Gen;
	UBiomeManager*         Biomes;
	MakeHeightmapAndBiomes(33, 42, S, Gen, Biomes);

	UResourceDistributor* Dist = NewObject<UResourceDistributor>(GetTransientPackage(), NAME_None, RF_Transient);

	AddExpectedError(TEXT("BiomeManager is null"), EAutomationExpectedErrorFlags::Contains, 1);
	const TArray<FIntPoint> Result = Dist->CalculatePlacementAreas({}, Gen, nullptr, nullptr, nullptr);
	TestEqual(TEXT("Null BiomeManager returns empty array"), Result.Num(), 0);
	return true;
}

// -----------------------------------------------------------------------
// Test: All returned cells are within heightmap bounds
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_PlacementAreas_AllCellsInBounds,
	"Priordium.MapGenerator.ResourceDistributor.PlacementAreas.AllCellsInBounds",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_PlacementAreas_AllCellsInBounds::RunTest(const FString& Parameters)
{
	UMapGeneratorSettings* S;
	UHeightmapGenerator*   Gen;
	UBiomeManager*         Biomes;
	MakeHeightmapAndBiomes(33, 42, S, Gen, Biomes);

	UResourceDistributor* Dist = NewObject<UResourceDistributor>(GetTransientPackage(), NAME_None, RF_Transient);
	FResourceSpawnRule Rule; // No restrictions

	const TArray<FIntPoint> Cells = Dist->CalculatePlacementAreas(Rule, Gen, Biomes, nullptr, nullptr);
	const FIntPoint Res = Gen->GetResolution();

	bool bAllInBounds = true;
	for (const FIntPoint& C : Cells)
	{
		if (C.X < 0 || C.X >= Res.X || C.Y < 0 || C.Y >= Res.Y)
		{
			bAllInBounds = false;
			break;
		}
	}
	TestTrue(TEXT("All placement cells are within heightmap bounds"), bAllInBounds);
	return true;
}

// -----------------------------------------------------------------------
// Test: Biome filter reduces the count compared to no filter
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_PlacementAreas_FilterReducesCount,
	"Priordium.MapGenerator.ResourceDistributor.PlacementAreas.FilterReducesCount",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_PlacementAreas_FilterReducesCount::RunTest(const FString& Parameters)
{
	UMapGeneratorSettings* S;
	UHeightmapGenerator*   Gen;
	UBiomeManager*         Biomes;
	MakeHeightmapAndBiomes(65, 42, S, Gen, Biomes);

	UResourceDistributor* Dist = NewObject<UResourceDistributor>(GetTransientPackage(), NAME_None, RF_Transient);

	FResourceSpawnRule NoFilter;
	FResourceSpawnRule ForestOnly;
	ForestOnly.AllowedBiomes.Add(EBiomeType::Forest);

	const int32 AllCount    = Dist->CalculatePlacementAreas(NoFilter,    Gen, Biomes, nullptr, nullptr).Num();
	const int32 ForestCount = Dist->CalculatePlacementAreas(ForestOnly,  Gen, Biomes, nullptr, nullptr).Num();

	TestTrue(TEXT("No filter returns more cells than Forest-only filter"),
		AllCount >= ForestCount);
	TestTrue(TEXT("Some valid cells were found"), AllCount > 0);
	return true;
}
