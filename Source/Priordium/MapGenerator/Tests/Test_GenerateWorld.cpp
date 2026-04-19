// Copyright Priordium. All Rights Reserved.
//
// Test_GenerateWorld.cpp
// Tests AMapGenerator::GenerateWorld() via observable state.
//
// Note: Dynamic multicast delegates require UFUNCTION-bound callbacks.
// We verify delegate outcomes indirectly via IsGenerating() and
// GetGenerationProgress() which are set before/after broadcasts.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "AMapGenerator.h"
#include "UMapGeneratorSettings.h"

// -----------------------------------------------------------------------
// Helper
// -----------------------------------------------------------------------

static UMapGeneratorSettings* MakeMinimalSettings_GW()
{
	UMapGeneratorSettings* S = NewObject<UMapGeneratorSettings>(
		GetTransientPackage(), NAME_None, RF_Transient);
	S->MapSizeX        = 1;
	S->MapSizeY        = 1;
	S->Resolution      = 33;
	S->QuadSize        = 100;
	S->SeaLevel        = 0.3f;
	S->Seed            = 7;
	// Disable water actor spawning: Water plugin blueprints are not available
	// in automated test environments and would produce load warnings.
	S->bGenerateRivers = false;
	S->bGenerateLakes  = false;
	return S;
}

// -----------------------------------------------------------------------
// Test: GenerateWorld with null settings -- early return, progress stays 0
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_GenerateWorld_NullSettings_EarlyReturn,
	"Priordium.MapGenerator.AMapGenerator.GenerateWorld.NullSettings_EarlyReturn",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_GenerateWorld_NullSettings_EarlyReturn::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	if (!World) { AddWarning(TEXT("GWorld is null, skipping")); return true; }

	AMapGenerator* Gen = World->SpawnActor<AMapGenerator>(AMapGenerator::StaticClass());
	if (!TestNotNull(TEXT("AMapGenerator spawned"), Gen)) return true;

	Gen->GeneratorSettings = nullptr;
	AddExpectedError(TEXT("ValidateSettings -- GeneratorSettings is null"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("GenerateWorld failed"), EAutomationExpectedErrorFlags::Contains, 1);
	Gen->GenerateWorld();

	// If early-returned due to null settings, IsGenerating must be false
	// and progress must remain 0 (never set to 1.0)
	TestFalse(TEXT("IsGenerating is false after failed GenerateWorld"), Gen->IsGenerating());
	TestEqual(TEXT("Progress is 0 after null-settings GenerateWorld"), Gen->GetGenerationProgress(), 0.0f);

	Gen->Destroy();
	return true;
}

// -----------------------------------------------------------------------
// Test: GenerateWorld with valid settings -- completes and progress == 1
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_GenerateWorld_ValidSettings_Completes,
	"Priordium.MapGenerator.AMapGenerator.GenerateWorld.ValidSettings_Completes",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_GenerateWorld_ValidSettings_Completes::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	if (!World) { AddWarning(TEXT("GWorld is null, skipping")); return true; }

	AMapGenerator* Gen = World->SpawnActor<AMapGenerator>(AMapGenerator::StaticClass());
	if (!TestNotNull(TEXT("AMapGenerator spawned"), Gen)) return true;

	Gen->GeneratorSettings = MakeMinimalSettings_GW();
	Gen->GenerateWorld();

	TestFalse(TEXT("IsGenerating() is false after GenerateWorld() returns"), Gen->IsGenerating());
	TestEqual(TEXT("GetGenerationProgress() == 1.0 after success"), Gen->GetGenerationProgress(), 1.0f);

	Gen->Destroy();
	return true;
}

// -----------------------------------------------------------------------
// Test: OnGenerationCompleted delegate is bound (has at least valid object)
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_GenerateWorld_DelegatesExist,
	"Priordium.MapGenerator.AMapGenerator.GenerateWorld.DelegatesExist",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_GenerateWorld_DelegatesExist::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	if (!World) { AddWarning(TEXT("GWorld is null, skipping")); return true; }

	AMapGenerator* Gen = World->SpawnActor<AMapGenerator>(AMapGenerator::StaticClass());
	if (!TestNotNull(TEXT("AMapGenerator spawned"), Gen)) return true;

	// Delegates are UPROPERTY members -- their existence is structural.
	// Calling IsBound() on an empty delegate does not crash.
	const bool bCompletedBound = Gen->OnGenerationCompleted.IsBound();
	const bool bFailedBound    = Gen->OnGenerationFailed.IsBound();
	const bool bStepBound      = Gen->OnStepCompleted.IsBound();

	// We just verify the check itself doesn't crash (they are unbound initially)
	TestFalse(TEXT("OnGenerationCompleted initially unbound"), bCompletedBound);
	TestFalse(TEXT("OnGenerationFailed initially unbound"),    bFailedBound);
	TestFalse(TEXT("OnStepCompleted initially unbound"),       bStepBound);

	Gen->Destroy();
	return true;
}

// -----------------------------------------------------------------------
// Test: Calling GenerateWorld twice in a row -- second call is a no-op
//       (bIsGenerating guard -- synchronous pipeline so this won't trigger
//        mid-generation, but we test re-entrant safety at the API level)
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_GenerateWorld_DoubleCall_Safe,
	"Priordium.MapGenerator.AMapGenerator.GenerateWorld.DoubleCall_Safe",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_GenerateWorld_DoubleCall_Safe::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	if (!World) { AddWarning(TEXT("GWorld is null, skipping")); return true; }

	AMapGenerator* Gen = World->SpawnActor<AMapGenerator>(AMapGenerator::StaticClass());
	if (!TestNotNull(TEXT("AMapGenerator spawned"), Gen)) return true;

	Gen->GeneratorSettings = MakeMinimalSettings_GW();
	Gen->GenerateWorld();
	Gen->GenerateWorld(); // second call should be harmless

	TestFalse(TEXT("IsGenerating is false after double GenerateWorld"), Gen->IsGenerating());
	TestEqual(TEXT("Progress is 1.0 after double GenerateWorld"), Gen->GetGenerationProgress(), 1.0f);

	Gen->Destroy();
	return true;
}
