// Copyright Priordium. All Rights Reserved.
//
// Test_ClearGeneratedWorld.cpp
// Tests AMapGenerator::ClearGeneratedWorld().

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "AMapGenerator.h"
#include "UMapGeneratorSettings.h"
#include "UResourceDistributor.h"

// -----------------------------------------------------------------------
// Helper
// -----------------------------------------------------------------------

static UMapGeneratorSettings* MakeMinimalSettings_CW()
{
	UMapGeneratorSettings* S = NewObject<UMapGeneratorSettings>(
		GetTransientPackage(), NAME_None, RF_Transient);
	S->MapSizeX        = 1;
	S->MapSizeY        = 1;
	S->Resolution      = 33;
	S->QuadSize        = 100;
	S->SeaLevel        = 0.3f;
	S->Seed            = 99;
	// Disable water actor spawning: Water plugin blueprints are not available
	// in automated test environments and would produce load warnings.
	S->bGenerateRivers = false;
	S->bGenerateLakes  = false;
	return S;
}

// -----------------------------------------------------------------------
// Test: ClearGeneratedWorld on fresh (never generated) actor is safe
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ClearGeneratedWorld_SafeWhenNotGenerated,
	"Priordium.MapGenerator.AMapGenerator.ClearGeneratedWorld.SafeWhenNotGenerated",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ClearGeneratedWorld_SafeWhenNotGenerated::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	if (!World) { AddWarning(TEXT("GWorld is null, skipping")); return true; }

	AMapGenerator* Gen = World->SpawnActor<AMapGenerator>(AMapGenerator::StaticClass());
	if (!TestNotNull(TEXT("AMapGenerator spawned"), Gen)) return true;

	// Should not crash
	Gen->ClearGeneratedWorld();

	TestFalse(TEXT("IsGenerating is false after clear"), Gen->IsGenerating());
	TestEqual(TEXT("Progress is 0 after clear"), Gen->GetGenerationProgress(), 0.0f);

	Gen->Destroy();
	return true;
}

// -----------------------------------------------------------------------
// Test: ClearGeneratedWorld after GenerateWorld resets progress to 0
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ClearGeneratedWorld_ResetsProgress,
	"Priordium.MapGenerator.AMapGenerator.ClearGeneratedWorld.ResetsProgress",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ClearGeneratedWorld_ResetsProgress::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	if (!World) { AddWarning(TEXT("GWorld is null, skipping")); return true; }

	AMapGenerator* Gen = World->SpawnActor<AMapGenerator>(AMapGenerator::StaticClass());
	if (!TestNotNull(TEXT("AMapGenerator spawned"), Gen)) return true;

	Gen->GeneratorSettings = MakeMinimalSettings_CW();
	Gen->GenerateWorld();

	// Progress should be 1.0 after generation
	TestEqual(TEXT("Progress is 1.0 after generate"), Gen->GetGenerationProgress(), 1.0f);

	Gen->ClearGeneratedWorld();
	TestEqual(TEXT("Progress is 0.0 after ClearGeneratedWorld"), Gen->GetGenerationProgress(), 0.0f);

	Gen->Destroy();
	return true;
}

// -----------------------------------------------------------------------
// Test: ClearGeneratedWorld empties ResourceDistributor
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ClearGeneratedWorld_ClearsResources,
	"Priordium.MapGenerator.AMapGenerator.ClearGeneratedWorld.ClearsResources",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ClearGeneratedWorld_ClearsResources::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	if (!World) { AddWarning(TEXT("GWorld is null, skipping")); return true; }

	AMapGenerator* Gen = World->SpawnActor<AMapGenerator>(AMapGenerator::StaticClass());
	if (!TestNotNull(TEXT("AMapGenerator spawned"), Gen)) return true;

	// Manually spawn a resource so ResourceDistributor has something to clear
	if (TestNotNull(TEXT("ResourceDistributor is valid"), Gen->ResourceDistributor))
	{
		Gen->ResourceDistributor->SpawnResource(
			World, AActor::StaticClass(), FGameplayTag(), FVector::ZeroVector);
		TestEqual(TEXT("ResourceDistributor has 1 resource before clear"),
			Gen->ResourceDistributor->GetSpawnedResources().Num(), 1);

		Gen->ClearGeneratedWorld();

		TestEqual(TEXT("ResourceDistributor is empty after ClearGeneratedWorld"),
			Gen->ResourceDistributor->GetSpawnedResources().Num(), 0);
	}

	Gen->Destroy();
	return true;
}
