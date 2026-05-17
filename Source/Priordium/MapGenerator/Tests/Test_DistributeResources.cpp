// Copyright Priordium. All Rights Reserved.
//
// Test_DistributeResources.cpp
// Tests DistributeResources() of UResourceDistributor.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UResourceDistributor.h"
#include "UHeightmapGenerator.h"
#include "UBiomeManager.h"
#include "UMapGeneratorSettings.h"
#include "FResourceSpawnRule.h"

// -----------------------------------------------------------------------
// Test: Null Settings â†’ returns false
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_DistributeResources_NullSettings,
	"Priordium.MapGenerator.ResourceDistributor.DistributeResources.NullSettings",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_DistributeResources_NullSettings::RunTest(const FString& Parameters)
{
	UResourceDistributor* Dist = NewObject<UResourceDistributor>(GetTransientPackage(), NAME_None, RF_Transient);

	AddExpectedError(TEXT("Settings is null"), EAutomationExpectedErrorFlags::Contains, 1);
	const bool bResult = Dist->DistributeResources(nullptr, nullptr, nullptr, nullptr, nullptr);
	TestFalse(TEXT("Null Settings returns false"), bResult);
	return true;
}

// -----------------------------------------------------------------------
// Test: Null Heightmap â†’ returns false
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_DistributeResources_NullHeightmap,
	"Priordium.MapGenerator.ResourceDistributor.DistributeResources.NullHeightmap",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_DistributeResources_NullHeightmap::RunTest(const FString& Parameters)
{
	UMapGeneratorSettings* S = NewObject<UMapGeneratorSettings>(GetTransientPackage(), NAME_None, RF_Transient);
	UResourceDistributor* Dist = NewObject<UResourceDistributor>(GetTransientPackage(), NAME_None, RF_Transient);

	AddExpectedError(TEXT("Heightmap is null"), EAutomationExpectedErrorFlags::Contains, 1);
	const bool bResult = Dist->DistributeResources(S, nullptr, nullptr, nullptr, nullptr);
	TestFalse(TEXT("Null Heightmap returns false"), bResult);
	return true;
}

// -----------------------------------------------------------------------
// Test: Empty ResourceSpawnRules â†’ returns false
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_DistributeResources_EmptyRules,
	"Priordium.MapGenerator.ResourceDistributor.DistributeResources.EmptyRules",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_DistributeResources_EmptyRules::RunTest(const FString& Parameters)
{
	UMapGeneratorSettings* S = NewObject<UMapGeneratorSettings>(GetTransientPackage(), NAME_None, RF_Transient);
	S->Resolution = 33; S->Seed = 42;
	S->bUseContinentMask = false;
	S->HeightmapConfig.Octaves = 4;
	S->HeightmapConfig.Frequency = 0.05f;
	S->HeightmapConfig.Amplitude = 1.0f;
	S->HeightmapConfig.Persistence = 0.5f;
	S->HeightmapConfig.Lacunarity = 2.0f;
	// ResourceSpawnRules is empty by default

	UHeightmapGenerator* Gen = NewObject<UHeightmapGenerator>(GetTransientPackage(), NAME_None, RF_Transient);
	Gen->Generate(S);

	UBiomeManager* Biomes = NewObject<UBiomeManager>(GetTransientPackage(), NAME_None, RF_Transient);
	Biomes->AssignBiomes(S, Gen);

	UResourceDistributor* Dist = NewObject<UResourceDistributor>(GetTransientPackage(), NAME_None, RF_Transient);

	// IsRegex=false: the pattern contains parentheses, so regex interpretation is disabled.
	AddExpectedError(TEXT("GetWorld() returned null"), EAutomationExpectedErrorFlags::Contains, 1, false);
	const bool bResult = Dist->DistributeResources(S, Gen, Biomes, nullptr, nullptr);
	TestFalse(TEXT("Empty ResourceSpawnRules returns false"), bResult);
	return true;
}

// -----------------------------------------------------------------------
// Test: Invalid ResourceTag in rule â†’ skipped, returns false
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_DistributeResources_InvalidTag_Skipped,
	"Priordium.MapGenerator.ResourceDistributor.DistributeResources.InvalidTag_Skipped",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_DistributeResources_InvalidTag_Skipped::RunTest(const FString& Parameters)
{
	UMapGeneratorSettings* S = NewObject<UMapGeneratorSettings>(GetTransientPackage(), NAME_None, RF_Transient);
	S->Resolution = 33; S->Seed = 42;
	S->bUseContinentMask = false;
	S->HeightmapConfig.Octaves = 4;
	S->HeightmapConfig.Frequency = 0.05f;
	S->HeightmapConfig.Amplitude = 1.0f;
	S->HeightmapConfig.Persistence = 0.5f;
	S->HeightmapConfig.Lacunarity = 2.0f;

	FResourceSpawnRule Rule;
	// ResourceTag is invalid (default-constructed)
	Rule.ActorClasses.Add(TSoftClassPtr<AActor>(AActor::StaticClass()));
	Rule.Density    = 1.0f;
	S->ResourceSpawnRules.Add(Rule);

	UHeightmapGenerator* Gen = NewObject<UHeightmapGenerator>(GetTransientPackage(), NAME_None, RF_Transient);
	Gen->Generate(S);

	UBiomeManager* Biomes = NewObject<UBiomeManager>(GetTransientPackage(), NAME_None, RF_Transient);
	Biomes->AssignBiomes(S, Gen);

	UResourceDistributor* Dist = NewObject<UResourceDistributor>(GetTransientPackage(), NAME_None, RF_Transient);

	// IsRegex=false: the pattern contains parentheses, so regex interpretation is disabled.
	AddExpectedError(TEXT("GetWorld() returned null"), EAutomationExpectedErrorFlags::Contains, 1, false);
	const bool bResult = Dist->DistributeResources(S, Gen, Biomes, nullptr, nullptr);
	TestFalse(TEXT("Rule with invalid ResourceTag is skipped, no actors spawned"), bResult);
	TestEqual(TEXT("SpawnedResources is empty when rule is skipped"), Dist->GetSpawnedResources().Num(), 0);
	return true;
}
