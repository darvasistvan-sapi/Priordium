// Copyright Priordium. All Rights Reserved.
//
// Test_GetTemperatureAt.cpp
// Tests GetTemperatureAt() on UClimateZoneManager.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UClimateZoneManager.h"
#include "UMapGeneratorSettings.h"
#include "FBiomeCell.h"

// Helper: build a 1-cell BiomeMap with a given temperature
static TArray<FBiomeCell> MakeSingleCellMap(float Temp)
{
	TArray<FBiomeCell> Map;
	Map.SetNum(1);
	Map[0].Temperature = Temp;
	Map[0].Moisture    = 0.4f;
	Map[0].Height      = 0.2f;
	return Map;
}

static UMapGeneratorSettings* MakeSettings1()
{
	UMapGeneratorSettings* S = NewObject<UMapGeneratorSettings>(
		GetTransientPackage(), NAME_None, RF_Transient);
	S->Resolution = 1;
	return S;
}

// -----------------------------------------------------------------------
// Test: Invalid index returns -1.0f
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_GetTemperatureAt_InvalidIndex,
	"Priordium.MapGenerator.ClimateZoneManager.GetTemperatureAt.InvalidIndex",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_GetTemperatureAt_InvalidIndex::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("GetTemperatureAt -- invalid index"), EAutomationExpectedErrorFlags::Contains, 2);

	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);

	// TemperatureMap is empty before CalculateClimateZones
	TestEqual(TEXT("Index 0 on empty map returns -1"),  Manager->GetTemperatureAt(0,    0.5f), -1.0f);
	TestEqual(TEXT("Index 99 on empty map returns -1"), Manager->GetTemperatureAt(99,   0.5f), -1.0f);
	return true;
}

// -----------------------------------------------------------------------
// Test: Valid index returns seasonally adjusted temperature
//   Summer (TimeOfYear=0.5): BaseTemp + SummerTempModifier
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_GetTemperatureAt_Summer,
	"Priordium.MapGenerator.ClimateZoneManager.GetTemperatureAt.Summer",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_GetTemperatureAt_Summer::RunTest(const FString& Parameters)
{
	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);

	const float BaseTemp = 0.5f;
	TArray<FBiomeCell> Map = MakeSingleCellMap(BaseTemp);
	Manager->CalculateClimateZones(Map, MakeSettings1());

	const float Expected = Manager->CalculateSeasonalTemperature(BaseTemp, 0.5f);
	const float Result   = Manager->GetTemperatureAt(0, 0.5f);

	TestTrue(TEXT("GetTemperatureAt summer matches CalculateSeasonalTemperature"),
		FMath::IsNearlyEqual(Result, Expected, 1e-4f));
	return true;
}

// -----------------------------------------------------------------------
// Test: Valid index returns seasonally adjusted temperature
//   Winter (TimeOfYear=0.0): BaseTemp + WinterTempModifier
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_GetTemperatureAt_Winter,
	"Priordium.MapGenerator.ClimateZoneManager.GetTemperatureAt.Winter",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_GetTemperatureAt_Winter::RunTest(const FString& Parameters)
{
	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);

	const float BaseTemp = 0.5f;
	TArray<FBiomeCell> Map = MakeSingleCellMap(BaseTemp);
	Manager->CalculateClimateZones(Map, MakeSettings1());

	const float Expected = Manager->CalculateSeasonalTemperature(BaseTemp, 0.0f);
	const float Result   = Manager->GetTemperatureAt(0, 0.0f);

	TestTrue(TEXT("GetTemperatureAt winter matches CalculateSeasonalTemperature"),
		FMath::IsNearlyEqual(Result, Expected, 1e-4f));
	return true;
}

// -----------------------------------------------------------------------
// Test: Winter is colder than summer for the same cell
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_GetTemperatureAt_WinterColderThanSummer,
	"Priordium.MapGenerator.ClimateZoneManager.GetTemperatureAt.WinterColderThanSummer",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_GetTemperatureAt_WinterColderThanSummer::RunTest(const FString& Parameters)
{
	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);

	TArray<FBiomeCell> Map = MakeSingleCellMap(0.5f);
	Manager->CalculateClimateZones(Map, MakeSettings1());

	const float Summer = Manager->GetTemperatureAt(0, 0.5f);
	const float Winter = Manager->GetTemperatureAt(0, 0.0f);

	TestTrue(TEXT("Winter is colder than summer"), Winter < Summer);
	return true;
}

// -----------------------------------------------------------------------
// Test: TemperatureMap is stored independently from the BiomeMap
//   (original BiomeMap can go out of scope without affecting results)
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_GetTemperatureAt_TemperaturePreserved,
	"Priordium.MapGenerator.ClimateZoneManager.GetTemperatureAt.TemperaturePreserved",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_GetTemperatureAt_TemperaturePreserved::RunTest(const FString& Parameters)
{
	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);

	// Populate with a known temperature
	const float KnownTemp = 0.7f;
	{
		TArray<FBiomeCell> Map = MakeSingleCellMap(KnownTemp);
		Manager->CalculateClimateZones(Map, MakeSettings1());
	}
	// BiomeMap is out of scope; TemperatureMap is still accessible
	const float Result = Manager->GetTemperatureAt(0, 0.5f);
	TestTrue(TEXT("GetTemperatureAt returns valid result after BiomeMap leaves scope"), Result >= 0.0f);
	return true;
}
