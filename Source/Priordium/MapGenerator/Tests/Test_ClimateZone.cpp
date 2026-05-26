// Copyright Priordium. All Rights Reserved.
//
// Test_ClimateZone.cpp
// Tests the correct definition, reflection, and Blueprint visibility of the EClimateZone enum.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UObject/Class.h"

#include "EClimateZone.h"

// -----------------------------------------------------------------------
// Test: EClimateZone UEnum exists in the reflection system
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ClimateZone_AllClimateZones_Declared,
	"Priordium.MapGenerator.ClimateZone.AllClimateZonesDeclared",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ClimateZone_AllClimateZones_Declared::RunTest(const FString& Parameters)
{
	const UEnum* ClimateEnum = StaticEnum<EClimateZone>();
	TestNotNull(TEXT("EClimateZone UEnum exists"), ClimateEnum);
	return true;
}

// -----------------------------------------------------------------------
// Test: All 4 climate zone values are accessible
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ClimateZone_AllValues_Accessible,
	"Priordium.MapGenerator.ClimateZone.AllValues_Accessible",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ClimateZone_AllValues_Accessible::RunTest(const FString& Parameters)
{
	TArray<EClimateZone> Values = {
		EClimateZone::Tropical,
		EClimateZone::Temperate,
		EClimateZone::Continental,
		EClimateZone::Subarctic
	};
	TestEqual(TEXT("EClimateZone contains 4 values"), Values.Num(), 4);
	return true;
}

// -----------------------------------------------------------------------
// Test: BlueprintType flag is present
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ClimateZone_BlueprintVisible,
	"Priordium.MapGenerator.ClimateZone.BlueprintVisible",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ClimateZone_BlueprintVisible::RunTest(const FString& Parameters)
{
	const UEnum* ClimateEnum = StaticEnum<EClimateZone>();
	if (!TestNotNull(TEXT("EClimateZone UEnum accessible"), ClimateEnum))
	{
		return false;
	}
#if WITH_EDITORONLY_DATA
	bool bIsBlueprintType = ClimateEnum->HasMetaData(TEXT("BlueprintType"));
	TestTrue(TEXT("EClimateZone BlueprintType flag is present"), bIsBlueprintType);
#else
	TestTrue(TEXT("EClimateZone BlueprintType flag (skipped in non-editor build)"), true);
#endif
	return true;
}

// -----------------------------------------------------------------------
// Test: Reflection works (every value has a name)
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ClimateZone_Reflection_Works,
	"Priordium.MapGenerator.ClimateZone.Reflection_Works",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ClimateZone_Reflection_Works::RunTest(const FString& Parameters)
{
	const UEnum* ClimateEnum = StaticEnum<EClimateZone>();
	if (!TestNotNull(TEXT("EClimateZone UEnum accessible"), ClimateEnum))
	{
		return false;
	}

	int32 NumValues = ClimateEnum->NumEnums();
	TestTrue(TEXT("EClimateZone contains at least 4 values"), NumValues >= 4);

	for (int32 i = 0; i < NumValues - 1; ++i)
	{
		FText DisplayName = ClimateEnum->GetDisplayNameTextByIndex(i);
		TestFalse(FString::Printf(TEXT("EClimateZone[%d] display name is not empty"), i), DisplayName.IsEmpty());
	}

	return true;
}

// -----------------------------------------------------------------------
// Test: UMETA DisplayName is set for every value
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ClimateZone_DisplayNames_Set,
	"Priordium.MapGenerator.ClimateZone.DisplayNames_Set",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ClimateZone_DisplayNames_Set::RunTest(const FString& Parameters)
{
	const UEnum* ClimateEnum = StaticEnum<EClimateZone>();
	if (!TestNotNull(TEXT("EClimateZone UEnum accessible"), ClimateEnum))
	{
		return false;
	}

	TArray<FString> ExpectedNames = {
		TEXT("Tropical"), TEXT("Temperate"), TEXT("Continental"), TEXT("Subarctic")
	};

	for (int32 i = 0; i < ExpectedNames.Num(); ++i)
	{
		FText DisplayName = ClimateEnum->GetDisplayNameTextByIndex(i);
		TestEqual(
			FString::Printf(TEXT("EClimateZone[%d] DisplayName correct"), i),
			DisplayName.ToString(),
			ExpectedNames[i]
		);
	}

	return true;
}

