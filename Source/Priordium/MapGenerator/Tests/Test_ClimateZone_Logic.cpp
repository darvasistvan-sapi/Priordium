// Copyright Priordium. All Rights Reserved.
//
// Test_ClimateZone_Logic.cpp
// Tests the correctness of the DetermineClimateZone() algorithm:
// temperature-based zone determination and altitude correction.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UClimateZoneManager.h"
#include "FBiomeCell.h"

// -----------------------------------------------------------------------
// Test: High temperature + high moisture -> Tropical
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ClimateZone_HighTempHighMoisture_IsTropical,
	"Priordium.MapGenerator.ClimateZone.Logic.HighTempHighMoisture_IsTropical",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ClimateZone_HighTempHighMoisture_IsTropical::RunTest(const FString& Parameters)
{
	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);

	FBiomeCell Cell;
	Cell.Temperature = 0.9f;
	Cell.Moisture    = 0.8f;
	Cell.Height      = 0.3f;

	EClimateZone Zone = Manager->DetermineClimateZone(Cell);
	TestEqual(TEXT("Temp=0.9 Moisture=0.8 -> Tropical"), Zone, EClimateZone::Tropical);
	return true;
}

// -----------------------------------------------------------------------
// Test: High temperature + low moisture -> Temperate (not Tropical)
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ClimateZone_HighTempLowMoisture_IsNotTropical,
	"Priordium.MapGenerator.ClimateZone.Logic.HighTempLowMoisture_IsNotTropical",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ClimateZone_HighTempLowMoisture_IsNotTropical::RunTest(const FString& Parameters)
{
	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);

	FBiomeCell Cell;
	Cell.Temperature = 0.9f;
	Cell.Moisture    = 0.2f;  // low moisture
	Cell.Height      = 0.3f;

	EClimateZone Zone = Manager->DetermineClimateZone(Cell);
	TestEqual(TEXT("Temp=0.9 Moisture=0.2 -> Temperate"), Zone, EClimateZone::Temperate);
	return true;
}

// -----------------------------------------------------------------------
// Test: Medium temperature -> Temperate
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ClimateZone_MediumTemp_IsTemperate,
	"Priordium.MapGenerator.ClimateZone.Logic.MediumTemp_IsTemperate",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ClimateZone_MediumTemp_IsTemperate::RunTest(const FString& Parameters)
{
	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);

	FBiomeCell Cell;
	Cell.Temperature = 0.6f;
	Cell.Moisture    = 0.4f;
	Cell.Height      = 0.3f;

	EClimateZone Zone = Manager->DetermineClimateZone(Cell);
	TestEqual(TEXT("Temp=0.6 -> Temperate"), Zone, EClimateZone::Temperate);
	return true;
}

// -----------------------------------------------------------------------
// Test: Low temperature -> Continental
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ClimateZone_LowTemp_IsContinental,
	"Priordium.MapGenerator.ClimateZone.Logic.LowTemp_IsContinental",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ClimateZone_LowTemp_IsContinental::RunTest(const FString& Parameters)
{
	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);

	FBiomeCell Cell;
	Cell.Temperature = 0.3f;
	Cell.Moisture    = 0.4f;
	Cell.Height      = 0.3f;

	EClimateZone Zone = Manager->DetermineClimateZone(Cell);
	TestEqual(TEXT("Temp=0.3 -> Continental"), Zone, EClimateZone::Continental);
	return true;
}

// -----------------------------------------------------------------------
// Test: Very low temperature -> Subarctic
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ClimateZone_VeryLowTemp_IsSubarctic,
	"Priordium.MapGenerator.ClimateZone.Logic.VeryLowTemp_IsSubarctic",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ClimateZone_VeryLowTemp_IsSubarctic::RunTest(const FString& Parameters)
{
	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);

	FBiomeCell Cell;
	Cell.Temperature = 0.1f;
	Cell.Moisture    = 0.3f;
	Cell.Height      = 0.2f;

	EClimateZone Zone = Manager->DetermineClimateZone(Cell);
	TestEqual(TEXT("Temp=0.1 -> Subarctic"), Zone, EClimateZone::Subarctic);
	return true;
}

// -----------------------------------------------------------------------
// Test: Altitude correction -- high mountain cools the cell
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ClimateZone_HeightCorrection_CoolsDown,
	"Priordium.MapGenerator.ClimateZone.Logic.HeightCorrection_CoolsDown",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ClimateZone_HeightCorrection_CoolsDown::RunTest(const FString& Parameters)
{
	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);

	// Temp=0.5 + Height=0.8 -> effective temperature = 0.5 - 0.2 = 0.3 -> Continental
	FBiomeCell HighCell;
	HighCell.Temperature = 0.5f;
	HighCell.Moisture    = 0.4f;
	HighCell.Height      = 0.8f;  // > 0.7 -> -0.2 correction

	EClimateZone ZoneHigh = Manager->DetermineClimateZone(HighCell);
	TestEqual(TEXT("Temp=0.5 Height=0.8 -> Continental (after correction)"), ZoneHigh, EClimateZone::Continental);

	// Same values, low altitude -> Temperate (no correction)
	FBiomeCell LowCell;
	LowCell.Temperature = 0.5f;
	LowCell.Moisture    = 0.4f;
	LowCell.Height      = 0.3f;  // < 0.7 -> no correction

	EClimateZone ZoneLow = Manager->DetermineClimateZone(LowCell);
	TestEqual(TEXT("Temp=0.5 Height=0.3 -> Temperate (no correction)"), ZoneLow, EClimateZone::Temperate);

	return true;
}

// -----------------------------------------------------------------------
// Test: At altitude correction boundary (Height = 0.7) -- no correction
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ClimateZone_HeightBoundary_NoCorrection,
	"Priordium.MapGenerator.ClimateZone.Logic.HeightBoundary_NoCorrection",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ClimateZone_HeightBoundary_NoCorrection::RunTest(const FString& Parameters)
{
	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);

	// Exactly at 0.7 there is no correction (Height > 0.7 condition)
	FBiomeCell Cell;
	Cell.Temperature = 0.5f;
	Cell.Moisture    = 0.4f;
	Cell.Height      = 0.7f;

	EClimateZone Zone = Manager->DetermineClimateZone(Cell);
	TestEqual(TEXT("Temp=0.5 Height=0.7 (boundary) -> Temperate, no correction"), Zone, EClimateZone::Temperate);
	return true;
}

// -----------------------------------------------------------------------
// Test: Altitude correction shifts zone from Tropical to Temperate
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ClimateZone_HeightCorrection_TropicalToTemperate,
	"Priordium.MapGenerator.ClimateZone.Logic.HeightCorrection_TropicalToTemperate",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ClimateZone_HeightCorrection_TropicalToTemperate::RunTest(const FString& Parameters)
{
	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);

	// Temp=0.9 Moisture=0.8 on flatland -> Tropical
	FBiomeCell FlatCell;
	FlatCell.Temperature = 0.9f;
	FlatCell.Moisture    = 0.8f;
	FlatCell.Height      = 0.3f;
	TestEqual(TEXT("Tropical on flatland"), Manager->DetermineClimateZone(FlatCell), EClimateZone::Tropical);

	// Same values, high mountain -> Temp=0.9-0.2=0.7, but 0.7 is not > 0.75 -> Temperate
	FBiomeCell MountainCell;
	MountainCell.Temperature = 0.9f;
	MountainCell.Moisture    = 0.8f;
	MountainCell.Height      = 0.8f;
	TestEqual(TEXT("High mountain Tropical -> Temperate"), Manager->DetermineClimateZone(MountainCell), EClimateZone::Temperate);

	return true;
}

