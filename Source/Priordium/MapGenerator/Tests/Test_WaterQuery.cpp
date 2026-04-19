// Copyright Priordium. All Rights Reserved.
//
// Test_WaterQuery.cpp
// Tests BuildWaterBodies(), IsWaterAt(), GetNearestWaterDistance().

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UWaterSystemBuilder.h"
#include "UHeightmapGenerator.h"
#include "UMapGeneratorSettings.h"

static void MakeQueryTestPair(
	UHeightmapGenerator*&   OutH,
	UMapGeneratorSettings*& OutS)
{
	OutS = NewObject<UMapGeneratorSettings>(GetTransientPackage(), NAME_None, RF_Transient);
	OutS->Resolution                  = 65;
	OutS->Seed                        = 42;
	OutS->bUseContinentMask           = true;
	OutS->EdgeFalloffDistance         = 0.3f;
	OutS->SeaLevel                    = 0.3f;
	OutS->MaxRiverCount               = 3;
	OutS->MaxLakeCount                = 3;
	OutS->bGenerateRivers             = true;
	OutS->bGenerateLakes              = true;
	OutS->HeightmapConfig.Octaves     = 4;
	OutS->HeightmapConfig.Frequency   = 0.05f;
	OutS->HeightmapConfig.Amplitude   = 1.0f;
	OutS->HeightmapConfig.Persistence = 0.5f;
	OutS->HeightmapConfig.Lacunarity  = 2.0f;

	OutH = NewObject<UHeightmapGenerator>(GetTransientPackage(), NAME_None, RF_Transient);
	OutH->Generate(OutS);
}

// -----------------------------------------------------------------------
// Test: BuildWaterBodies returns true with valid inputs
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_WaterQuery_BuildReturnsTrue,
	"Priordium.MapGenerator.WaterSystemBuilder.WaterQuery.BuildReturnsTrue",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_WaterQuery_BuildReturnsTrue::RunTest(const FString& Parameters)
{
	UHeightmapGenerator*   Heightmap;
	UMapGeneratorSettings* Settings;
	MakeQueryTestPair(Heightmap, Settings);

	UWaterSystemBuilder* Builder = NewObject<UWaterSystemBuilder>(
		GetTransientPackage(), NAME_None, RF_Transient);

	bool bOK = Builder->BuildWaterBodies(Heightmap, Settings, 100.0f);
	TestTrue(TEXT("BuildWaterBodies returns true"), bOK);
	return true;
}

// -----------------------------------------------------------------------
// Test: BuildWaterBodies populates WaterBodyDefinitions
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_WaterQuery_DefinitionsPopulated,
	"Priordium.MapGenerator.WaterSystemBuilder.WaterQuery.DefinitionsPopulated",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_WaterQuery_DefinitionsPopulated::RunTest(const FString& Parameters)
{
	UHeightmapGenerator*   Heightmap;
	UMapGeneratorSettings* Settings;
	MakeQueryTestPair(Heightmap, Settings);

	UWaterSystemBuilder* Builder = NewObject<UWaterSystemBuilder>(
		GetTransientPackage(), NAME_None, RF_Transient);
	Builder->BuildWaterBodies(Heightmap, Settings, 100.0f);

	TestTrue(TEXT("WaterBodyDefinitions is not empty"), Builder->GetWaterBodyDefinitions().Num() > 0);
	TestTrue(TEXT("IsInitialized() is true"), Builder->IsInitialized());
	return true;
}

// -----------------------------------------------------------------------
// Test: IsWaterAt returns false before BuildWaterBodies
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_WaterQuery_IsWaterAt_FalseBeforeBuild,
	"Priordium.MapGenerator.WaterSystemBuilder.WaterQuery.IsWaterAt_FalseBeforeBuild",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_WaterQuery_IsWaterAt_FalseBeforeBuild::RunTest(const FString& Parameters)
{
	UWaterSystemBuilder* Builder = NewObject<UWaterSystemBuilder>(
		GetTransientPackage(), NAME_None, RF_Transient);

	TestFalse(TEXT("IsWaterAt returns false before BuildWaterBodies"),
		Builder->IsWaterAt(FVector2D::ZeroVector, 10000.0f));
	return true;
}

// -----------------------------------------------------------------------
// Test: IsWaterAt returns true at an ocean SplinePoint location
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_WaterQuery_IsWaterAt_TrueAtOceanPoint,
	"Priordium.MapGenerator.WaterSystemBuilder.WaterQuery.IsWaterAt_TrueAtOceanPoint",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_WaterQuery_IsWaterAt_TrueAtOceanPoint::RunTest(const FString& Parameters)
{
	UHeightmapGenerator*   Heightmap;
	UMapGeneratorSettings* Settings;
	MakeQueryTestPair(Heightmap, Settings);

	UWaterSystemBuilder* Builder = NewObject<UWaterSystemBuilder>(
		GetTransientPackage(), NAME_None, RF_Transient);
	Builder->BuildWaterBodies(Heightmap, Settings, 100.0f);

	// Find the first ocean SplinePoint and query exactly there
	for (const FWaterBodyDefinition& Def : Builder->GetWaterBodyDefinitions())
	{
		if (Def.WaterType == EWaterBodyType::Ocean && Def.SplinePoints.Num() > 0)
		{
			const FVector& SP = Def.SplinePoints[0];
			TestTrue(TEXT("IsWaterAt returns true at ocean SplinePoint"),
				Builder->IsWaterAt(FVector2D(SP.X, SP.Y), 1.0f));
			return true;
		}
	}

	AddWarning(TEXT("No ocean SplinePoints found, test inconclusive"));
	return true;
}

// -----------------------------------------------------------------------
// Test: GetNearestWaterDistance returns -1 before BuildWaterBodies
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_WaterQuery_GetNearestDist_MinusOneBeforeBuild,
	"Priordium.MapGenerator.WaterSystemBuilder.WaterQuery.GetNearestDist_MinusOneBeforeBuild",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_WaterQuery_GetNearestDist_MinusOneBeforeBuild::RunTest(const FString& Parameters)
{
	UWaterSystemBuilder* Builder = NewObject<UWaterSystemBuilder>(
		GetTransientPackage(), NAME_None, RF_Transient);

	TestEqual(TEXT("GetNearestWaterDistance returns -1 before BuildWaterBodies"),
		Builder->GetNearestWaterDistance(FVector2D::ZeroVector), -1.0f);
	return true;
}

// -----------------------------------------------------------------------
// Test: GetNearestWaterDistance returns 0 at a known SplinePoint
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_WaterQuery_GetNearestDist_ZeroAtSplinePoint,
	"Priordium.MapGenerator.WaterSystemBuilder.WaterQuery.GetNearestDist_ZeroAtSplinePoint",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_WaterQuery_GetNearestDist_ZeroAtSplinePoint::RunTest(const FString& Parameters)
{
	UHeightmapGenerator*   Heightmap;
	UMapGeneratorSettings* Settings;
	MakeQueryTestPair(Heightmap, Settings);

	UWaterSystemBuilder* Builder = NewObject<UWaterSystemBuilder>(
		GetTransientPackage(), NAME_None, RF_Transient);
	Builder->BuildWaterBodies(Heightmap, Settings, 100.0f);

	// Query exactly at the first available SplinePoint
	for (const FWaterBodyDefinition& Def : Builder->GetWaterBodyDefinitions())
	{
		if (Def.SplinePoints.Num() > 0)
		{
			const FVector& SP = Def.SplinePoints[0];
			const float Dist  = Builder->GetNearestWaterDistance(FVector2D(SP.X, SP.Y));
			TestTrue(TEXT("Distance at SplinePoint is ~0"), FMath::IsNearlyZero(Dist, 1e-3f));
			return true;
		}
	}

	AddWarning(TEXT("No SplinePoints found, test inconclusive"));
	return true;
}

// -----------------------------------------------------------------------
// Test: Null Heightmap -> BuildWaterBodies returns false
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_WaterQuery_NullHeightmap_ReturnsFalse,
	"Priordium.MapGenerator.WaterSystemBuilder.WaterQuery.NullHeightmap_ReturnsFalse",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_WaterQuery_NullHeightmap_ReturnsFalse::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("BuildWaterBodies -- Heightmap is null"), EAutomationExpectedErrorFlags::Contains, 1);

	UWaterSystemBuilder* Builder = NewObject<UWaterSystemBuilder>(
		GetTransientPackage(), NAME_None, RF_Transient);
	UMapGeneratorSettings* Settings = NewObject<UMapGeneratorSettings>(
		GetTransientPackage(), NAME_None, RF_Transient);

	TestFalse(TEXT("Null Heightmap -> BuildWaterBodies false"),
		Builder->BuildWaterBodies(nullptr, Settings, 100.0f));
	return true;
}
