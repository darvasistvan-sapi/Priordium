// Copyright Priordium. All Rights Reserved.
//
// Test_CalculateClimateZones.cpp
// Tests the CalculateClimateZones() method: ClimateZoneMap population,
// size matching, and realistic zone distribution.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UClimateZoneManager.h"
#include "UMapGeneratorSettings.h"
#include "FBiomeCell.h"

// Helper: build a BiomeMap with a given resolution and uniform temperature/moisture
static TArray<FBiomeCell> MakeBiomeMap(int32 Res, float Temp, float Moisture, float Height)
{
	TArray<FBiomeCell> Map;
	Map.SetNum(Res * Res);
	for (FBiomeCell& Cell : Map)
	{
		Cell.Temperature = Temp;
		Cell.Moisture    = Moisture;
		Cell.Height      = Height;
	}
	return Map;
}

static UMapGeneratorSettings* MakeSettings(int32 Res)
{
	UMapGeneratorSettings* S = NewObject<UMapGeneratorSettings>(
		GetTransientPackage(), NAME_None, RF_Transient);
	S->Resolution = Res;
	return S;
}

// -----------------------------------------------------------------------
// Test: ClimateZoneMap.Num() == BiomeMap.Num()
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_CalculateClimateZones_SizeMatchesBiomeMap,
	"Priordium.MapGenerator.ClimateZoneManager.CalculateClimateZones.SizeMatchesBiomeMap",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_CalculateClimateZones_SizeMatchesBiomeMap::RunTest(const FString& Parameters)
{
	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);

	const int32 Res = 16;
	TArray<FBiomeCell> BiomeMap = MakeBiomeMap(Res, 0.5f, 0.4f, 0.2f);
	UMapGeneratorSettings* Settings = MakeSettings(Res);

	bool bOK = Manager->CalculateClimateZones(BiomeMap, Settings);
	TestTrue(TEXT("CalculateClimateZones returns true"), bOK);
	TestEqual(TEXT("ClimateZoneMap size == BiomeMap size"), Manager->ClimateZoneMap.Num(), BiomeMap.Num());
	return true;
}

// -----------------------------------------------------------------------
// Test: IsInitialized() returns true after CalculateClimateZones
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_CalculateClimateZones_IsInitializedAfterCalc,
	"Priordium.MapGenerator.ClimateZoneManager.CalculateClimateZones.IsInitializedAfterCalc",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_CalculateClimateZones_IsInitializedAfterCalc::RunTest(const FString& Parameters)
{
	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);

	TestFalse(TEXT("Not initialized before calc"), Manager->IsInitialized());

	TArray<FBiomeCell> BiomeMap = MakeBiomeMap(8, 0.5f, 0.4f, 0.2f);
	Manager->CalculateClimateZones(BiomeMap, MakeSettings(8));

	TestTrue(TEXT("Initialized after calc"), Manager->IsInitialized());
	return true;
}

// -----------------------------------------------------------------------
// Test: All-tropical map produces only Tropical zones
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_CalculateClimateZones_AllTropical,
	"Priordium.MapGenerator.ClimateZoneManager.CalculateClimateZones.AllTropical",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_CalculateClimateZones_AllTropical::RunTest(const FString& Parameters)
{
	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);

	TArray<FBiomeCell> BiomeMap = MakeBiomeMap(8, 0.9f, 0.8f, 0.2f);
	Manager->CalculateClimateZones(BiomeMap, MakeSettings(8));

	for (int32 i = 0; i < Manager->ClimateZoneMap.Num(); ++i)
	{
		TestEqual(
			FString::Printf(TEXT("Cell[%d] is Tropical"), i),
			Manager->ClimateZoneMap[i], EClimateZone::Tropical);
	}
	return true;
}

// -----------------------------------------------------------------------
// Test: All-subarctic map produces only Subarctic zones
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_CalculateClimateZones_AllSubarctic,
	"Priordium.MapGenerator.ClimateZoneManager.CalculateClimateZones.AllSubarctic",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_CalculateClimateZones_AllSubarctic::RunTest(const FString& Parameters)
{
	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);

	TArray<FBiomeCell> BiomeMap = MakeBiomeMap(8, 0.1f, 0.2f, 0.2f);
	Manager->CalculateClimateZones(BiomeMap, MakeSettings(8));

	for (int32 i = 0; i < Manager->ClimateZoneMap.Num(); ++i)
	{
		TestEqual(
			FString::Printf(TEXT("Cell[%d] is Subarctic"), i),
			Manager->ClimateZoneMap[i], EClimateZone::Subarctic);
	}
	return true;
}

// -----------------------------------------------------------------------
// Test: Realistic zone distribution -- all 4 zones appear in a mixed map
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_CalculateClimateZones_RealisticDistribution,
	"Priordium.MapGenerator.ClimateZoneManager.CalculateClimateZones.RealisticDistribution",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_CalculateClimateZones_RealisticDistribution::RunTest(const FString& Parameters)
{
	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);

	// Build a map where temperature increases from 0.05 to 0.95 row by row
	// and moisture varies per column -- this guarantees all 4 zones appear
	const int32 Res = 16;
	TArray<FBiomeCell> BiomeMap;
	BiomeMap.SetNum(Res * Res);
	for (int32 y = 0; y < Res; ++y)
	{
		for (int32 x = 0; x < Res; ++x)
		{
			FBiomeCell& Cell = BiomeMap[y * Res + x];
			Cell.Temperature = 0.05f + (float)y / (float)(Res - 1) * 0.9f;
			Cell.Moisture    = 0.05f + (float)x / (float)(Res - 1) * 0.9f;
			Cell.Height      = 0.2f;
		}
	}

	Manager->CalculateClimateZones(BiomeMap, MakeSettings(Res));

	TSet<EClimateZone> FoundZones;
	for (EClimateZone Z : Manager->ClimateZoneMap)
	{
		FoundZones.Add(Z);
	}

	TestTrue(TEXT("Tropical zone present"),    FoundZones.Contains(EClimateZone::Tropical));
	TestTrue(TEXT("Temperate zone present"),   FoundZones.Contains(EClimateZone::Temperate));
	TestTrue(TEXT("Continental zone present"), FoundZones.Contains(EClimateZone::Continental));
	TestTrue(TEXT("Subarctic zone present"),   FoundZones.Contains(EClimateZone::Subarctic));
	return true;
}

// -----------------------------------------------------------------------
// Test: GetClimateZoneAt returns correct zone after CalculateClimateZones
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_CalculateClimateZones_GetZoneAtCorrect,
	"Priordium.MapGenerator.ClimateZoneManager.CalculateClimateZones.GetZoneAtCorrect",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_CalculateClimateZones_GetZoneAtCorrect::RunTest(const FString& Parameters)
{
	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);

	// Cell 0: Tropical, Cell 1: Subarctic
	TArray<FBiomeCell> BiomeMap;
	BiomeMap.SetNum(2);
	BiomeMap[0].Temperature = 0.9f; BiomeMap[0].Moisture = 0.8f; BiomeMap[0].Height = 0.2f;
	BiomeMap[1].Temperature = 0.1f; BiomeMap[1].Moisture = 0.2f; BiomeMap[1].Height = 0.2f;

	UMapGeneratorSettings* Settings = MakeSettings(1);
	Settings->Resolution = 2;
	Manager->CalculateClimateZones(BiomeMap, Settings);

	TestEqual(TEXT("Index 0 is Tropical"),  Manager->GetClimateZoneAt(0), EClimateZone::Tropical);
	TestEqual(TEXT("Index 1 is Subarctic"), Manager->GetClimateZoneAt(1), EClimateZone::Subarctic);
	return true;
}

// -----------------------------------------------------------------------
// Test: Null Settings returns false
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_CalculateClimateZones_NullSettings_ReturnsFalse,
	"Priordium.MapGenerator.ClimateZoneManager.CalculateClimateZones.NullSettings_ReturnsFalse",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_CalculateClimateZones_NullSettings_ReturnsFalse::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("CalculateClimateZones -- Settings null"), EAutomationExpectedErrorFlags::Contains, 1);

	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);

	TArray<FBiomeCell> BiomeMap = MakeBiomeMap(4, 0.5f, 0.4f, 0.2f);
	bool bOK = Manager->CalculateClimateZones(BiomeMap, nullptr);
	TestFalse(TEXT("Null Settings -> false"), bOK);
	return true;
}
