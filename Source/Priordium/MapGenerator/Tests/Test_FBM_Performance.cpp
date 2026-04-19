// Copyright Priordium. All Rights Reserved.
//
// Test_FBM_Performance.cpp
// Tests the performance of UHeightmapGenerator::Generate() at high resolution.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "UHeightmapGenerator.h"
#include "UMapGeneratorSettings.h"

// -----------------------------------------------------------------------
// Test: 1009x1009 resolution generation completes in under 5 seconds
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_FBM_Performance_1009,
	"Priordium.MapGenerator.HeightmapGenerator.FBM.Performance_1009x1009",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_FBM_Performance_1009::RunTest(const FString& Parameters)
{
	UHeightmapGenerator* Gen = NewObject<UHeightmapGenerator>();
	if (!TestNotNull(TEXT("Gen is not null"), Gen)) return false;

	UMapGeneratorSettings* Settings = NewObject<UMapGeneratorSettings>();
	Settings->Resolution                  = 1009;
	Settings->Seed                        = 42;
	Settings->HeightmapConfig.Octaves     = 6;
	Settings->HeightmapConfig.Frequency   = 0.005f;
	Settings->HeightmapConfig.Amplitude   = 1.0f;
	Settings->HeightmapConfig.Persistence = 0.5f;
	Settings->HeightmapConfig.Lacunarity  = 2.0f;

	const double StartTime = FPlatformTime::Seconds();
	bool bResult = Gen->Generate(Settings);
	const double ElapsedSeconds = FPlatformTime::Seconds() - StartTime;

	TestTrue(TEXT("Generate() 1009x1009 completed successfully"), bResult);
	TestEqual(TEXT("HeightmapData size is 1009*1009"),
		Gen->GetHeightmapData().Num(), 1009 * 1009);

	const double MaxAllowedSeconds = 5.0;
	if (ElapsedSeconds > MaxAllowedSeconds)
	{
		AddError(FString::Printf(
			TEXT("Generate() 1009x1009 too slow: %.2f sec (limit: %.0f sec)"),
			ElapsedSeconds, MaxAllowedSeconds));
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("FBM Performance 1009x1009: %.3f sec"), ElapsedSeconds);
	}

	return true;
}

// -----------------------------------------------------------------------
// Test: Memory usage -- 1009x1009 float array fits in memory (<= 4 MB)
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_FBM_Performance_MemoryUsage,
	"Priordium.MapGenerator.HeightmapGenerator.FBM.MemoryUsage",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_FBM_Performance_MemoryUsage::RunTest(const FString& Parameters)
{
	// 1009 * 1009 * 4 bytes (float) = ~4.06 MB -- acceptable
	const int32 Res         = 1009;
	const int32 ExpectedBytes = Res * Res * sizeof(float);
	const int32 MaxAllowedMB  = 4;

	TestTrue(
		FString::Printf(TEXT("1009x1009 heightmap <= %d MB (%d bytes)"), MaxAllowedMB, ExpectedBytes),
		ExpectedBytes <= MaxAllowedMB * 1024 * 1024
	);
	return true;
}

