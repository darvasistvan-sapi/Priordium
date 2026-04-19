// Copyright Priordium. All Rights Reserved.
//
// Test_WaterDistanceFilter.cpp
// Tests CheckWaterDistanceFilter() of UResourceDistributor.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UResourceDistributor.h"
#include "UWaterSystemBuilder.h"
#include "UHeightmapGenerator.h"
#include "UMapGeneratorSettings.h"
#include "FResourceSpawnRule.h"
#include "FWaterBodyDefinition.h"

// Helper: build a WaterSystemBuilder with a small heightmap.
// Rivers and lakes are enabled so at least one water body is generated (ocean removed from pipeline).
static UWaterSystemBuilder* MakeWater(int32 Res = 65, int32 Seed = 42)
{
	UMapGeneratorSettings* S = NewObject<UMapGeneratorSettings>(GetTransientPackage(), NAME_None, RF_Transient);
	S->Resolution = Res; S->Seed = Seed;
	S->bUseContinentMask = false;
	S->HeightmapConfig.Octaves = 6;
	S->HeightmapConfig.Frequency = 0.4f;
	S->HeightmapConfig.Amplitude = 1.0f;
	S->HeightmapConfig.Persistence = 0.5f;
	S->HeightmapConfig.Lacunarity = 2.0f;
	S->SeaLevel = 0.4f;
	S->bGenerateRivers = true;
	S->MaxRiverCount   = 3;
	S->bGenerateLakes  = true;
	S->MaxLakeCount    = 3;

	UHeightmapGenerator* Gen = NewObject<UHeightmapGenerator>(GetTransientPackage(), NAME_None, RF_Transient);
	Gen->Generate(S);

	UWaterSystemBuilder* Water = NewObject<UWaterSystemBuilder>(GetTransientPackage(), NAME_None, RF_Transient);
	Water->BuildWaterBodies(Gen, S, 100.0f);
	return Water;
}

// -----------------------------------------------------------------------
// Test: Null WaterSystem â†’ always pass
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_WaterDistanceFilter_NullWaterSystem,
	"Priordium.MapGenerator.ResourceDistributor.WaterDistanceFilter.NullWaterSystem",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_WaterDistanceFilter_NullWaterSystem::RunTest(const FString& Parameters)
{
	UResourceDistributor* Dist = NewObject<UResourceDistributor>(GetTransientPackage(), NAME_None, RF_Transient);
	FResourceSpawnRule Rule;
	Rule.MinWaterDistance = 100.0f;
	Rule.MaxWaterDistance = 5000.0f;

	TestTrue(TEXT("Null WaterSystem: filter always passes"),
		Dist->CheckWaterDistanceFilter(Rule, nullptr, FVector2D::ZeroVector));
	return true;
}

// -----------------------------------------------------------------------
// Test: No maximum constraint (MaxWaterDistance < 0) â†’ passes if min is met
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_WaterDistanceFilter_NoMaxLimit,
	"Priordium.MapGenerator.ResourceDistributor.WaterDistanceFilter.NoMaxLimit",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_WaterDistanceFilter_NoMaxLimit::RunTest(const FString& Parameters)
{
	UResourceDistributor* Dist = NewObject<UResourceDistributor>(GetTransientPackage(), NAME_None, RF_Transient);
	UWaterSystemBuilder*  Water = MakeWater();

	FResourceSpawnRule Rule;
	Rule.MinWaterDistance = 0.0f;
	Rule.MaxWaterDistance = -1.0f; // No maximum

	// Very far point should still pass when MaxWaterDistance is negative
	const FVector2D FarPoint(1e6f, 1e6f);
	TestTrue(TEXT("No max limit: very far point passes"),
		Dist->CheckWaterDistanceFilter(Rule, Water, FarPoint));
	return true;
}

// -----------------------------------------------------------------------
// Test: Point exactly at a water spline point (distance ~0) passes
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_WaterDistanceFilter_AtWaterPasses,
	"Priordium.MapGenerator.ResourceDistributor.WaterDistanceFilter.AtWaterPasses",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_WaterDistanceFilter_AtWaterPasses::RunTest(const FString& Parameters)
{
	UResourceDistributor* Dist = NewObject<UResourceDistributor>(GetTransientPackage(), NAME_None, RF_Transient);
	UWaterSystemBuilder*  Water = MakeWater();

	if (!TestTrue(TEXT("Water is initialized"), Water->IsInitialized())) return false;

	// Use the first spline point of the first water body -- guaranteed distance = 0.
	const TArray<FWaterBodyDefinition>& Defs = Water->GetWaterBodyDefinitions();
	const FVector FirstPt = Defs[0].SplinePoints[0];
	const FVector2D AtWater(FirstPt.X, FirstPt.Y);

	FResourceSpawnRule Rule;
	Rule.MinWaterDistance = 0.0f;
	Rule.MaxWaterDistance = 500.0f;

	TestTrue(TEXT("Point at first spline point passes when MaxWaterDistance is sufficient"),
		Dist->CheckWaterDistanceFilter(Rule, Water, AtWater));
	return true;
}

// -----------------------------------------------------------------------
// Test: MaxWaterDistance too small -> point far from water fails
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_WaterDistanceFilter_TooFarFails,
	"Priordium.MapGenerator.ResourceDistributor.WaterDistanceFilter.TooFarFails",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_WaterDistanceFilter_TooFarFails::RunTest(const FString& Parameters)
{
	UResourceDistributor* Dist = NewObject<UResourceDistributor>(GetTransientPackage(), NAME_None, RF_Transient);
	UWaterSystemBuilder*  Water = MakeWater();

	if (!TestTrue(TEXT("Water is initialized"), Water->IsInitialized())) return false;

	// A point very far from the map origin is guaranteed to exceed any reasonable MaxWaterDistance.
	FResourceSpawnRule Rule;
	Rule.MinWaterDistance = 0.0f;
	Rule.MaxWaterDistance = 10.0f;

	const FVector2D FarPoint(1e6f, 1e6f);
	TestFalse(TEXT("Point 1M units away fails with MaxWaterDistance=10"),
		Dist->CheckWaterDistanceFilter(Rule, Water, FarPoint));
	return true;
}
