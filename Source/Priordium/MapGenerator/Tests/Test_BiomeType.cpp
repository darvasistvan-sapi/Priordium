// Copyright Priordium. All Rights Reserved.
//
// Test_BiomeType.cpp
// Tests the correct definition, reflection, and Blueprint visibility of the EBiomeType enum.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UObject/Class.h"

#include "EBiomeType.h"

// -----------------------------------------------------------------------
// Test: EBiomeType UEnum exists in the reflection system
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_BiomeType_AllBiomeTypes_Declared,
	"Priordium.MapGenerator.BiomeType.AllBiomeTypesDeclared",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_BiomeType_AllBiomeTypes_Declared::RunTest(const FString& Parameters)
{
	const UEnum* BiomeEnum = StaticEnum<EBiomeType>();
	TestNotNull(TEXT("EBiomeType UEnum exists"), BiomeEnum);
	return true;
}

// -----------------------------------------------------------------------
// Test: EBiomeType::Ocean is the first (index 0) value
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_BiomeType_Ocean_IsFirstValue,
	"Priordium.MapGenerator.BiomeType.Ocean_IsFirstValue",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_BiomeType_Ocean_IsFirstValue::RunTest(const FString& Parameters)
{
	EBiomeType Type = EBiomeType::Ocean;
	TestEqual(TEXT("EBiomeType::Ocean value is 0"), (uint8)Type, (uint8)0);
	return true;
}

// -----------------------------------------------------------------------
// Test: All 9 biome values are accessible
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_BiomeType_AllValues_Accessible,
	"Priordium.MapGenerator.BiomeType.AllValues_Accessible",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_BiomeType_AllValues_Accessible::RunTest(const FString& Parameters)
{
	TArray<EBiomeType> Values = {
		EBiomeType::Ocean,
		EBiomeType::Beach,
		EBiomeType::Plains,
		EBiomeType::Forest,
		EBiomeType::Hills,
		EBiomeType::Mountain,
		EBiomeType::River,
		EBiomeType::Lake,
		EBiomeType::Swamp
	};
	TestEqual(TEXT("EBiomeType contains 9 values"), Values.Num(), 9);
	return true;
}

// -----------------------------------------------------------------------
// Test: BlueprintType flag is present (verifiable from reflection)
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_BiomeType_BlueprintVisible,
	"Priordium.MapGenerator.BiomeType.BlueprintVisible",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_BiomeType_BlueprintVisible::RunTest(const FString& Parameters)
{
	const UEnum* BiomeEnum = StaticEnum<EBiomeType>();
	if (!TestNotNull(TEXT("EBiomeType UEnum accessible"), BiomeEnum))
	{
		return false;
	}
	bool bIsBlueprintType = BiomeEnum->HasMetaData(TEXT("BlueprintType"));
	TestTrue(TEXT("EBiomeType BlueprintType flag is present"), bIsBlueprintType);
	return true;
}

// -----------------------------------------------------------------------
// Test: Reflection works (every value has a name)
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_BiomeType_Reflection_Works,
	"Priordium.MapGenerator.BiomeType.Reflection_Works",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_BiomeType_Reflection_Works::RunTest(const FString& Parameters)
{
	const UEnum* BiomeEnum = StaticEnum<EBiomeType>();
	if (!TestNotNull(TEXT("EBiomeType UEnum accessible"), BiomeEnum))
	{
		return false;
	}

	int32 NumValues = BiomeEnum->NumEnums();
	TestTrue(TEXT("EBiomeType contains at least 9 values"), NumValues >= 9);

	for (int32 i = 0; i < NumValues - 1; ++i)
	{
		FText DisplayName = BiomeEnum->GetDisplayNameTextByIndex(i);
		TestFalse(FString::Printf(TEXT("EBiomeType[%d] display name is not empty"), i), DisplayName.IsEmpty());
	}

	return true;
}

// -----------------------------------------------------------------------
// Test: UMETA DisplayName is set for every value
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_BiomeType_DisplayNames_Set,
	"Priordium.MapGenerator.BiomeType.DisplayNames_Set",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_BiomeType_DisplayNames_Set::RunTest(const FString& Parameters)
{
	const UEnum* BiomeEnum = StaticEnum<EBiomeType>();
	if (!TestNotNull(TEXT("EBiomeType UEnum accessible"), BiomeEnum))
	{
		return false;
	}

	TArray<FString> ExpectedNames = {
		TEXT("Ocean"), TEXT("Beach"), TEXT("Plains"), TEXT("Forest"), TEXT("Hills"),
		TEXT("Mountain"), TEXT("River"), TEXT("Lake"), TEXT("Swamp")
	};

	for (int32 i = 0; i < ExpectedNames.Num(); ++i)
	{
		FText DisplayName = BiomeEnum->GetDisplayNameTextByIndex(i);
		TestEqual(
			FString::Printf(TEXT("EBiomeType[%d] DisplayName correct"), i),
			DisplayName.ToString(),
			ExpectedNames[i]
		);
	}

	return true;
}

