// Copyright Priordium. All Rights Reserved.
//
// Test_CheckFilters.cpp
// Tests CheckFilters(), GetBiomeFilterResult() and GetClimateFilterResult()
// of UResourceDistributor.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UResourceDistributor.h"
#include "FResourceSpawnRule.h"

// Helper: create a distributor
static UResourceDistributor* MakeDistributor()
{
	return NewObject<UResourceDistributor>(GetTransientPackage(), NAME_None, RF_Transient);
}

// -----------------------------------------------------------------------
// Test: Empty AllowedBiomes and AllowedClimateZones â†’ all pass
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_CheckFilters_NoFiltersAllowsAny,
	"Priordium.MapGenerator.ResourceDistributor.CheckFilters.NoFiltersAllowsAny",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_CheckFilters_NoFiltersAllowsAny::RunTest(const FString& Parameters)
{
	UResourceDistributor* Dist = MakeDistributor();
	FResourceSpawnRule Rule; // Empty AllowedBiomes, AllowedClimateZones

	TestTrue(TEXT("Mountain/Tropical/0.5 passes with empty filter"),
		Dist->CheckFilters(Rule, EBiomeType::Mountain, EClimateZone::Tropical, 0.5f));
	TestTrue(TEXT("Ocean/Subarctic/0.0 passes with empty filter"),
		Dist->CheckFilters(Rule, EBiomeType::Ocean, EClimateZone::Subarctic, 0.0f));
	return true;
}

// -----------------------------------------------------------------------
// Test: Biome filter -- matching biome passes
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_CheckFilters_BiomeFilter_Match,
	"Priordium.MapGenerator.ResourceDistributor.CheckFilters.BiomeFilter_Match",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_CheckFilters_BiomeFilter_Match::RunTest(const FString& Parameters)
{
	UResourceDistributor* Dist = MakeDistributor();
	FResourceSpawnRule Rule;
	Rule.AllowedBiomes.Add(EBiomeType::Forest);

	TestTrue(TEXT("Forest biome passes Forest filter"),
		Dist->CheckFilters(Rule, EBiomeType::Forest, EClimateZone::Temperate, 0.5f));
	return true;
}

// -----------------------------------------------------------------------
// Test: Biome filter -- non-matching biome fails
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_CheckFilters_BiomeFilter_NoMatch,
	"Priordium.MapGenerator.ResourceDistributor.CheckFilters.BiomeFilter_NoMatch",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_CheckFilters_BiomeFilter_NoMatch::RunTest(const FString& Parameters)
{
	UResourceDistributor* Dist = MakeDistributor();
	FResourceSpawnRule Rule;
	Rule.AllowedBiomes.Add(EBiomeType::Forest);

	TestFalse(TEXT("Mountain biome fails Forest filter"),
		Dist->CheckFilters(Rule, EBiomeType::Mountain, EClimateZone::Temperate, 0.5f));
	return true;
}

// -----------------------------------------------------------------------
// Test: Climate filter -- matching climate passes
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_CheckFilters_ClimateFilter_Match,
	"Priordium.MapGenerator.ResourceDistributor.CheckFilters.ClimateFilter_Match",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_CheckFilters_ClimateFilter_Match::RunTest(const FString& Parameters)
{
	UResourceDistributor* Dist = MakeDistributor();
	FResourceSpawnRule Rule;
	Rule.AllowedClimateZones.Add(EClimateZone::Tropical);

	TestTrue(TEXT("Tropical climate passes Tropical filter"),
		Dist->CheckFilters(Rule, EBiomeType::Plains, EClimateZone::Tropical, 0.5f));
	return true;
}

// -----------------------------------------------------------------------
// Test: Climate filter -- non-matching climate fails
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_CheckFilters_ClimateFilter_NoMatch,
	"Priordium.MapGenerator.ResourceDistributor.CheckFilters.ClimateFilter_NoMatch",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_CheckFilters_ClimateFilter_NoMatch::RunTest(const FString& Parameters)
{
	UResourceDistributor* Dist = MakeDistributor();
	FResourceSpawnRule Rule;
	Rule.AllowedClimateZones.Add(EClimateZone::Tropical);

	TestFalse(TEXT("Subarctic climate fails Tropical filter"),
		Dist->CheckFilters(Rule, EBiomeType::Plains, EClimateZone::Subarctic, 0.5f));
	return true;
}

// -----------------------------------------------------------------------
// Test: Height filter -- in range passes
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_CheckFilters_HeightFilter_InRange,
	"Priordium.MapGenerator.ResourceDistributor.CheckFilters.HeightFilter_InRange",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_CheckFilters_HeightFilter_InRange::RunTest(const FString& Parameters)
{
	UResourceDistributor* Dist = MakeDistributor();
	FResourceSpawnRule Rule;
	Rule.MinHeight = 0.3f;
	Rule.MaxHeight = 0.7f;

	TestTrue(TEXT("Height 0.5 passes [0.3, 0.7] range"),
		Dist->CheckFilters(Rule, EBiomeType::Plains, EClimateZone::Temperate, 0.5f));
	TestTrue(TEXT("Height 0.3 (boundary) passes"),
		Dist->CheckFilters(Rule, EBiomeType::Plains, EClimateZone::Temperate, 0.3f));
	TestTrue(TEXT("Height 0.7 (boundary) passes"),
		Dist->CheckFilters(Rule, EBiomeType::Plains, EClimateZone::Temperate, 0.7f));
	return true;
}

// -----------------------------------------------------------------------
// Test: Height filter -- out of range fails
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_CheckFilters_HeightFilter_OutOfRange,
	"Priordium.MapGenerator.ResourceDistributor.CheckFilters.HeightFilter_OutOfRange",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_CheckFilters_HeightFilter_OutOfRange::RunTest(const FString& Parameters)
{
	UResourceDistributor* Dist = MakeDistributor();
	FResourceSpawnRule Rule;
	Rule.MinHeight = 0.3f;
	Rule.MaxHeight = 0.7f;

	TestFalse(TEXT("Height 0.1 fails [0.3, 0.7] range"),
		Dist->CheckFilters(Rule, EBiomeType::Plains, EClimateZone::Temperate, 0.1f));
	TestFalse(TEXT("Height 0.9 fails [0.3, 0.7] range"),
		Dist->CheckFilters(Rule, EBiomeType::Plains, EClimateZone::Temperate, 0.9f));
	return true;
}

// -----------------------------------------------------------------------
// Test: Combined filter -- all conditions pass â†’ true
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_CheckFilters_CombinedFilter_Pass,
	"Priordium.MapGenerator.ResourceDistributor.CheckFilters.CombinedFilter_Pass",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_CheckFilters_CombinedFilter_Pass::RunTest(const FString& Parameters)
{
	UResourceDistributor* Dist = MakeDistributor();
	FResourceSpawnRule Rule;
	Rule.AllowedBiomes.Add(EBiomeType::Forest);
	Rule.AllowedClimateZones.Add(EClimateZone::Temperate);
	Rule.MinHeight = 0.3f;
	Rule.MaxHeight = 0.8f;

	TestTrue(TEXT("Forest/Temperate/0.5 passes all combined filters"),
		Dist->CheckFilters(Rule, EBiomeType::Forest, EClimateZone::Temperate, 0.5f));
	return true;
}

// -----------------------------------------------------------------------
// Test: Combined filter -- wrong biome fails
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_CheckFilters_CombinedFilter_FailBiome,
	"Priordium.MapGenerator.ResourceDistributor.CheckFilters.CombinedFilter_FailBiome",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_CheckFilters_CombinedFilter_FailBiome::RunTest(const FString& Parameters)
{
	UResourceDistributor* Dist = MakeDistributor();
	FResourceSpawnRule Rule;
	Rule.AllowedBiomes.Add(EBiomeType::Forest);
	Rule.AllowedClimateZones.Add(EClimateZone::Temperate);
	Rule.MinHeight = 0.3f;
	Rule.MaxHeight = 0.8f;

	TestFalse(TEXT("Mountain/Temperate/0.5 fails due to biome mismatch"),
		Dist->CheckFilters(Rule, EBiomeType::Mountain, EClimateZone::Temperate, 0.5f));
	return true;
}
