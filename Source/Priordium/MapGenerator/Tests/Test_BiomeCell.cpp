// Copyright Priordium. All Rights Reserved.
//
// Test_BiomeCell.cpp
// Tests the functionality of the FBiomeCell struct.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UObject/Class.h"

#include "FBiomeCell.h"

// -----------------------------------------------------------------------
// Test: FBiomeCell default values are correct
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_BiomeCell_DefaultValues,
	"Priordium.MapGenerator.BiomeManager.BiomeCell.DefaultValues",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_BiomeCell_DefaultValues::RunTest(const FString& Parameters)
{
	FBiomeCell Cell;

	TestEqual(TEXT("BiomeType default Plains"),    Cell.BiomeType,    EBiomeType::Plains);
	TestEqual(TEXT("Height default 0.5"),          Cell.Height,       0.5f);
	TestEqual(TEXT("Temperature default 0.5"),     Cell.Temperature,  0.5f);
	TestEqual(TEXT("Moisture default 0.5"),        Cell.Moisture,     0.5f);
	float WeightSum = 0.0f;
	for (float W : Cell.BiomeBlendWeights) { WeightSum += W; }
	TestEqual(TEXT("BiomeBlendWeights all zero by default"), WeightSum, 0.0f);

	return true;
}

// -----------------------------------------------------------------------
// Test: FBiomeCell values are modifiable
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_BiomeCell_Modifiable,
	"Priordium.MapGenerator.BiomeManager.BiomeCell.Modifiable",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_BiomeCell_Modifiable::RunTest(const FString& Parameters)
{
	FBiomeCell Cell;
	Cell.BiomeType   = EBiomeType::Forest;
	Cell.Height      = 0.7f;
	Cell.Temperature = 0.6f;
	Cell.Moisture    = 0.8f;

	TestEqual(TEXT("BiomeType = Forest"),     Cell.BiomeType,    EBiomeType::Forest);
	TestEqual(TEXT("Height = 0.7"),           Cell.Height,       0.7f);
	TestEqual(TEXT("Temperature = 0.6"),      Cell.Temperature,  0.6f);
	TestEqual(TEXT("Moisture = 0.8"),         Cell.Moisture,     0.8f);

	return true;
}

// -----------------------------------------------------------------------
// Test: Array BiomeBlendWeights indexing operations
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_BiomeCell_BlendWeights_Operations,
	"Priordium.MapGenerator.BiomeManager.BiomeCell.BlendWeightsOperations",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_BiomeCell_BlendWeights_Operations::RunTest(const FString& Parameters)
{
	FBiomeCell Cell;

	// Write via index
	Cell.BiomeBlendWeights[static_cast<int32>(EBiomeType::Forest)] = 0.6f;
	Cell.BiomeBlendWeights[static_cast<int32>(EBiomeType::Plains)] = 0.4f;

	// Read back
	TestEqual(TEXT("Forest weight = 0.6"),
		Cell.BiomeBlendWeights[static_cast<int32>(EBiomeType::Forest)], 0.6f);
	TestEqual(TEXT("Plains weight = 0.4"),
		Cell.BiomeBlendWeights[static_cast<int32>(EBiomeType::Plains)], 0.4f);

	// Unset biome is 0
	TestEqual(TEXT("Mountain weight = 0.0 (unset)"),
		Cell.BiomeBlendWeights[static_cast<int32>(EBiomeType::Mountain)], 0.0f);

	return true;
}

// -----------------------------------------------------------------------
// Test: FBiomeCell is copyable (copy semantics)
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_BiomeCell_CopySemantics,
	"Priordium.MapGenerator.BiomeManager.BiomeCell.CopySemantics",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_BiomeCell_CopySemantics::RunTest(const FString& Parameters)
{
	FBiomeCell Original;
	Original.BiomeType   = EBiomeType::Mountain;
	Original.Height      = 0.9f;
	Original.Temperature = 0.2f;
	Original.BiomeBlendWeights[static_cast<int32>(EBiomeType::Mountain)] = 0.8f;
	Original.BiomeBlendWeights[static_cast<int32>(EBiomeType::Hills)]   = 0.2f;

	FBiomeCell Copy = Original;

	TestEqual(TEXT("Copy.BiomeType matches"),    Copy.BiomeType,   Original.BiomeType);
	TestEqual(TEXT("Copy.Height matches"),        Copy.Height,      Original.Height);
	TestEqual(TEXT("Copy.Temperature matches"),   Copy.Temperature, Original.Temperature);
	TestEqual(TEXT("Copy.BiomeBlendWeights[Mountain] matches"),
		Copy.BiomeBlendWeights[static_cast<int32>(EBiomeType::Mountain)],
		Original.BiomeBlendWeights[static_cast<int32>(EBiomeType::Mountain)]);
	TestEqual(TEXT("Copy.BiomeBlendWeights[Hills] matches"),
		Copy.BiomeBlendWeights[static_cast<int32>(EBiomeType::Hills)],
		Original.BiomeBlendWeights[static_cast<int32>(EBiomeType::Hills)]);

	// Independent copy
	Copy.Height = 0.1f;
	TestNotEqual(TEXT("Modifying Copy does not affect original"), Copy.Height, Original.Height);

	return true;
}

// -----------------------------------------------------------------------
// Test: FBiomeCell UStruct reflection
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_BiomeCell_Reflection,
	"Priordium.MapGenerator.BiomeManager.BiomeCell.Reflection",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_BiomeCell_Reflection::RunTest(const FString& Parameters)
{
	const UScriptStruct* Struct = TBaseStructure<FBiomeCell>::Get();
	TestNotNull(TEXT("FBiomeCell UScriptStruct exists"), Struct);

	if (!Struct) return false;

	TArray<FString> ExpectedProps = {
		TEXT("BiomeType"), TEXT("Height"), TEXT("Temperature"),
		TEXT("Moisture"), TEXT("BiomeBlendWeights")
	};
	for (const FString& PropName : ExpectedProps)
	{
		FProperty* Prop = Struct->FindPropertyByName(*PropName);
		TestNotNull(*FString::Printf(TEXT("FBiomeCell::%s property exists"), *PropName), Prop);
	}

	return true;
}

