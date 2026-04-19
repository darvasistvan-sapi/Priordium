// Copyright Priordium. All Rights Reserved.
//
// Test_HeightmapConfig.cpp
// Tests the default values, reflection, and Blueprint visibility of the FHeightmapConfig struct.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

#include "FHeightmapConfig.h"

// -----------------------------------------------------------------------
// Test: FHeightmapConfig default values are correct
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_HeightmapConfig_DefaultConstruction,
	"Priordium.MapGenerator.HeightmapConfig.DefaultConstruction",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_HeightmapConfig_DefaultConstruction::RunTest(const FString& Parameters)
{
	FHeightmapConfig Config;

	TestEqual(TEXT("Frequency default value is 0.01"), Config.Frequency, 0.01f);
	TestEqual(TEXT("Amplitude default value is 1.0"),  Config.Amplitude, 1.0f);
	TestEqual(TEXT("Octaves default value is 6"),      Config.Octaves,   6);
	TestEqual(TEXT("Persistence default value is 0.5"),Config.Persistence, 0.5f);
	TestEqual(TEXT("Lacunarity default value is 2.0"), Config.Lacunarity, 2.0f);

	return true;
}

// -----------------------------------------------------------------------
// Test: FHeightmapConfig UStruct exists in the reflection system
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_HeightmapConfig_StructReflection,
	"Priordium.MapGenerator.HeightmapConfig.StructReflection",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_HeightmapConfig_StructReflection::RunTest(const FString& Parameters)
{
	const UScriptStruct* Struct = TBaseStructure<FHeightmapConfig>::Get();
	TestNotNull(TEXT("FHeightmapConfig UScriptStruct exists"), Struct);
	return true;
}

// -----------------------------------------------------------------------
// Test: FHeightmapConfig BlueprintType flag is present
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_HeightmapConfig_BlueprintType,
	"Priordium.MapGenerator.HeightmapConfig.BlueprintType",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_HeightmapConfig_BlueprintType::RunTest(const FString& Parameters)
{
	const UScriptStruct* Struct = TBaseStructure<FHeightmapConfig>::Get();
	if (!TestNotNull(TEXT("FHeightmapConfig UScriptStruct accessible"), Struct))
	{
		return false;
	}
#if WITH_EDITORONLY_DATA
	bool bIsBlueprintType = Struct->HasMetaData(TEXT("BlueprintType"));
	TestTrue(TEXT("FHeightmapConfig BlueprintType flag is present"), bIsBlueprintType);
#else
	TestTrue(TEXT("FHeightmapConfig BlueprintType flag (skipped in non-editor build)"), true);
#endif
	return true;
}

// -----------------------------------------------------------------------
// Test: All UPROPERTYs are present in the struct
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_HeightmapConfig_PropertiesExist,
	"Priordium.MapGenerator.HeightmapConfig.PropertiesExist",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_HeightmapConfig_PropertiesExist::RunTest(const FString& Parameters)
{
	const UScriptStruct* Struct = TBaseStructure<FHeightmapConfig>::Get();
	if (!TestNotNull(TEXT("FHeightmapConfig UScriptStruct accessible"), Struct))
	{
		return false;
	}

	TArray<FString> ExpectedProps = {
		TEXT("Frequency"), TEXT("Amplitude"), TEXT("Octaves"),
		TEXT("Persistence"), TEXT("Lacunarity")
	};

	for (const FString& PropName : ExpectedProps)
	{
		FProperty* Prop = Struct->FindPropertyByName(*PropName);
		TestNotNull(*FString::Printf(TEXT("FHeightmapConfig::%s property exists"), *PropName), Prop);
	}

	return true;
}

// -----------------------------------------------------------------------
// Test: Values are modifiable
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_HeightmapConfig_ValuesModifiable,
	"Priordium.MapGenerator.HeightmapConfig.ValuesModifiable",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_HeightmapConfig_ValuesModifiable::RunTest(const FString& Parameters)
{
	FHeightmapConfig Config;
	Config.Frequency   = 0.05f;
	Config.Amplitude   = 2.0f;
	Config.Octaves     = 8;
	Config.Persistence = 0.7f;
	Config.Lacunarity  = 3.0f;

	TestEqual(TEXT("Frequency is modifiable"), Config.Frequency,   0.05f);
	TestEqual(TEXT("Amplitude is modifiable"), Config.Amplitude,   2.0f);
	TestEqual(TEXT("Octaves is modifiable"),   Config.Octaves,     8);
	TestEqual(TEXT("Persistence is modifiable"), Config.Persistence, 0.7f);
	TestEqual(TEXT("Lacunarity is modifiable"), Config.Lacunarity,  3.0f);

	return true;
}

// -----------------------------------------------------------------------
// Test: Struct is copyable (copy semantics)
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_HeightmapConfig_CopySemantics,
	"Priordium.MapGenerator.HeightmapConfig.CopySemantics",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_HeightmapConfig_CopySemantics::RunTest(const FString& Parameters)
{
	FHeightmapConfig Original;
	Original.Frequency = 0.05f;
	Original.Octaves   = 8;

	FHeightmapConfig Copy = Original;

	TestEqual(TEXT("Copy Frequency matches"), Copy.Frequency, Original.Frequency);
	TestEqual(TEXT("Copy Octaves matches"),   Copy.Octaves,   Original.Octaves);

	// Modification does not affect the original
	Copy.Frequency = 0.99f;
	TestNotEqual(TEXT("Modifying copy does not affect original"), Copy.Frequency, Original.Frequency);

	return true;
}

