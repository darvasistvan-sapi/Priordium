// Copyright Priordium. All Rights Reserved.
//
// Test_ValidateSettings.cpp
// Tests AMapGenerator::ValidateSettings().

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "AMapGenerator.h"
#include "UMapGeneratorSettings.h"

// -----------------------------------------------------------------------
// Helper: create a fully valid UMapGeneratorSettings
// -----------------------------------------------------------------------

static UMapGeneratorSettings* MakeValidSettings()
{
	UMapGeneratorSettings* S = NewObject<UMapGeneratorSettings>(
		GetTransientPackage(), NAME_None, RF_Transient);
	S->MapSizeX   = 4;
	S->MapSizeY   = 4;
	S->Resolution = 33;
	S->QuadSize   = 100;
	S->SeaLevel   = 0.3f;
	return S;
}

// -----------------------------------------------------------------------
// Test: null GeneratorSettings â†’ false
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ValidateSettings_NullSettings,
	"Priordium.MapGenerator.AMapGenerator.ValidateSettings.NullSettings",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ValidateSettings_NullSettings::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	if (!World) { AddWarning(TEXT("GWorld is null, skipping")); return true; }

	AMapGenerator* Gen = World->SpawnActor<AMapGenerator>(AMapGenerator::StaticClass());
	if (!TestNotNull(TEXT("AMapGenerator spawned"), Gen)) return true;

	Gen->GeneratorSettings = nullptr;
	AddExpectedError(TEXT("GeneratorSettings is null"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("ValidateSettings returns false when GeneratorSettings is null"),
		Gen->ValidateSettings());

	Gen->Destroy();
	return true;
}

// -----------------------------------------------------------------------
// Test: MapSizeX == 0 â†’ false
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ValidateSettings_ZeroMapSizeX,
	"Priordium.MapGenerator.AMapGenerator.ValidateSettings.ZeroMapSizeX",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ValidateSettings_ZeroMapSizeX::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	if (!World) { AddWarning(TEXT("GWorld is null, skipping")); return true; }

	AMapGenerator* Gen = World->SpawnActor<AMapGenerator>(AMapGenerator::StaticClass());
	if (!TestNotNull(TEXT("AMapGenerator spawned"), Gen)) return true;

	UMapGeneratorSettings* S = MakeValidSettings();
	S->MapSizeX = 0;
	Gen->GeneratorSettings = S;
	AddExpectedError(TEXT("MapSizeX"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("ValidateSettings returns false for MapSizeX == 0"),
		Gen->ValidateSettings());

	Gen->Destroy();
	return true;
}

// -----------------------------------------------------------------------
// Test: Resolution < 33 â†’ false
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ValidateSettings_ResolutionTooSmall,
	"Priordium.MapGenerator.AMapGenerator.ValidateSettings.ResolutionTooSmall",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ValidateSettings_ResolutionTooSmall::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	if (!World) { AddWarning(TEXT("GWorld is null, skipping")); return true; }

	AMapGenerator* Gen = World->SpawnActor<AMapGenerator>(AMapGenerator::StaticClass());
	if (!TestNotNull(TEXT("AMapGenerator spawned"), Gen)) return true;

	UMapGeneratorSettings* S = MakeValidSettings();
	S->Resolution = 16;
	Gen->GeneratorSettings = S;
	AddExpectedError(TEXT("Resolution must be"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("ValidateSettings returns false for Resolution < 33"),
		Gen->ValidateSettings());

	Gen->Destroy();
	return true;
}

// -----------------------------------------------------------------------
// Test: SeaLevel out of [0,1] â†’ false
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ValidateSettings_SeaLevelOutOfRange,
	"Priordium.MapGenerator.AMapGenerator.ValidateSettings.SeaLevelOutOfRange",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ValidateSettings_SeaLevelOutOfRange::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	if (!World) { AddWarning(TEXT("GWorld is null, skipping")); return true; }

	AMapGenerator* Gen = World->SpawnActor<AMapGenerator>(AMapGenerator::StaticClass());
	if (!TestNotNull(TEXT("AMapGenerator spawned"), Gen)) return true;

	UMapGeneratorSettings* S = MakeValidSettings();
	S->SeaLevel = 1.5f;
	Gen->GeneratorSettings = S;
	AddExpectedError(TEXT("SeaLevel must be"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("ValidateSettings returns false for SeaLevel > 1"),
		Gen->ValidateSettings());

	Gen->Destroy();
	return true;
}

// -----------------------------------------------------------------------
// Test: fully valid settings â†’ true
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ValidateSettings_ValidReturnsTrue,
	"Priordium.MapGenerator.AMapGenerator.ValidateSettings.ValidReturnsTrue",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ValidateSettings_ValidReturnsTrue::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	if (!World) { AddWarning(TEXT("GWorld is null, skipping")); return true; }

	AMapGenerator* Gen = World->SpawnActor<AMapGenerator>(AMapGenerator::StaticClass());
	if (!TestNotNull(TEXT("AMapGenerator spawned"), Gen)) return true;

	Gen->GeneratorSettings = MakeValidSettings();
	TestTrue(TEXT("ValidateSettings returns true for fully valid settings"),
		Gen->ValidateSettings());

	Gen->Destroy();
	return true;
}
