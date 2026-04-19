// Copyright Priordium. All Rights Reserved.
//
// Test_LandscapeCreation.cpp
// Tests the Landscape creation functionality of ULandscapeBuilder.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "ULandscapeBuilder.h"
#include "UMapGeneratorSettings.h"
#include "UHeightmapGenerator.h"

// Helper function: create basic Settings
static UMapGeneratorSettings* MakeLandscapeSettings(int32 Res = 33, int32 QuadSize = 100)
{
	UMapGeneratorSettings* S = NewObject<UMapGeneratorSettings>();
	S->Resolution                  = Res;
	S->QuadSize                    = QuadSize;
	S->Seed                        = 42;
	S->bUseContinentMask           = false;
	S->HeightmapConfig.Octaves     = 4;
	S->HeightmapConfig.Frequency   = 0.05f;
	S->HeightmapConfig.Amplitude   = 1.0f;
	S->HeightmapConfig.Persistence = 0.5f;
	S->HeightmapConfig.Lacunarity  = 2.0f;
	return S;
}

// -----------------------------------------------------------------------
// Test: BuildLandscape -- returns false with null Settings
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_LandscapeCreation_NullSettings,
	"Priordium.MapGenerator.LandscapeBuilder.Creation.NullSettings",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_LandscapeCreation_NullSettings::RunTest(const FString& Parameters)
{
	ULandscapeBuilder* Builder = NewObject<ULandscapeBuilder>();
	if (!TestNotNull(TEXT("Builder is not null"), Builder)) return false;

	UHeightmapGenerator* Gen = NewObject<UHeightmapGenerator>();
	Gen->InitializeHeightmap(33, 33);

	AddExpectedError(TEXT("Settings null"), EAutomationExpectedErrorFlags::Contains, 1);
	const bool bResult = Builder->BuildLandscape(nullptr, Gen, nullptr);

	TestFalse(TEXT("BuildLandscape returns false with null Settings"), bResult);
	TestFalse(TEXT("IsLandscapeCreated() remains false"), Builder->IsLandscapeCreated());
	return true;
}

// -----------------------------------------------------------------------
// Test: BuildLandscape -- returns false with null Heightmap
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_LandscapeCreation_NullHeightmap,
	"Priordium.MapGenerator.LandscapeBuilder.Creation.NullHeightmap",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_LandscapeCreation_NullHeightmap::RunTest(const FString& Parameters)
{
	ULandscapeBuilder* Builder = NewObject<ULandscapeBuilder>();
	if (!TestNotNull(TEXT("Builder is not null"), Builder)) return false;

	AddExpectedError(TEXT("Heightmap null"), EAutomationExpectedErrorFlags::Contains, 1);
	const bool bResult = Builder->BuildLandscape(MakeLandscapeSettings(), nullptr, nullptr);

	TestFalse(TEXT("BuildLandscape returns false with null Heightmap"), bResult);
	return true;
}

// -----------------------------------------------------------------------
// Test: BuildLandscape -- returns false with uninitialized Heightmap
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_LandscapeCreation_UninitializedHeightmap,
	"Priordium.MapGenerator.LandscapeBuilder.Creation.UninitializedHeightmap",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_LandscapeCreation_UninitializedHeightmap::RunTest(const FString& Parameters)
{
	ULandscapeBuilder* Builder  = NewObject<ULandscapeBuilder>();
	UHeightmapGenerator* Gen    = NewObject<UHeightmapGenerator>();
	if (!TestNotNull(TEXT("Builder is not null"), Builder)) return false;

	// Gen is not initialized
	AddExpectedError(TEXT("Heightmap null"), EAutomationExpectedErrorFlags::Contains, 1);
	const bool bResult = Builder->BuildLandscape(MakeLandscapeSettings(), Gen, nullptr);

	TestFalse(TEXT("BuildLandscape returns false with uninitialized Heightmap"), bResult);
	return true;
}

// -----------------------------------------------------------------------
// Test: ConvertHeightmapToUint16 -- returns array of correct size
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_LandscapeCreation_ConversionSize,
	"Priordium.MapGenerator.LandscapeBuilder.Creation.ConversionSize",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_LandscapeCreation_ConversionSize::RunTest(const FString& Parameters)
{
	ULandscapeBuilder* Builder  = NewObject<ULandscapeBuilder>();
	UHeightmapGenerator* Gen    = NewObject<UHeightmapGenerator>();
	if (!TestNotNull(TEXT("Builder is not null"), Builder)) return false;

	UMapGeneratorSettings* Settings = MakeLandscapeSettings(33);
	Gen->Generate(Settings);

	const TArray<uint16> Result = Builder->ConvertHeightmapToUint16(Gen->GetHeightmapData());

	TestEqual(TEXT("Conversion result has 33*33 = 1089 elements"), Result.Num(), 33 * 33);
	return true;
}

// -----------------------------------------------------------------------
// Test: BuildLandscape -- returns false without World (no World in test environment)
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_LandscapeCreation_NoWorld,
	"Priordium.MapGenerator.LandscapeBuilder.Creation.NoWorld",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_LandscapeCreation_NoWorld::RunTest(const FString& Parameters)
{
	ULandscapeBuilder* Builder  = NewObject<ULandscapeBuilder>();
	UHeightmapGenerator* Gen    = NewObject<UHeightmapGenerator>();
	if (!TestNotNull(TEXT("Builder is not null"), Builder)) return false;

	UMapGeneratorSettings* Settings = MakeLandscapeSettings(33);
	Gen->Generate(Settings);

	// No World in test environment -> BuildLandscape runs on the World null branch
	AddExpectedError(TEXT("World null"), EAutomationExpectedErrorFlags::Contains, 1);
	const bool bResult = Builder->BuildLandscape(Settings, Gen, nullptr);

	// No World -> false expected
	TestFalse(TEXT("BuildLandscape returns false without World"), bResult);
	TestFalse(TEXT("IsLandscapeCreated() remains false"), Builder->IsLandscapeCreated());
	return true;
}

