// Copyright Priordium. All Rights Reserved.
//
// Test_WaterBodyDefinition.cpp
// Tests FWaterBodyDefinition struct: defaults, fields, and EWaterBodyType enum.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "FWaterBodyDefinition.h"
#include "WaterBodyTypes.h"

// -----------------------------------------------------------------------
// Test: FWaterBodyDefinition can be created with default values
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_WaterBodyDefinition_DefaultValues,
	"Priordium.MapGenerator.WaterSystemBuilder.WaterBodyDefinition.DefaultValues",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_WaterBodyDefinition_DefaultValues::RunTest(const FString& Parameters)
{
	FWaterBodyDefinition Def;
	TestEqual(TEXT("Default WaterType is River"),   Def.WaterType, EWaterBodyType::River);
	TestEqual(TEXT("Default Width is 200.0"),        Def.Width,     200.0f);
	TestEqual(TEXT("Default Depth is 100.0"),        Def.Depth,     100.0f);
	TestEqual(TEXT("SplinePoints empty by default"), Def.SplinePoints.Num(), 0);
	return true;
}

// -----------------------------------------------------------------------
// Test: FWaterBodyDefinition fields can be set
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_WaterBodyDefinition_FieldAssignment,
	"Priordium.MapGenerator.WaterSystemBuilder.WaterBodyDefinition.FieldAssignment",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_WaterBodyDefinition_FieldAssignment::RunTest(const FString& Parameters)
{
	FWaterBodyDefinition Def;
	Def.WaterType = EWaterBodyType::Lake;
	Def.Width     = 500.0f;
	Def.Depth     = 250.0f;
	Def.SplinePoints.Add(FVector(0.0f,    0.0f, 0.0f));
	Def.SplinePoints.Add(FVector(1000.0f, 0.0f, 0.0f));

	TestEqual(TEXT("WaterType set to Lake"),  Def.WaterType,            EWaterBodyType::Lake);
	TestEqual(TEXT("Width set to 500"),       Def.Width,                500.0f);
	TestEqual(TEXT("Depth set to 250"),       Def.Depth,                250.0f);
	TestEqual(TEXT("SplinePoints has 2 pts"), Def.SplinePoints.Num(),   2);
	return true;
}

// -----------------------------------------------------------------------
// Test: EWaterBodyType enum has River, Lake, Ocean values
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_WaterBodyDefinition_EWaterBodyType_Values,
	"Priordium.MapGenerator.WaterSystemBuilder.WaterBodyDefinition.EWaterBodyType_Values",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_WaterBodyDefinition_EWaterBodyType_Values::RunTest(const FString& Parameters)
{
	// Verify enum values are distinct
	TestNotEqual(TEXT("River != Lake"),  EWaterBodyType::River, EWaterBodyType::Lake);
	TestNotEqual(TEXT("River != Ocean"), EWaterBodyType::River, EWaterBodyType::Ocean);
	TestNotEqual(TEXT("Lake != Ocean"),  EWaterBodyType::Lake,  EWaterBodyType::Ocean);
	return true;
}

// -----------------------------------------------------------------------
// Test: Multiple FWaterBodyDefinitions can be stored in a TArray
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_WaterBodyDefinition_ArrayStorage,
	"Priordium.MapGenerator.WaterSystemBuilder.WaterBodyDefinition.ArrayStorage",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_WaterBodyDefinition_ArrayStorage::RunTest(const FString& Parameters)
{
	TArray<FWaterBodyDefinition> Defs;

	FWaterBodyDefinition River;
	River.WaterType = EWaterBodyType::River;
	River.Width     = 300.0f;

	FWaterBodyDefinition Lake;
	Lake.WaterType = EWaterBodyType::Lake;
	Lake.Width     = 800.0f;

	FWaterBodyDefinition Ocean;
	Ocean.WaterType = EWaterBodyType::Ocean;
	Ocean.Width     = 10000.0f;

	Defs.Add(River);
	Defs.Add(Lake);
	Defs.Add(Ocean);

	TestEqual(TEXT("Array has 3 definitions"),         Defs.Num(),          3);
	TestEqual(TEXT("First is River"),  Defs[0].WaterType, EWaterBodyType::River);
	TestEqual(TEXT("Second is Lake"),  Defs[1].WaterType, EWaterBodyType::Lake);
	TestEqual(TEXT("Third is Ocean"),  Defs[2].WaterType, EWaterBodyType::Ocean);
	return true;
}

// -----------------------------------------------------------------------
// Test: FWaterBodyDefinition UStruct is registered in the reflection system
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_WaterBodyDefinition_UStructExists,
	"Priordium.MapGenerator.WaterSystemBuilder.WaterBodyDefinition.UStructExists",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_WaterBodyDefinition_UStructExists::RunTest(const FString& Parameters)
{
	UScriptStruct* Struct = FWaterBodyDefinition::StaticStruct();
	TestNotNull(TEXT("FWaterBodyDefinition UScriptStruct exists"), Struct);
	return true;
}
