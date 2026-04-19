// Copyright Priordium. All Rights Reserved.
//
// Test_PipelineSteps.cpp
// Tests AMapGenerator::ExecuteGenerationStep().

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "AMapGenerator.h"
#include "UMapGeneratorSettings.h"

// -----------------------------------------------------------------------
// Helper: create a minimal valid UMapGeneratorSettings
// -----------------------------------------------------------------------

static UMapGeneratorSettings* MakeMinimalSettings_PS()
{
	UMapGeneratorSettings* S = NewObject<UMapGeneratorSettings>(
		GetTransientPackage(), NAME_None, RF_Transient);
	S->MapSizeX   = 1;
	S->MapSizeY   = 1;
	S->Resolution = 33;
	S->QuadSize   = 100;
	S->SeaLevel   = 0.3f;
	S->Seed       = 42;
	return S;
}

// -----------------------------------------------------------------------
// Test: out-of-range step index returns false
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_PipelineSteps_InvalidIndex,
	"Priordium.MapGenerator.AMapGenerator.PipelineSteps.InvalidIndex",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_PipelineSteps_InvalidIndex::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	if (!World) { AddWarning(TEXT("GWorld is null, skipping")); return true; }

	AMapGenerator* Gen = World->SpawnActor<AMapGenerator>(AMapGenerator::StaticClass());
	if (!TestNotNull(TEXT("AMapGenerator spawned"), Gen)) return true;

	Gen->GeneratorSettings = MakeMinimalSettings_PS();

	AddExpectedError(TEXT("invalid step index"), EAutomationExpectedErrorFlags::Contains, 3);
	TestFalse(TEXT("Step -1 returns false"), Gen->ExecuteGenerationStep(-1));
	TestFalse(TEXT("Step  6 returns false"), Gen->ExecuteGenerationStep(6));
	TestFalse(TEXT("Step 99 returns false"), Gen->ExecuteGenerationStep(99));

	Gen->Destroy();
	return true;
}

// -----------------------------------------------------------------------
// Test: Step 0 (HeightmapGenerator::Generate) succeeds
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_PipelineSteps_Step0_Heightmap,
	"Priordium.MapGenerator.AMapGenerator.PipelineSteps.Step0_Heightmap",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_PipelineSteps_Step0_Heightmap::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	if (!World) { AddWarning(TEXT("GWorld is null, skipping")); return true; }

	AMapGenerator* Gen = World->SpawnActor<AMapGenerator>(AMapGenerator::StaticClass());
	if (!TestNotNull(TEXT("AMapGenerator spawned"), Gen)) return true;

	Gen->GeneratorSettings = MakeMinimalSettings_PS();
	TestTrue(TEXT("Step 0 (Heightmap) succeeds"), Gen->ExecuteGenerationStep(0));

	Gen->Destroy();
	return true;
}

// -----------------------------------------------------------------------
// Test: Steps 0-5 run successfully in sequence
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_PipelineSteps_AllStepsSequential,
	"Priordium.MapGenerator.AMapGenerator.PipelineSteps.AllStepsSequential",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_PipelineSteps_AllStepsSequential::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	if (!World) { AddWarning(TEXT("GWorld is null, skipping")); return true; }

	AMapGenerator* Gen = World->SpawnActor<AMapGenerator>(AMapGenerator::StaticClass());
	if (!TestNotNull(TEXT("AMapGenerator spawned"), Gen)) return true;

	Gen->GeneratorSettings = MakeMinimalSettings_PS();

	for (int32 Step = 0; Step < 6; ++Step)
	{
		const bool bOk = Gen->ExecuteGenerationStep(Step);
		TestTrue(FString::Printf(TEXT("Step %d succeeds"), Step), bOk);
		if (!bOk) break;
	}

	Gen->Destroy();
	return true;
}
