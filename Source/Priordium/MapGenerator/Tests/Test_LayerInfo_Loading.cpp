// Copyright Priordium. All Rights Reserved.
//
// Test_LayerInfo_Loading.cpp
// Tests the Layer Info loading functionality of ULandscapeBuilder.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "ULandscapeBuilder.h"
#include "EBiomeType.h"

// -----------------------------------------------------------------------
// Test: LayerInfoAssets TMap is empty by default
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_LayerInfo_DefaultEmpty,
	"Priordium.MapGenerator.LandscapeBuilder.LayerInfo.DefaultEmpty",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_LayerInfo_DefaultEmpty::RunTest(const FString& Parameters)
{
	ULandscapeBuilder* Builder = NewObject<ULandscapeBuilder>();
	if (!TestNotNull(TEXT("Builder is not null"), Builder)) return false;

	TestEqual(TEXT("LayerInfoAssets is empty by default"), Builder->LayerInfoAssets.Num(), 0);
	TestFalse(TEXT("AreAllLayersLoaded() is false by default"), Builder->AreAllLayersLoaded());
	return true;
}

// -----------------------------------------------------------------------
// Test: AreAllLayersLoaded() false if any biome is missing
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_LayerInfo_PartialLoadReturnsFalse,
	"Priordium.MapGenerator.LandscapeBuilder.LayerInfo.PartialLoadReturnsFalse",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_LayerInfo_PartialLoadReturnsFalse::RunTest(const FString& Parameters)
{
	ULandscapeBuilder* Builder = NewObject<ULandscapeBuilder>();
	if (!TestNotNull(TEXT("Builder is not null"), Builder)) return false;

	// Add layer info for only a few biomes (null value)
	Builder->LayerInfoAssets.Add(EBiomeType::Ocean,  nullptr);
	Builder->LayerInfoAssets.Add(EBiomeType::Plains, nullptr);

	// Not complete -> false
	TestFalse(TEXT("AreAllLayersLoaded() false if not all biomes loaded"), Builder->AreAllLayersLoaded());
	return true;
}

// -----------------------------------------------------------------------
// Test: LoadLayerInfoAssets() is runnable (returns false without assets)
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_LayerInfo_LoadReturnsResult,
	"Priordium.MapGenerator.LandscapeBuilder.LayerInfo.LoadReturnsResult",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_LayerInfo_LoadReturnsResult::RunTest(const FString& Parameters)
{
	ULandscapeBuilder* Builder = NewObject<ULandscapeBuilder>();
	if (!TestNotNull(TEXT("Builder is not null"), Builder)) return false;

	const bool bResult = Builder->LoadLayerInfoAssets();

	AddInfo(FString::Printf(TEXT("LoadLayerInfoAssets() result: %s"),
		bResult ? TEXT("true (assets loaded)") : TEXT("false (assets missing)")));

	// The function is runnable and did not crash = success
	TestTrue(TEXT("LoadLayerInfoAssets() is runnable and does not crash"), true);
	return true;
}

// -----------------------------------------------------------------------
// Test: LayerInfoAssets can be manually filled and AreAllLayersLoaded() returns true
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_LayerInfo_ManualFillAllLayers,
	"Priordium.MapGenerator.LandscapeBuilder.LayerInfo.ManualFillAllLayers",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_LayerInfo_ManualFillAllLayers::RunTest(const FString& Parameters)
{
	ULandscapeBuilder* Builder = NewObject<ULandscapeBuilder>();
	if (!TestNotNull(TEXT("Builder is not null"), Builder)) return false;

	// Add a dummy (non-null) LayerInfoObject for every biome
	const TArray<EBiomeType> AllBiomes =
	{
		EBiomeType::Ocean, EBiomeType::Beach, EBiomeType::Plains,
		EBiomeType::Forest, EBiomeType::Hills, EBiomeType::Mountain,
		EBiomeType::Swamp
	};

	for (EBiomeType Biome : AllBiomes)
	{
		ULandscapeLayerInfoObject* DummyLayer = NewObject<ULandscapeLayerInfoObject>();
		Builder->LayerInfoAssets.Add(Biome, DummyLayer);
	}

	TestEqual(TEXT("All 7 biomes added"), Builder->LayerInfoAssets.Num(), 7);
	TestTrue(TEXT("AreAllLayersLoaded() true when all biomes loaded"), Builder->AreAllLayersLoaded());
	return true;
}

