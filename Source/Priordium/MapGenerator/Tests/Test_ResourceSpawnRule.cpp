// Copyright Priordium. All Rights Reserved.
//
// Test_ResourceSpawnRule.cpp
// Tests the default values, reflection, and fields of FResourceSpawnRule.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

#include "FResourceSpawnRule.h"

// -----------------------------------------------------------------------
// Test: FResourceSpawnRule default values are correct
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ResourceSpawnRule_DefaultConstruction,
	"Priordium.MapGenerator.ResourceSpawnRule.DefaultConstruction",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ResourceSpawnRule_DefaultConstruction::RunTest(const FString& Parameters)
{
	FResourceSpawnRule Rule;

	TestFalse(TEXT("ResourceTag default is invalid"), Rule.ResourceTag.IsValid());
	TestEqual(TEXT("ActorClasses default is empty"), Rule.ActorClasses.Num(), 0);
	TestEqual(TEXT("AllowedBiomes default is empty"),        Rule.AllowedBiomes.Num(),        0);
	TestEqual(TEXT("AllowedClimateZones default is empty"),  Rule.AllowedClimateZones.Num(),  0);
	TestEqual(TEXT("MinHeight default is 0.0"),   Rule.MinHeight,   0.0f);
	TestEqual(TEXT("MaxHeight default is 1.0"),   Rule.MaxHeight,   1.0f);
	TestEqual(TEXT("Density default is 0.5"),     Rule.Density,     0.5f);
	TestEqual(TEXT("ClusterSizeMin default is 1"), Rule.ClusterSizeMin, 1);
	TestEqual(TEXT("ClusterSizeMax default is 5"), Rule.ClusterSizeMax, 5);
	TestEqual(TEXT("ClusterRadius default is 500"), Rule.ClusterRadius, 500.0f);
	TestEqual(TEXT("MinWaterDistance default is 0"), Rule.MinWaterDistance, 0.0f);
	TestTrue(TEXT("MaxWaterDistance default is negative (no maximum)"), Rule.MaxWaterDistance < 0.0f);

	return true;
}

// -----------------------------------------------------------------------
// Test: FResourceSpawnRule UStruct exists in the reflection system
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ResourceSpawnRule_StructReflection,
	"Priordium.MapGenerator.ResourceSpawnRule.StructReflection",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ResourceSpawnRule_StructReflection::RunTest(const FString& Parameters)
{
	const UScriptStruct* Struct = TBaseStructure<FResourceSpawnRule>::Get();
	TestNotNull(TEXT("FResourceSpawnRule UScriptStruct exists"), Struct);
	return true;
}

// -----------------------------------------------------------------------
// Test: FResourceSpawnRule BlueprintType flag is present
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ResourceSpawnRule_BlueprintType,
	"Priordium.MapGenerator.ResourceSpawnRule.BlueprintType",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ResourceSpawnRule_BlueprintType::RunTest(const FString& Parameters)
{
	const UScriptStruct* Struct = TBaseStructure<FResourceSpawnRule>::Get();
	if (!TestNotNull(TEXT("FResourceSpawnRule UScriptStruct is accessible"), Struct))
		return false;

	TestTrue(TEXT("FResourceSpawnRule BlueprintType flag is present"),
		Struct->HasMetaData(TEXT("BlueprintType")));
	return true;
}

// -----------------------------------------------------------------------
// Test: All UPROPERTYs exist in the struct
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ResourceSpawnRule_PropertiesExist,
	"Priordium.MapGenerator.ResourceSpawnRule.PropertiesExist",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ResourceSpawnRule_PropertiesExist::RunTest(const FString& Parameters)
{
	const UScriptStruct* Struct = TBaseStructure<FResourceSpawnRule>::Get();
	if (!TestNotNull(TEXT("FResourceSpawnRule UScriptStruct is accessible"), Struct))
		return false;

	const TArray<FString> ExpectedProps = {
		TEXT("ResourceTag"), TEXT("ActorClasses"),
		TEXT("AllowedBiomes"), TEXT("AllowedClimateZones"),
		TEXT("MinHeight"), TEXT("MaxHeight"),
		TEXT("Density"), TEXT("ClusterSizeMin"), TEXT("ClusterSizeMax"),
		TEXT("ClusterRadius"), TEXT("MinWaterDistance"), TEXT("MaxWaterDistance")
	};

	for (const FString& PropName : ExpectedProps)
	{
		FProperty* Prop = Struct->FindPropertyByName(*PropName);
		TestNotNull(*FString::Printf(TEXT("FResourceSpawnRule::%s property exists"), *PropName), Prop);
	}

	return true;
}

// -----------------------------------------------------------------------
// Test: TArray<EBiomeType> AllowedBiomes works
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ResourceSpawnRule_TArrayBiomesWorks,
	"Priordium.MapGenerator.ResourceSpawnRule.TArrayBiomesWorks",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ResourceSpawnRule_TArrayBiomesWorks::RunTest(const FString& Parameters)
{
	FResourceSpawnRule Rule;

	Rule.AllowedBiomes.Add(EBiomeType::Forest);
	Rule.AllowedBiomes.Add(EBiomeType::Plains);

	TestEqual(TEXT("AllowedBiomes contains 2 elements"), Rule.AllowedBiomes.Num(), 2);
	TestEqual(TEXT("AllowedBiomes[0] = Forest"), Rule.AllowedBiomes[0], EBiomeType::Forest);
	TestEqual(TEXT("AllowedBiomes[1] = Plains"), Rule.AllowedBiomes[1], EBiomeType::Plains);

	return true;
}

// -----------------------------------------------------------------------
// Test: TArray<EClimateZone> AllowedClimateZones works
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ResourceSpawnRule_TArrayClimateZonesWorks,
	"Priordium.MapGenerator.ResourceSpawnRule.TArrayClimateZonesWorks",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ResourceSpawnRule_TArrayClimateZonesWorks::RunTest(const FString& Parameters)
{
	FResourceSpawnRule Rule;

	Rule.AllowedClimateZones.Add(EClimateZone::Tropical);
	Rule.AllowedClimateZones.Add(EClimateZone::Temperate);

	TestEqual(TEXT("AllowedClimateZones contains 2 elements"), Rule.AllowedClimateZones.Num(), 2);
	TestEqual(TEXT("AllowedClimateZones[0] = Tropical"),  Rule.AllowedClimateZones[0], EClimateZone::Tropical);
	TestEqual(TEXT("AllowedClimateZones[1] = Temperate"), Rule.AllowedClimateZones[1], EClimateZone::Temperate);

	return true;
}

// -----------------------------------------------------------------------
// Test: Struct is copyable (copy semantics)
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ResourceSpawnRule_CopySemantics,
	"Priordium.MapGenerator.ResourceSpawnRule.CopySemantics",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ResourceSpawnRule_CopySemantics::RunTest(const FString& Parameters)
{
	FResourceSpawnRule Original;
	Original.Density = 0.8f;
	Original.MinHeight = 0.2f;
	Original.MaxHeight = 0.9f;
	Original.AllowedBiomes.Add(EBiomeType::Mountain);
	Original.AllowedClimateZones.Add(EClimateZone::Subarctic);

	const FResourceSpawnRule Copy = Original;

	TestEqual(TEXT("Copy Density matches"),               Copy.Density,                   Original.Density);
	TestEqual(TEXT("Copy MinHeight matches"),             Copy.MinHeight,                 Original.MinHeight);
	TestEqual(TEXT("Copy MaxHeight matches"),             Copy.MaxHeight,                 Original.MaxHeight);
	TestEqual(TEXT("Copy AllowedBiomes size matches"),    Copy.AllowedBiomes.Num(),       Original.AllowedBiomes.Num());
	TestEqual(TEXT("Copy AllowedClimateZones matches"),   Copy.AllowedClimateZones.Num(), Original.AllowedClimateZones.Num());

	return true;
}
