// Copyright Priordium. All Rights Reserved.
//
// Test_ClimateZoneManager_Structure.cpp
// Tests the structure, instantiability, and basic interface of UClimateZoneManager.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UClimateZoneManager.h"

// -----------------------------------------------------------------------
// Test: UClimateZoneManager can be created as a UObject
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ClimateZoneManager_CanCreate,
	"Priordium.MapGenerator.ClimateZoneManager.CanCreate",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ClimateZoneManager_CanCreate::RunTest(const FString& Parameters)
{
	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);
	TestNotNull(TEXT("UClimateZoneManager can be created"), Manager);
	return true;
}

// -----------------------------------------------------------------------
// Test: Not initialized by default (ClimateZoneMap empty)
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ClimateZoneManager_NotInitializedByDefault,
	"Priordium.MapGenerator.ClimateZoneManager.NotInitializedByDefault",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ClimateZoneManager_NotInitializedByDefault::RunTest(const FString& Parameters)
{
	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);
	TestFalse(TEXT("Empty manager is not initialized"), Manager->IsInitialized());
	TestEqual(TEXT("ClimateZoneMap initially empty"), Manager->ClimateZoneMap.Num(), 0);
	return true;
}

// -----------------------------------------------------------------------
// Test: UClimateZoneManager is accessible in the UObject reflection system
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ClimateZoneManager_ClassExists,
	"Priordium.MapGenerator.ClimateZoneManager.ClassExists",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ClimateZoneManager_ClassExists::RunTest(const FString& Parameters)
{
	UClass* ManagerClass = UClimateZoneManager::StaticClass();
	TestNotNull(TEXT("UClimateZoneManager UClass exists"), ManagerClass);
	return true;
}

// -----------------------------------------------------------------------
// Test: GetClimateZoneAt returns Temperate for invalid index
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ClimateZoneManager_InvalidIndex_ReturnsTemperate,
	"Priordium.MapGenerator.ClimateZoneManager.InvalidIndex_ReturnsTemperate",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ClimateZoneManager_InvalidIndex_ReturnsTemperate::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("GetClimateZoneAt -- invalid index"), EAutomationExpectedErrorFlags::Contains, 3);

	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);

	EClimateZone Zone = Manager->GetClimateZoneAt(0);
	TestEqual(TEXT("Empty map - index 0 -> Temperate"), Zone, EClimateZone::Temperate);

	Zone = Manager->GetClimateZoneAt(-1);
	TestEqual(TEXT("Negative index -> Temperate"), Zone, EClimateZone::Temperate);

	Zone = Manager->GetClimateZoneAt(9999);
	TestEqual(TEXT("Too large index -> Temperate"), Zone, EClimateZone::Temperate);

	return true;
}

// -----------------------------------------------------------------------
// Test: CalculateClimateZones returns false with empty BiomeMap
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ClimateZoneManager_EmptyBiomeMap_ReturnsFalse,
	"Priordium.MapGenerator.ClimateZoneManager.EmptyBiomeMap_ReturnsFalse",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ClimateZoneManager_EmptyBiomeMap_ReturnsFalse::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("CalculateClimateZones -- BiomeMap is empty"), EAutomationExpectedErrorFlags::Contains, 1);

	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);

	TArray<FBiomeCell> EmptyMap;
	bool bResult = Manager->CalculateClimateZones(EmptyMap, nullptr);
	TestFalse(TEXT("Empty BiomeMap -> false"), bResult);

	return true;
}

