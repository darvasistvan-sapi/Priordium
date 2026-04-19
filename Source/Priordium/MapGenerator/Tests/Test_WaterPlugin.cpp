// Copyright Priordium. All Rights Reserved.
//
// Test_WaterPlugin.cpp
// Tests Water plugin availability: module loaded, AWaterBody classes accessible.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"

// -----------------------------------------------------------------------
// Test: Water module is loaded
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_WaterPlugin_ModuleLoaded,
	"Priordium.MapGenerator.WaterSystemBuilder.WaterPlugin.ModuleLoaded",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_WaterPlugin_ModuleLoaded::RunTest(const FString& Parameters)
{
	const bool bLoaded = FModuleManager::Get().IsModuleLoaded(TEXT("Water"));
	TestTrue(TEXT("Water module is loaded"), bLoaded);
	return true;
}

// -----------------------------------------------------------------------
// Test: AWaterBodyRiver UClass is accessible via reflection
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_WaterPlugin_WaterBodyRiverClassExists,
	"Priordium.MapGenerator.WaterSystemBuilder.WaterPlugin.WaterBodyRiverClassExists",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_WaterPlugin_WaterBodyRiverClassExists::RunTest(const FString& Parameters)
{
	UClass* RiverClass = UClass::TryFindTypeSlow<UClass>(TEXT("/Script/Water.WaterBodyRiver"));
	TestNotNull(TEXT("AWaterBodyRiver UClass is accessible"), RiverClass);
	return true;
}

// -----------------------------------------------------------------------
// Test: AWaterBodyLake UClass is accessible via reflection
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_WaterPlugin_WaterBodyLakeClassExists,
	"Priordium.MapGenerator.WaterSystemBuilder.WaterPlugin.WaterBodyLakeClassExists",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_WaterPlugin_WaterBodyLakeClassExists::RunTest(const FString& Parameters)
{
	UClass* LakeClass = UClass::TryFindTypeSlow<UClass>(TEXT("/Script/Water.WaterBodyLake"));
	TestNotNull(TEXT("AWaterBodyLake UClass is accessible"), LakeClass);
	return true;
}

// -----------------------------------------------------------------------
// Test: AWaterBodyOcean UClass is accessible via reflection
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_WaterPlugin_WaterBodyOceanClassExists,
	"Priordium.MapGenerator.WaterSystemBuilder.WaterPlugin.WaterBodyOceanClassExists",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_WaterPlugin_WaterBodyOceanClassExists::RunTest(const FString& Parameters)
{
	UClass* OceanClass = UClass::TryFindTypeSlow<UClass>(TEXT("/Script/Water.WaterBodyOcean"));
	TestNotNull(TEXT("AWaterBodyOcean UClass is accessible"), OceanClass);
	return true;
}
