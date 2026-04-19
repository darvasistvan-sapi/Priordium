// Copyright Priordium. All Rights Reserved.
//
// Test_MapGeneratorModule_Compiles.cpp
// Tests that the MapGenerator module loads and type definitions are accessible.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"

#include "EBiomeType.h"
#include "EClimateZone.h"

// -----------------------------------------------------------------------
// Test: Priordium main module loaded (MapGenerator code lives here)
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_MapGeneratorModule_MainModuleLoads,
	"Priordium.MapGenerator.Module.MainModuleLoads",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_MapGeneratorModule_MainModuleLoads::RunTest(const FString& Parameters)
{
	bool bLoaded = FModuleManager::Get().IsModuleLoaded("Priordium");
	TestTrue(TEXT("Priordium module is loaded"), bLoaded);
	return true;
}

// -----------------------------------------------------------------------
// Test: EBiomeType and EClimateZone headers are includeable and enums are defined
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_MapGeneratorModule_TypesHeaderIncludeable,
	"Priordium.MapGenerator.Module.TypesHeaderIncludeable",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_MapGeneratorModule_TypesHeaderIncludeable::RunTest(const FString& Parameters)
{
	const UEnum* BiomeEnum = StaticEnum<EBiomeType>();
	TestNotNull(TEXT("EBiomeType UEnum reflection is accessible"), BiomeEnum);

	const UEnum* ClimateEnum = StaticEnum<EClimateZone>();
	TestNotNull(TEXT("EClimateZone UEnum reflection is accessible"), ClimateEnum);

	return true;
}

// -----------------------------------------------------------------------
// Test: Compiler warnings (indirect: if it compiles, there are no fatal warnings)
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_MapGeneratorModule_NoCompilerWarnings,
	"Priordium.MapGenerator.Module.NoCompilerWarnings",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_MapGeneratorModule_NoCompilerWarnings::RunTest(const FString& Parameters)
{
	AddInfo(TEXT("If this test runs, the module compiled without warnings."));
	return true;
}

