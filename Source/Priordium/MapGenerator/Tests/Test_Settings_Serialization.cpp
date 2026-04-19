// Copyright Priordium. All Rights Reserved.
//
// Test_Settings_Serialization.cpp
// Tests the serializability and copyability of UMapGeneratorSettings.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UObject/Class.h"

#include "UMapGeneratorSettings.h"

// -----------------------------------------------------------------------
// Test: Two UMapGeneratorSettings instances are independent
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_Settings_Serialization_TwoInstancesIndependent,
	"Priordium.MapGenerator.Settings.Serialization.TwoInstancesIndependent",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_Settings_Serialization_TwoInstancesIndependent::RunTest(const FString& Parameters)
{
	UMapGeneratorSettings* SettingsA = NewObject<UMapGeneratorSettings>();
	UMapGeneratorSettings* SettingsB = NewObject<UMapGeneratorSettings>();

	if (!TestNotNull(TEXT("SettingsA can be instantiated"), SettingsA)) return false;
	if (!TestNotNull(TEXT("SettingsB can be instantiated"), SettingsB)) return false;

	SettingsA->Seed = 111;
	SettingsB->Seed = 222;

	TestNotEqual(TEXT("Two instances have independent Seed values"), SettingsA->Seed, SettingsB->Seed);
	return true;
}

// -----------------------------------------------------------------------
// Test: ResourceSpawnRules array is manageable
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_Settings_Serialization_ResourceRulesArray,
	"Priordium.MapGenerator.Settings.Serialization.ResourceRulesArray",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_Settings_Serialization_ResourceRulesArray::RunTest(const FString& Parameters)
{
	UMapGeneratorSettings* Settings = NewObject<UMapGeneratorSettings>();
	if (!TestNotNull(TEXT("Settings can be instantiated"), Settings)) return false;

	TestEqual(TEXT("ResourceSpawnRules is initially empty"), Settings->ResourceSpawnRules.Num(), 0);

	FResourceSpawnRule IronRule;
	IronRule.Density = 0.3f;
	IronRule.AllowedBiomes.Add(EBiomeType::Mountain);
	IronRule.AllowedBiomes.Add(EBiomeType::Hills);

	Settings->ResourceSpawnRules.Add(IronRule);
	TestEqual(TEXT("ResourceSpawnRules contains 1 element"), Settings->ResourceSpawnRules.Num(), 1);
	TestEqual(TEXT("First element Density is correct"), Settings->ResourceSpawnRules[0].Density, 0.3f);
	TestEqual(TEXT("First element AllowedBiomes count is 2"), Settings->ResourceSpawnRules[0].AllowedBiomes.Num(), 2);

	FResourceSpawnRule WoodRule;
	WoodRule.Density = 0.8f;
	WoodRule.AllowedBiomes.Add(EBiomeType::Forest);

	Settings->ResourceSpawnRules.Add(WoodRule);
	TestEqual(TEXT("ResourceSpawnRules contains 2 elements"), Settings->ResourceSpawnRules.Num(), 2);
	TestEqual(TEXT("Second element Density is correct"), Settings->ResourceSpawnRules[1].Density, 0.8f);

	return true;
}

// -----------------------------------------------------------------------
// Test: UMapGeneratorSettings deep copy (CopyPropertiesForUnrelatedObjects)
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_Settings_Serialization_DeepCopy,
	"Priordium.MapGenerator.Settings.Serialization.DeepCopy",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_Settings_Serialization_DeepCopy::RunTest(const FString& Parameters)
{
	UMapGeneratorSettings* Original = NewObject<UMapGeneratorSettings>();
	if (!TestNotNull(TEXT("Original can be instantiated"), Original)) return false;

	Original->Seed                   = 99999;
	Original->MapSizeX               = 2048;
	Original->SeaLevel               = 0.25f;
	Original->HeightmapConfig.Octaves = 4;
	Original->bGenerateRivers        = false;
	Original->MaxLakeCount           = 20;

	UMapGeneratorSettings* Copy = NewObject<UMapGeneratorSettings>();
	if (!TestNotNull(TEXT("Copy can be instantiated"), Copy)) return false;

	UEngine::CopyPropertiesForUnrelatedObjects(Original, Copy);

	TestEqual(TEXT("Copy.Seed matches"),                   Copy->Seed,                   Original->Seed);
	TestEqual(TEXT("Copy.MapSizeX matches"),               Copy->MapSizeX,               Original->MapSizeX);
	TestEqual(TEXT("Copy.SeaLevel matches"),               Copy->SeaLevel,               Original->SeaLevel);
	TestEqual(TEXT("Copy.HeightmapConfig.Octaves matches"),Copy->HeightmapConfig.Octaves,Original->HeightmapConfig.Octaves);
	TestEqual(TEXT("Copy.bGenerateRivers matches"),        Copy->bGenerateRivers,        Original->bGenerateRivers);
	TestEqual(TEXT("Copy.MaxLakeCount matches"),           Copy->MaxLakeCount,           Original->MaxLakeCount);

	// Independent copy: modifying the original does not affect the copy
	Original->Seed = 1;
	TestNotEqual(TEXT("Copy.Seed did not change after Original was modified"), Copy->Seed, Original->Seed);

	return true;
}

// -----------------------------------------------------------------------
// Test: Heightmap nested struct values are preserved after deep copy
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_Settings_Serialization_NestedStructDeepCopy,
	"Priordium.MapGenerator.Settings.Serialization.NestedStructDeepCopy",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_Settings_Serialization_NestedStructDeepCopy::RunTest(const FString& Parameters)
{
	UMapGeneratorSettings* Original = NewObject<UMapGeneratorSettings>();
	if (!TestNotNull(TEXT("Original can be instantiated"), Original)) return false;

	Original->HeightmapConfig.Frequency   = 0.05f;
	Original->HeightmapConfig.Amplitude   = 3.0f;
	Original->HeightmapConfig.Octaves     = 7;
	Original->HeightmapConfig.Persistence = 0.6f;
	Original->HeightmapConfig.Lacunarity  = 2.5f;

	UMapGeneratorSettings* Copy = NewObject<UMapGeneratorSettings>();
	if (!TestNotNull(TEXT("Copy can be instantiated"), Copy)) return false;

	UEngine::CopyPropertiesForUnrelatedObjects(Original, Copy);

	TestEqual(TEXT("HeightmapConfig.Frequency preserved"), Copy->HeightmapConfig.Frequency,   0.05f);
	TestEqual(TEXT("HeightmapConfig.Amplitude preserved"), Copy->HeightmapConfig.Amplitude,   3.0f);
	TestEqual(TEXT("HeightmapConfig.Octaves preserved"),   Copy->HeightmapConfig.Octaves,     7);
	TestEqual(TEXT("HeightmapConfig.Persistence preserved"),Copy->HeightmapConfig.Persistence,0.6f);
	TestEqual(TEXT("HeightmapConfig.Lacunarity preserved"), Copy->HeightmapConfig.Lacunarity,  2.5f);

	return true;
}

