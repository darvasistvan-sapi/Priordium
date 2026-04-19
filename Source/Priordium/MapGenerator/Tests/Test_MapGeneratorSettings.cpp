// Copyright Priordium. All Rights Reserved.
//
// Test_MapGeneratorSettings.cpp
// Tests the basic functionality of the UMapGeneratorSettings Data Asset.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "Engine/DataAsset.h"

#include "UMapGeneratorSettings.h"

// -----------------------------------------------------------------------
// Test: UMapGeneratorSettings class exists in the reflection system
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_MapGeneratorSettings_ClassExists,
	"Priordium.MapGenerator.MapGeneratorSettings.ClassExists",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_MapGeneratorSettings_ClassExists::RunTest(const FString& Parameters)
{
	UClass* SettingsClass = UMapGeneratorSettings::StaticClass();
	TestNotNull(TEXT("UMapGeneratorSettings class exists"), SettingsClass);
	return true;
}

// -----------------------------------------------------------------------
// Test: UMapGeneratorSettings inherits from UDataAsset
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_MapGeneratorSettings_InheritsDataAsset,
	"Priordium.MapGenerator.MapGeneratorSettings.InheritsDataAsset",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_MapGeneratorSettings_InheritsDataAsset::RunTest(const FString& Parameters)
{
	UClass* SettingsClass = UMapGeneratorSettings::StaticClass();
	if (!TestNotNull(TEXT("UMapGeneratorSettings class is accessible"), SettingsClass))
	{
		return false;
	}
	bool bIsDataAsset = SettingsClass->IsChildOf(UDataAsset::StaticClass());
	TestTrue(TEXT("UMapGeneratorSettings inherits from UDataAsset"), bIsDataAsset);
	return true;
}

// -----------------------------------------------------------------------
// Test: UMapGeneratorSettings BlueprintType flag is present
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_MapGeneratorSettings_BlueprintType,
	"Priordium.MapGenerator.MapGeneratorSettings.BlueprintType",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_MapGeneratorSettings_BlueprintType::RunTest(const FString& Parameters)
{
	UClass* SettingsClass = UMapGeneratorSettings::StaticClass();
	if (!TestNotNull(TEXT("UMapGeneratorSettings class is accessible"), SettingsClass))
	{
		return false;
	}
	bool bIsBlueprintType = SettingsClass->HasMetaData(TEXT("BlueprintType"));
	TestTrue(TEXT("UMapGeneratorSettings BlueprintType flag is present"), bIsBlueprintType);
	return true;
}

// -----------------------------------------------------------------------
// Test: Default values are correct
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_MapGeneratorSettings_DefaultValues,
	"Priordium.MapGenerator.MapGeneratorSettings.DefaultValues",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_MapGeneratorSettings_DefaultValues::RunTest(const FString& Parameters)
{
	UMapGeneratorSettings* Settings = NewObject<UMapGeneratorSettings>();
	if (!TestNotNull(TEXT("UMapGeneratorSettings can be instantiated"), Settings))
	{
		return false;
	}

	// General
	TestEqual(TEXT("Seed default value is 0"),        Settings->Seed,       0);
	TestEqual(TEXT("MapSizeX default value is 512"),  Settings->MapSizeX,   512);
	TestEqual(TEXT("MapSizeY default value is 512"),  Settings->MapSizeY,   512);
	TestEqual(TEXT("Resolution default value is 513"),Settings->Resolution, 513);

	// Biomes
	TestEqual(TEXT("ClimateZone default value is Temperate"),
		Settings->ClimateZone, EClimateZone::Temperate);
	TestEqual(TEXT("SeaLevel default value is 0.3"),           Settings->SeaLevel,          0.3f);
	TestEqual(TEXT("MountainThreshold default value is 0.75"), Settings->MountainThreshold, 0.75f);

	// Water
	TestTrue (TEXT("bGenerateRivers default is true"),   Settings->bGenerateRivers);
	TestEqual(TEXT("MaxRiverCount default value is 5"),  Settings->MaxRiverCount, 5);
	TestTrue (TEXT("bGenerateLakes default is true"),    Settings->bGenerateLakes);
	TestEqual(TEXT("MaxLakeCount default value is 10"),  Settings->MaxLakeCount,  10);

	// Resources
	TestEqual(TEXT("ResourceSpawnRules default is empty array"),
		Settings->ResourceSpawnRules.Num(), 0);

	return true;
}

// -----------------------------------------------------------------------
// Test: Heightmap configuration default values are correct
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_MapGeneratorSettings_HeightmapConfigDefaults,
	"Priordium.MapGenerator.MapGeneratorSettings.HeightmapConfigDefaults",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_MapGeneratorSettings_HeightmapConfigDefaults::RunTest(const FString& Parameters)
{
	UMapGeneratorSettings* Settings = NewObject<UMapGeneratorSettings>();
	if (!TestNotNull(TEXT("UMapGeneratorSettings can be instantiated"), Settings))
	{
		return false;
	}

	TestEqual(TEXT("HeightmapConfig.Frequency default is 0.01"),   Settings->HeightmapConfig.Frequency,   0.01f);
	TestEqual(TEXT("HeightmapConfig.Amplitude default is 1.0"),    Settings->HeightmapConfig.Amplitude,   1.0f);
	TestEqual(TEXT("HeightmapConfig.Octaves default is 6"),        Settings->HeightmapConfig.Octaves,     6);
	TestEqual(TEXT("HeightmapConfig.Persistence default is 0.5"),  Settings->HeightmapConfig.Persistence, 0.5f);
	TestEqual(TEXT("HeightmapConfig.Lacunarity default is 2.0"),   Settings->HeightmapConfig.Lacunarity,  2.0f);

	return true;
}

// -----------------------------------------------------------------------
// Test: All UPROPERTYs exist in the General category
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_MapGeneratorSettings_GeneralPropertiesExist,
	"Priordium.MapGenerator.MapGeneratorSettings.GeneralPropertiesExist",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_MapGeneratorSettings_GeneralPropertiesExist::RunTest(const FString& Parameters)
{
	UClass* SettingsClass = UMapGeneratorSettings::StaticClass();
	if (!TestNotNull(TEXT("UMapGeneratorSettings class is accessible"), SettingsClass))
	{
		return false;
	}

	TArray<FString> GeneralProps = { TEXT("Seed"), TEXT("MapSizeX"), TEXT("MapSizeY"), TEXT("Resolution") };
	for (const FString& PropName : GeneralProps)
	{
		FProperty* Prop = SettingsClass->FindPropertyByName(*PropName);
		TestNotNull(*FString::Printf(TEXT("UMapGeneratorSettings::%s property exists"), *PropName), Prop);
	}
	return true;
}

// -----------------------------------------------------------------------
// Test: All UPROPERTYs exist in the other categories
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_MapGeneratorSettings_AllPropertiesExist,
	"Priordium.MapGenerator.MapGeneratorSettings.AllPropertiesExist",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_MapGeneratorSettings_AllPropertiesExist::RunTest(const FString& Parameters)
{
	UClass* SettingsClass = UMapGeneratorSettings::StaticClass();
	if (!TestNotNull(TEXT("UMapGeneratorSettings class is accessible"), SettingsClass))
	{
		return false;
	}

	TArray<FString> ExpectedProps = {
		// General
		TEXT("Seed"), TEXT("MapSizeX"), TEXT("MapSizeY"), TEXT("Resolution"),
		// Heightmap
		TEXT("HeightmapConfig"),
		// Biomes
		TEXT("ClimateZone"), TEXT("SeaLevel"), TEXT("MountainThreshold"),
		// Water
		TEXT("bGenerateRivers"), TEXT("MaxRiverCount"), TEXT("bGenerateLakes"), TEXT("MaxLakeCount"),
		// Resources
		TEXT("ResourceSpawnRules")
	};

	for (const FString& PropName : ExpectedProps)
	{
		FProperty* Prop = SettingsClass->FindPropertyByName(*PropName);
		TestNotNull(*FString::Printf(TEXT("UMapGeneratorSettings::%s property exists"), *PropName), Prop);
	}
	return true;
}

// -----------------------------------------------------------------------
// Test: Values are modifiable
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_MapGeneratorSettings_ValuesModifiable,
	"Priordium.MapGenerator.MapGeneratorSettings.ValuesModifiable",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_MapGeneratorSettings_ValuesModifiable::RunTest(const FString& Parameters)
{
	UMapGeneratorSettings* Settings = NewObject<UMapGeneratorSettings>();
	if (!TestNotNull(TEXT("UMapGeneratorSettings can be instantiated"), Settings))
	{
		return false;
	}

	Settings->Seed       = 12345;
	Settings->MapSizeX   = 1024;
	Settings->MapSizeY   = 1024;
	Settings->Resolution = 1025;
	Settings->SeaLevel   = 0.4f;
	Settings->HeightmapConfig.Octaves = 8;

	TestEqual(TEXT("Seed is modifiable"),       Settings->Seed,       12345);
	TestEqual(TEXT("MapSizeX is modifiable"),   Settings->MapSizeX,   1024);
	TestEqual(TEXT("MapSizeY is modifiable"),   Settings->MapSizeY,   1024);
	TestEqual(TEXT("Resolution is modifiable"), Settings->Resolution, 1025);
	TestEqual(TEXT("SeaLevel is modifiable"),   Settings->SeaLevel,   0.4f);
	TestEqual(TEXT("HeightmapConfig.Octaves is modifiable"), Settings->HeightmapConfig.Octaves, 8);

	return true;
}

