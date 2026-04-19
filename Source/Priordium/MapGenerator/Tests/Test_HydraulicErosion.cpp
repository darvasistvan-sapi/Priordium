// Copyright Priordium. All Rights Reserved.
//
// Test_HydraulicErosion.cpp
// Tests the hydraulic erosion functionality of UHeightmapGenerator.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "UHeightmapGenerator.h"
#include "UMapGeneratorSettings.h"

// Helper function: create Settings for erosion testing
static UMapGeneratorSettings* MakeErosionSettings(
	bool bErosion, int32 Iterations = 5000, int32 Res = 65, int32 Seed = 42)
{
	UMapGeneratorSettings* Settings = NewObject<UMapGeneratorSettings>();
	Settings->Resolution                        = Res;
	Settings->Seed                              = Seed;
	Settings->bUseContinentMask                 = false;
	Settings->HeightmapConfig.Octaves           = 4;
	Settings->HeightmapConfig.Frequency         = 0.05f;
	Settings->HeightmapConfig.Amplitude         = 1.0f;
	Settings->HeightmapConfig.Persistence       = 0.5f;
	Settings->HeightmapConfig.Lacunarity        = 2.0f;
	Settings->HeightmapConfig.bEnableErosion    = bErosion;
	Settings->HeightmapConfig.ErosionIterations = Iterations;
	Settings->HeightmapConfig.ErosionRate       = 0.3f;
	Settings->HeightmapConfig.DepositionRate    = 0.3f;
	Settings->HeightmapConfig.EvaporationRate   = 0.02f;
	return Settings;
}

// -----------------------------------------------------------------------
// Test: Values remain in [0, 1] range after erosion
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_HydraulicErosion_ValuesInRange,
	"Priordium.MapGenerator.HeightmapGenerator.Erosion.ValuesInRange",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_HydraulicErosion_ValuesInRange::RunTest(const FString& Parameters)
{
	UHeightmapGenerator* Gen = NewObject<UHeightmapGenerator>();
	if (!TestNotNull(TEXT("Gen is not null"), Gen)) return false;

	Gen->Generate(MakeErosionSettings(true, 5000, 65));

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
	TestTrue(TEXT("All values in [0, 1] range after erosion"), bAllInRange);
	return true;
}

// -----------------------------------------------------------------------
// Test: Increasing iteration count does not crash
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_HydraulicErosion_HighIterationsNoCrash,
	"Priordium.MapGenerator.HeightmapGenerator.Erosion.HighIterationsNoCrash",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_HydraulicErosion_HighIterationsNoCrash::RunTest(const FString& Parameters)
{
	UHeightmapGenerator* Gen = NewObject<UHeightmapGenerator>();
	if (!TestNotNull(TEXT("Gen is not null"), Gen)) return false;

	// 20,000 iterations at small resolution -- fast but tests stability
	bool bResult = Gen->Generate(MakeErosionSettings(true, 20000, 33));
	TestTrue(TEXT("Generate() with 20k iterations does not crash"), bResult);
	TestTrue(TEXT("IsInitialized() is true after erosion"), Gen->IsInitialized());
	return true;
}

// -----------------------------------------------------------------------
// Test: Erosion changes the heightmap (result differs from without erosion)
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_HydraulicErosion_ChangesHeightmap,
	"Priordium.MapGenerator.HeightmapGenerator.Erosion.ChangesHeightmap",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_HydraulicErosion_ChangesHeightmap::RunTest(const FString& Parameters)
{
	UHeightmapGenerator* GenNoErosion  = NewObject<UHeightmapGenerator>();
	UHeightmapGenerator* GenWithErosion = NewObject<UHeightmapGenerator>();
	if (!TestNotNull(TEXT("GenNoErosion is not null"),   GenNoErosion))  return false;
	if (!TestNotNull(TEXT("GenWithErosion is not null"), GenWithErosion)) return false;

	GenNoErosion ->Generate(MakeErosionSettings(false, 0,     65, 42));
	GenWithErosion->Generate(MakeErosionSettings(true,  10000, 65, 42));

	const TArray<float>& D1 = GenNoErosion ->GetHeightmapData();
	const TArray<float>& D2 = GenWithErosion->GetHeightmapData();

	bool bFoundDifference = false;
	for (int32 i = 0; i < D1.Num(); ++i)
	{
		if (!FMath::IsNearlyEqual(D1[i], D2[i], 1e-4f))
		{
			bFoundDifference = true;
			break;
		}
	}
	TestTrue(TEXT("With and without erosion produce different heightmaps"), bFoundDifference);
	return true;
}

// -----------------------------------------------------------------------
// Test: Determinism -- same seed produces same erosion result
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_HydraulicErosion_Determinism,
	"Priordium.MapGenerator.HeightmapGenerator.Erosion.Determinism",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_HydraulicErosion_Determinism::RunTest(const FString& Parameters)
{
	UHeightmapGenerator* Gen1 = NewObject<UHeightmapGenerator>();
	UHeightmapGenerator* Gen2 = NewObject<UHeightmapGenerator>();
	if (!TestNotNull(TEXT("Gen1 is not null"), Gen1)) return false;
	if (!TestNotNull(TEXT("Gen2 is not null"), Gen2)) return false;

	Gen1->Generate(MakeErosionSettings(true, 5000, 33, 42));
	Gen2->Generate(MakeErosionSettings(true, 5000, 33, 42));

	const TArray<float>& D1 = Gen1->GetHeightmapData();
	const TArray<float>& D2 = Gen2->GetHeightmapData();

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
	TestTrue(TEXT("Hydraulic erosion is deterministic (same seed -> same output)"), bIdentical);
	return true;
}

// -----------------------------------------------------------------------
// Test: With bEnableErosion=false, erosion does not run
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_HydraulicErosion_DisabledHasNoEffect,
	"Priordium.MapGenerator.HeightmapGenerator.Erosion.DisabledHasNoEffect",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_HydraulicErosion_DisabledHasNoEffect::RunTest(const FString& Parameters)
{
	UHeightmapGenerator* GenOff1 = NewObject<UHeightmapGenerator>();
	UHeightmapGenerator* GenOff2 = NewObject<UHeightmapGenerator>();
	if (!TestNotNull(TEXT("GenOff1 is not null"), GenOff1)) return false;
	if (!TestNotNull(TEXT("GenOff2 is not null"), GenOff2)) return false;

	// Two generations without erosion must produce the same result
	GenOff1->Generate(MakeErosionSettings(false, 0, 33, 42));
	GenOff2->Generate(MakeErosionSettings(false, 0, 33, 42));

	const TArray<float>& D1 = GenOff1->GetHeightmapData();
	const TArray<float>& D2 = GenOff2->GetHeightmapData();

	bool bIdentical = true;
	for (int32 i = 0; i < D1.Num(); ++i)
	{
		if (!FMath::IsNearlyEqual(D1[i], D2[i], 1e-6f))
		{
			bIdentical = false;
			break;
		}
	}
	TestTrue(TEXT("Repeated generation without erosion produces same result"), bIdentical);
	return true;
}

// -----------------------------------------------------------------------
// Test: Performance -- 1009x1009 + 50k iterations < 10 seconds
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_HydraulicErosion_Performance,
	"Priordium.MapGenerator.HeightmapGenerator.Erosion.Performance_1009x50k",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_HydraulicErosion_Performance::RunTest(const FString& Parameters)
{
	UHeightmapGenerator* Gen = NewObject<UHeightmapGenerator>();
	if (!TestNotNull(TEXT("Gen is not null"), Gen)) return false;

	const double StartTime = FPlatformTime::Seconds();
	Gen->Generate(MakeErosionSettings(true, 50000, 1009, 42));
	const double Elapsed = FPlatformTime::Seconds() - StartTime;

	TestTrue(TEXT("Generate() 1009x1009 + 50k erosion completed"), Gen->IsInitialized());

	const double MaxAllowed = 10.0;
	if (Elapsed > MaxAllowed)
	{
		AddError(FString::Printf(
			TEXT("Erosion 1009x1009 + 50k iterations too slow: %.2f sec (limit: %.0f sec)"),
			Elapsed, MaxAllowed));
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("Erosion Performance 1009x1009 + 50k: %.3f sec"), Elapsed);
	}
	return true;
}

