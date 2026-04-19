// Copyright Priordium. All Rights Reserved.
//
// Test_SeasonProfile.cpp
// Tests FSeasonProfile defaults and CalculateSeasonalTemperature().

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UClimateZoneManager.h"
#include "FSeasonProfile.h"

// -----------------------------------------------------------------------
// Test: FSeasonProfile defaults
//   SummerTempModifier = 0.1f
//   WinterTempModifier = -0.15f
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_SeasonProfile_Defaults,
	"Priordium.MapGenerator.ClimateZoneManager.SeasonProfile.Defaults",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_SeasonProfile_Defaults::RunTest(const FString& Parameters)
{
	FSeasonProfile Profile;
	TestEqual(TEXT("Default SummerTempModifier is 0.1"),  Profile.SummerTempModifier,  0.1f);
	TestEqual(TEXT("Default WinterTempModifier is -0.15"), Profile.WinterTempModifier, -0.15f);
	return true;
}

// -----------------------------------------------------------------------
// Test: Winter temperature (TimeOfYear=0.0)
//   t = (cos(0) + 1) / 2 = 1.0  ->  Modifier = WinterTempModifier
//   Result = Clamp(BaseTemp + WinterTempModifier, 0, 1)
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_SeasonProfile_WinterTemperature,
	"Priordium.MapGenerator.ClimateZoneManager.SeasonProfile.WinterTemperature",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_SeasonProfile_WinterTemperature::RunTest(const FString& Parameters)
{
	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);

	// Default profile: WinterTempModifier = -0.15
	const float BaseTemp   = 0.5f;
	const float Expected   = BaseTemp + Manager->SeasonProfile.WinterTempModifier; // 0.35
	const float Result     = Manager->CalculateSeasonalTemperature(BaseTemp, 0.0f);

	TestTrue(TEXT("Winter result near BaseTemp + WinterModifier"),
		FMath::IsNearlyEqual(Result, Expected, 1e-4f));
	return true;
}

// -----------------------------------------------------------------------
// Test: Summer temperature (TimeOfYear=0.5)
//   t = (cos(PI) + 1) / 2 = 0.0  ->  Modifier = SummerTempModifier
//   Result = Clamp(BaseTemp + SummerTempModifier, 0, 1)
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_SeasonProfile_SummerTemperature,
	"Priordium.MapGenerator.ClimateZoneManager.SeasonProfile.SummerTemperature",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_SeasonProfile_SummerTemperature::RunTest(const FString& Parameters)
{
	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);

	// Default profile: SummerTempModifier = 0.1
	const float BaseTemp   = 0.5f;
	const float Expected   = BaseTemp + Manager->SeasonProfile.SummerTempModifier; // 0.6
	const float Result     = Manager->CalculateSeasonalTemperature(BaseTemp, 0.5f);

	TestTrue(TEXT("Summer result near BaseTemp + SummerModifier"),
		FMath::IsNearlyEqual(Result, Expected, 1e-4f));
	return true;
}

// -----------------------------------------------------------------------
// Test: Summer is warmer than winter for the same base temperature
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_SeasonProfile_SummerWarmerThanWinter,
	"Priordium.MapGenerator.ClimateZoneManager.SeasonProfile.SummerWarmerThanWinter",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_SeasonProfile_SummerWarmerThanWinter::RunTest(const FString& Parameters)
{
	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);

	const float Base    = 0.5f;
	const float Summer  = Manager->CalculateSeasonalTemperature(Base, 0.5f);
	const float Winter  = Manager->CalculateSeasonalTemperature(Base, 0.0f);

	TestTrue(TEXT("Summer temp > Winter temp for same base"), Summer > Winter);
	return true;
}

// -----------------------------------------------------------------------
// Test: Result is clamped to [0, 1] -- too cold base in winter
//   BaseTemp=0.05f + WinterModifier=-0.15f => -0.10f -> clamp to 0.0f
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_SeasonProfile_ClampToZero,
	"Priordium.MapGenerator.ClimateZoneManager.SeasonProfile.ClampToZero",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_SeasonProfile_ClampToZero::RunTest(const FString& Parameters)
{
	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);

	// Very cold base in winter: 0.05 + (-0.15) = -0.10 -> clamp -> 0.0
	const float Result = Manager->CalculateSeasonalTemperature(0.05f, 0.0f);
	TestTrue(TEXT("Result clamped to 0.0 for very cold winter"), FMath::IsNearlyEqual(Result, 0.0f, 1e-4f));
	return true;
}

// -----------------------------------------------------------------------
// Test: Result is clamped to [0, 1] -- too hot base in summer
//   BaseTemp=0.95f + SummerModifier=0.1f => 1.05f -> clamp to 1.0f
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_SeasonProfile_ClampToOne,
	"Priordium.MapGenerator.ClimateZoneManager.SeasonProfile.ClampToOne",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_SeasonProfile_ClampToOne::RunTest(const FString& Parameters)
{
	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);

	// Very hot base in summer: 0.95 + 0.1 = 1.05 -> clamp -> 1.0
	const float Result = Manager->CalculateSeasonalTemperature(0.95f, 0.5f);
	TestTrue(TEXT("Result clamped to 1.0 for very hot summer"), FMath::IsNearlyEqual(Result, 1.0f, 1e-4f));
	return true;
}

// -----------------------------------------------------------------------
// Test: Smooth transition -- spring (TimeOfYear=0.25) is between winter and summer
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_SeasonProfile_SmoothTransition,
	"Priordium.MapGenerator.ClimateZoneManager.SeasonProfile.SmoothTransition",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_SeasonProfile_SmoothTransition::RunTest(const FString& Parameters)
{
	UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
		GetTransientPackage(), NAME_None, RF_Transient);

	const float Base   = 0.5f;
	const float Winter = Manager->CalculateSeasonalTemperature(Base, 0.0f);
	const float Spring = Manager->CalculateSeasonalTemperature(Base, 0.25f);
	const float Summer = Manager->CalculateSeasonalTemperature(Base, 0.5f);

	TestTrue(TEXT("Spring is between winter and summer"),
		Spring > Winter && Spring < Summer);
	return true;
}
