// Copyright Priordium. All Rights Reserved.
//
// Test_GetClimateZoneAt.cpp
// Tests GetClimateZoneAt() on UClimateZoneManager.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UClimateZoneManager.h"
#include "UMapGeneratorSettings.h"
#include "FBiomeCell.h"

// Helper: build a single-cell BiomeMap
static TArray<FBiomeCell> MakeOneCell(float Temp, float Moisture, float Height)
{
	TArray<FBiomeCell> Map;
	Map.SetNum(1);
	Map[0].Temperature = Temp;
	Map[0].Moisture    = Moisture;
	Map[0].Height      = Height;
	return Map;
}

static UMapGeneratorSettings* MakeRes1Settings()
{
	UMapGeneratorSettings* S = NewObject<UMapGeneratorSettings>(
		GetTransientPackage(), NAME_None, RF_Transient);
	S->Resolution = 1;
	return S;
}

// -----------------------------------------------------------------------
// Test: GetClimateZoneAt returns Temperate for invalid index (bounds check)
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_GetClimateZoneAt_InvalidIndex_DefaultsTemperate,
	"Priordium.MapGenerator.ClimateZoneManager.GetClimateZoneAt.InvalidIndex_DefaultsTemperate",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_GetClimateZoneAt_InvalidIndex_DefaultsTemperate::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("GetClimateZoneAt -- invalid index"), EAutomationExpectedErrorFlags::Contains, 3);

	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);

	// Map is empty -- any index is invalid
	TestEqual(TEXT("Index 0 on empty map -> Temperate"),
		Manager->GetClimateZoneAt(0),    EClimateZone::Temperate);
	TestEqual(TEXT("Negative index -> Temperate"),
		Manager->GetClimateZoneAt(-1),   EClimateZone::Temperate);
	TestEqual(TEXT("Large index -> Temperate"),
		Manager->GetClimateZoneAt(9999), EClimateZone::Temperate);
	return true;
}

// -----------------------------------------------------------------------
// Test: GetClimateZoneAt returns Tropical for a tropical cell
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_GetClimateZoneAt_ReturnsCorrectZone_Tropical,
	"Priordium.MapGenerator.ClimateZoneManager.GetClimateZoneAt.ReturnsCorrectZone_Tropical",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_GetClimateZoneAt_ReturnsCorrectZone_Tropical::RunTest(const FString& Parameters)
{
	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);

	TArray<FBiomeCell> Map = MakeOneCell(0.9f, 0.8f, 0.2f);
	Manager->CalculateClimateZones(Map, MakeRes1Settings());

	TestEqual(TEXT("Tropical cell -> GetClimateZoneAt returns Tropical"),
		Manager->GetClimateZoneAt(0), EClimateZone::Tropical);
	return true;
}

// -----------------------------------------------------------------------
// Test: GetClimateZoneAt returns Subarctic for a cold cell
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_GetClimateZoneAt_ReturnsCorrectZone_Subarctic,
	"Priordium.MapGenerator.ClimateZoneManager.GetClimateZoneAt.ReturnsCorrectZone_Subarctic",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_GetClimateZoneAt_ReturnsCorrectZone_Subarctic::RunTest(const FString& Parameters)
{
	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);

	TArray<FBiomeCell> Map = MakeOneCell(0.1f, 0.2f, 0.2f);
	Manager->CalculateClimateZones(Map, MakeRes1Settings());

	TestEqual(TEXT("Cold cell -> GetClimateZoneAt returns Subarctic"),
		Manager->GetClimateZoneAt(0), EClimateZone::Subarctic);
	return true;
}

// -----------------------------------------------------------------------
// Test: GetClimateZoneAt matches DetermineClimateZone for same input
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_GetClimateZoneAt_MatchesDetermineClimateZone,
	"Priordium.MapGenerator.ClimateZoneManager.GetClimateZoneAt.MatchesDetermineClimateZone",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_GetClimateZoneAt_MatchesDetermineClimateZone::RunTest(const FString& Parameters)
{
	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);

	// 4 representative cells
	TArray<FBiomeCell> Map;
	Map.SetNum(4);
	Map[0].Temperature = 0.9f; Map[0].Moisture = 0.8f; Map[0].Height = 0.2f; // Tropical
	Map[1].Temperature = 0.6f; Map[1].Moisture = 0.4f; Map[1].Height = 0.2f; // Temperate
	Map[2].Temperature = 0.3f; Map[2].Moisture = 0.4f; Map[2].Height = 0.2f; // Continental
	Map[3].Temperature = 0.1f; Map[3].Moisture = 0.2f; Map[3].Height = 0.2f; // Subarctic

	UMapGeneratorSettings* Settings = NewObject<UMapGeneratorSettings>(
		GetTransientPackage(), NAME_None, RF_Transient);
	Settings->Resolution = 2;
	Manager->CalculateClimateZones(Map, Settings);

	for (int32 i = 0; i < Map.Num(); ++i)
	{
		const EClimateZone Expected = Manager->DetermineClimateZone(Map[i]);
		TestEqual(
			FString::Printf(TEXT("GetClimateZoneAt(%d) matches DetermineClimateZone"), i),
			Manager->GetClimateZoneAt(i), Expected);
	}
	return true;
}

// -----------------------------------------------------------------------
// Test: GetClimateZoneAt returns out-of-bounds Temperate after filling a 2-cell map
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_GetClimateZoneAt_OutOfBoundsAfterInit,
	"Priordium.MapGenerator.ClimateZoneManager.GetClimateZoneAt.OutOfBoundsAfterInit",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_GetClimateZoneAt_OutOfBoundsAfterInit::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("GetClimateZoneAt -- invalid index"), EAutomationExpectedErrorFlags::Contains, 1);

	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);

	TArray<FBiomeCell> Map;
	Map.SetNum(2);
	Map[0].Temperature = 0.9f; Map[0].Moisture = 0.8f; Map[0].Height = 0.2f;
	Map[1].Temperature = 0.1f; Map[1].Moisture = 0.2f; Map[1].Height = 0.2f;

	UMapGeneratorSettings* Settings = NewObject<UMapGeneratorSettings>(
		GetTransientPackage(), NAME_None, RF_Transient);
	Settings->Resolution = 2;
	Manager->CalculateClimateZones(Map, Settings);

	// Index 2 is out of bounds for a 2-cell map
	TestEqual(TEXT("Index 2 on 2-cell map -> Temperate"),
		Manager->GetClimateZoneAt(2), EClimateZone::Temperate);
	return true;
}
