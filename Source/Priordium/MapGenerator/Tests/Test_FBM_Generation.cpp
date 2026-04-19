// Copyright Priordium. All Rights Reserved.
//
// Test_FBM_Generation.cpp
// Tests the FBM functionality of UHeightmapGenerator::Generate().

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "UHeightmapGenerator.h"
#include "UMapGeneratorSettings.h"

// -----------------------------------------------------------------------
// Helper function: Create Settings for FBM testing WITHOUT mask.
// Continent mask is disabled so FBM result can be measured cleanly.
// -----------------------------------------------------------------------
static UMapGeneratorSettings* MakeTestSettings(int32 Resolution = 33, int32 Seed = 42, int32 Octaves = 4)
{
	UMapGeneratorSettings* Settings = NewObject<UMapGeneratorSettings>();
	Settings->Resolution                  = Resolution;
	Settings->Seed                        = Seed;
	Settings->bUseContinentMask           = false;
	Settings->HeightmapConfig.Octaves     = Octaves;
	Settings->HeightmapConfig.Frequency   = 0.05f;
	Settings->HeightmapConfig.Amplitude   = 1.0f;
	Settings->HeightmapConfig.Persistence = 0.5f;
	Settings->HeightmapConfig.Lacunarity  = 2.0f;
	return Settings;
}

// -----------------------------------------------------------------------
// Test: Generate() returns false with null Settings
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_FBM_Generate_NullSettings,
	"Priordium.MapGenerator.HeightmapGenerator.FBM.Generate_NullSettings",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_FBM_Generate_NullSettings::RunTest(const FString& Parameters)
{
	UHeightmapGenerator* Gen = NewObject<UHeightmapGenerator>();
	if (!TestNotNull(TEXT("Gen is not null"), Gen)) return false;

	AddExpectedError(TEXT("Settings null"), EAutomationExpectedMessageFlags::Contains, 1);
	bool bResult = Gen->Generate(nullptr);
	TestFalse(TEXT("Generate(null) returns false"), bResult);
	return true;
}

// -----------------------------------------------------------------------
// Test: Generate() returns true with valid Settings
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_FBM_Generate_Success,
	"Priordium.MapGenerator.HeightmapGenerator.FBM.Generate_Success",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_FBM_Generate_Success::RunTest(const FString& Parameters)
{
	UHeightmapGenerator* Gen = NewObject<UHeightmapGenerator>();
	if (!TestNotNull(TEXT("Gen is not null"), Gen)) return false;

	UMapGeneratorSettings* Settings = MakeTestSettings();

	bool bResult = Gen->Generate(Settings);
	TestTrue(TEXT("Generate() returns true with valid Settings"), bResult);
	TestTrue(TEXT("IsInitialized() is true after Generate()"), Gen->IsInitialized());
	TestEqual(TEXT("HeightmapData size is Resolution^2"),
		Gen->GetHeightmapData().Num(), Settings->Resolution * Settings->Resolution);
	return true;
}

// -----------------------------------------------------------------------
// Test: All generated values are in the [0, 1] range
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_FBM_ValuesInRange,
	"Priordium.MapGenerator.HeightmapGenerator.FBM.ValuesInRange",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_FBM_ValuesInRange::RunTest(const FString& Parameters)
{
	UHeightmapGenerator* Gen = NewObject<UHeightmapGenerator>();
	if (!TestNotNull(TEXT("Gen is not null"), Gen)) return false;

	Gen->Generate(MakeTestSettings(65, 1337, 6));

	const TArray<float>& Data = Gen->GetHeightmapData();
	bool bAllInRange = true;
	for (int32 i = 0; i < Data.Num(); ++i)
	{
		if (Data[i] < 0.0f || Data[i] > 1.0f)
		{
			AddError(FString::Printf(TEXT("Data[%d] = %.6f -- outside [0, 1] range"), i, Data[i]));
			bAllInRange = false;
			break;
		}
	}

	TestTrue(TEXT("All heightmap values in [0, 1] range"), bAllInRange);
	return true;
}

// -----------------------------------------------------------------------
// Test: Same Seed produces the same heightmap (determinism)
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_FBM_Determinism,
	"Priordium.MapGenerator.HeightmapGenerator.FBM.Determinism",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_FBM_Determinism::RunTest(const FString& Parameters)
{
	UHeightmapGenerator* Gen1 = NewObject<UHeightmapGenerator>();
	UHeightmapGenerator* Gen2 = NewObject<UHeightmapGenerator>();
	if (!TestNotNull(TEXT("Gen1 is not null"), Gen1)) return false;
	if (!TestNotNull(TEXT("Gen2 is not null"), Gen2)) return false;

	Gen1->Generate(MakeTestSettings(33, 42));
	Gen2->Generate(MakeTestSettings(33, 42));

	const TArray<float>& D1 = Gen1->GetHeightmapData();
	const TArray<float>& D2 = Gen2->GetHeightmapData();

	TestEqual(TEXT("Same Seed: same array size"), D1.Num(), D2.Num());

	bool bIdentical = true;
	for (int32 i = 0; i < D1.Num(); ++i)
	{
		if (!FMath::IsNearlyEqual(D1[i], D2[i], 1e-6f))
		{
			AddError(FString::Printf(TEXT("D1[%d]=%.6f != D2[%d]=%.6f"), i, D1[i], i, D2[i]));
			bIdentical = false;
			break;
		}
	}

	TestTrue(TEXT("Same Seed produces same heightmap"), bIdentical);
	return true;
}

// -----------------------------------------------------------------------
// Test: Different Seeds produce different heightmaps
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_FBM_DifferentSeeds,
	"Priordium.MapGenerator.HeightmapGenerator.FBM.DifferentSeeds",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_FBM_DifferentSeeds::RunTest(const FString& Parameters)
{
	UHeightmapGenerator* Gen1 = NewObject<UHeightmapGenerator>();
	UHeightmapGenerator* Gen2 = NewObject<UHeightmapGenerator>();
	if (!TestNotNull(TEXT("Gen1 is not null"), Gen1)) return false;
	if (!TestNotNull(TEXT("Gen2 is not null"), Gen2)) return false;

	Gen1->Generate(MakeTestSettings(33, 100));
	Gen2->Generate(MakeTestSettings(33, 999));

	const TArray<float>& D1 = Gen1->GetHeightmapData();
	const TArray<float>& D2 = Gen2->GetHeightmapData();

	bool bFoundDifference = false;
	for (int32 i = 0; i < D1.Num(); ++i)
	{
		if (!FMath::IsNearlyEqual(D1[i], D2[i], 1e-4f))
		{
			bFoundDifference = true;
			break;
		}
	}

	TestTrue(TEXT("Different Seeds produce different heightmaps"), bFoundDifference);
	return true;
}

// -----------------------------------------------------------------------
// Test: Changing the Octaves parameter affects the result
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_FBM_OctavesAffectOutput,
	"Priordium.MapGenerator.HeightmapGenerator.FBM.OctavesAffectOutput",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_FBM_OctavesAffectOutput::RunTest(const FString& Parameters)
{
	UHeightmapGenerator* Gen1 = NewObject<UHeightmapGenerator>();
	UHeightmapGenerator* Gen2 = NewObject<UHeightmapGenerator>();
	if (!TestNotNull(TEXT("Gen1 is not null"), Gen1)) return false;
	if (!TestNotNull(TEXT("Gen2 is not null"), Gen2)) return false;

	Gen1->Generate(MakeTestSettings(33, 42, 1));  // 1 octave
	Gen2->Generate(MakeTestSettings(33, 42, 6));  // 6 octaves

	const TArray<float>& D1 = Gen1->GetHeightmapData();
	const TArray<float>& D2 = Gen2->GetHeightmapData();

	bool bFoundDifference = false;
	for (int32 i = 0; i < D1.Num(); ++i)
	{
		if (!FMath::IsNearlyEqual(D1[i], D2[i], 1e-4f))
		{
			bFoundDifference = true;
			break;
		}
	}

	TestTrue(TEXT("Different Octaves produce different heightmaps"), bFoundDifference);
	return true;
}

// -----------------------------------------------------------------------
// Test: Normalization -- without mask min ~= 0, max ~= 1
// Note: with continent mask ON, max < 1 (mask pulls values down),
// so this test is run with mask OFF.
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_FBM_NormalizationRange,
	"Priordium.MapGenerator.HeightmapGenerator.FBM.NormalizationRange",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_FBM_NormalizationRange::RunTest(const FString& Parameters)
{
	UHeightmapGenerator* Gen = NewObject<UHeightmapGenerator>();
	if (!TestNotNull(TEXT("Gen is not null"), Gen)) return false;

	// MakeTestSettings: bUseContinentMask = false
	Gen->Generate(MakeTestSettings(65, 42, 4));

	const TArray<float>& Data = Gen->GetHeightmapData();

	float MinVal = Data[0];
	float MaxVal = Data[0];
	for (float H : Data)
	{
		if (H < MinVal) MinVal = H;
		if (H > MaxVal) MaxVal = H;
	}

	// After min-max normalization: min = 0.0, max = 1.0 (exactly)
	TestTrue(TEXT("Min value near 0 (< 0.001)"), MinVal < 0.001f);
	TestTrue(TEXT("Max value near 1 (> 0.999)"), MaxVal > 0.999f);
	return true;
}

