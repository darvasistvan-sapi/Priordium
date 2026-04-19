// Copyright Priordium. All Rights Reserved.
//
// Test_ContinentMask.cpp
// Tests the continent mask functionality of UHeightmapGenerator.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "UHeightmapGenerator.h"
#include "UMapGeneratorSettings.h"

// Helper function: create Settings
static UMapGeneratorSettings* MakeMaskSettings(bool bUseMask, float Falloff = 0.3f, int32 Res = 65)
{
	UMapGeneratorSettings* Settings = NewObject<UMapGeneratorSettings>();
	Settings->Resolution                  = Res;
	Settings->Seed                        = 42;
	Settings->bUseContinentMask           = bUseMask;
	Settings->EdgeFalloffDistance         = Falloff;
	Settings->HeightmapConfig.Octaves     = 4;
	Settings->HeightmapConfig.Frequency   = 0.05f;
	Settings->HeightmapConfig.Amplitude   = 1.0f;
	Settings->HeightmapConfig.Persistence = 0.5f;
	Settings->HeightmapConfig.Lacunarity  = 2.0f;
	return Settings;
}

// -----------------------------------------------------------------------
// Test: Edge values are near 0 after mask application
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ContinentMask_EdgesNearZero,
	"Priordium.MapGenerator.HeightmapGenerator.EdgeFalloff.EdgesNearZero",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ContinentMask_EdgesNearZero::RunTest(const FString& Parameters)
{
	UHeightmapGenerator* Gen = NewObject<UHeightmapGenerator>();
	if (!TestNotNull(TEXT("Gen is not null"), Gen)) return false;

	Gen->Generate(MakeMaskSettings(true, 0.3f, 65));

	const int32 LastX = Gen->GetResolution().X - 1;
	const int32 LastY = Gen->GetResolution().Y - 1;

	// Corners -- Smoothstep(0) = 0, then * FBM value = always 0.0f
	TestEqual(TEXT("(0,0) corner = 0.0"),        Gen->GetHeightAt(0,     0    ), 0.0f);
	TestEqual(TEXT("(LastX,0) corner = 0.0"),     Gen->GetHeightAt(LastX, 0    ), 0.0f);
	TestEqual(TEXT("(0,LastY) corner = 0.0"),     Gen->GetHeightAt(0,     LastY), 0.0f);
	TestEqual(TEXT("(LastX,LastY) corner = 0.0"), Gen->GetHeightAt(LastX, LastY), 0.0f);

	// Centers of edge rows also near 0
	const float TopEdge    = Gen->GetHeightAt(LastX / 2, 0);
	const float BottomEdge = Gen->GetHeightAt(LastX / 2, LastY);
	TestTrue(TEXT("Top edge center < 0.05"),    TopEdge    < 0.05f);
	TestTrue(TEXT("Bottom edge center < 0.05"), BottomEdge < 0.05f);

	return true;
}

// -----------------------------------------------------------------------
// Test: Mask does not zero out the center area
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ContinentMask_CenterUntouched,
	"Priordium.MapGenerator.HeightmapGenerator.EdgeFalloff.CenterUntouched",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ContinentMask_CenterUntouched::RunTest(const FString& Parameters)
{
	UHeightmapGenerator* Gen = NewObject<UHeightmapGenerator>();
	if (!TestNotNull(TEXT("Gen is not null"), Gen)) return false;

	Gen->Generate(MakeMaskSettings(true, 0.3f, 65));

	const int32 CX = Gen->GetResolution().X / 2;
	const int32 CY = Gen->GetResolution().Y / 2;

	// Center: DistFromEdge = 0.5, MaskT = 0.5/0.3 > 1.0 -> Smoothstep(1) = 1.0
	// So center value = FBM value * 1.0 = untouched
	const float CenterVal = Gen->GetHeightAt(CX, CY);
	TestTrue(TEXT("Center value > 0.1 (was not zeroed out)"), CenterVal > 0.1f);

	return true;
}

// -----------------------------------------------------------------------
// Test: Smooth transition -- larger falloff produces smaller values near the edge
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ContinentMask_SmoothTransition,
	"Priordium.MapGenerator.HeightmapGenerator.EdgeFalloff.SmoothTransition",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ContinentMask_SmoothTransition::RunTest(const FString& Parameters)
{
	UHeightmapGenerator* GenLarge = NewObject<UHeightmapGenerator>();
	UHeightmapGenerator* GenSmall = NewObject<UHeightmapGenerator>();
	if (!TestNotNull(TEXT("GenLarge is not null"), GenLarge)) return false;
	if (!TestNotNull(TEXT("GenSmall is not null"), GenSmall)) return false;

	// Same seed, same noise -- only falloff differs
	GenLarge->Generate(MakeMaskSettings(true, 0.45f, 65));
	GenSmall->Generate(MakeMaskSettings(true, 0.15f, 65));

	// Large falloff -> smaller values near edge, because the reduced zone extends further
	const int32 NearEdgeX = 5;
	const int32 MidY      = 32;

	const float LargeEdgeVal = GenLarge->GetHeightAt(NearEdgeX, MidY);
	const float SmallEdgeVal = GenSmall->GetHeightAt(NearEdgeX, MidY);

	TestTrue(TEXT("Large falloff produces smaller or equal values near edge than small falloff"),
		LargeEdgeVal <= SmallEdgeVal + 0.01f);

	return true;
}

// -----------------------------------------------------------------------
// Test: Values remain in [0, 1] range after mask application
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ContinentMask_ValuesInRange,
	"Priordium.MapGenerator.HeightmapGenerator.EdgeFalloff.ValuesInRange",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ContinentMask_ValuesInRange::RunTest(const FString& Parameters)
{
	UHeightmapGenerator* Gen = NewObject<UHeightmapGenerator>();
	if (!TestNotNull(TEXT("Gen is not null"), Gen)) return false;

	Gen->Generate(MakeMaskSettings(true, 0.3f, 65));

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
	TestTrue(TEXT("All values in [0, 1] range after mask"), bAllInRange);
	return true;
}

// -----------------------------------------------------------------------
// Test: With bUseContinentMask=false, corner values differ from mask ON case
//
// Note: With mask OFF, corner value is NOT necessarily > 0,
// because min-max normalization sets the smallest FBM value to 0.
// Instead: CENTER values are similar (same noise), CORNER values differ.
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ContinentMask_DisabledHasNoEffect,
	"Priordium.MapGenerator.HeightmapGenerator.EdgeFalloff.DisabledHasNoEffect",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ContinentMask_DisabledHasNoEffect::RunTest(const FString& Parameters)
{
	UHeightmapGenerator* GenOn  = NewObject<UHeightmapGenerator>();
	UHeightmapGenerator* GenOff = NewObject<UHeightmapGenerator>();
	if (!TestNotNull(TEXT("GenOn is not null"),  GenOn))  return false;
	if (!TestNotNull(TEXT("GenOff is not null"), GenOff)) return false;

	GenOn ->Generate(MakeMaskSettings(true,  0.3f, 33));
	GenOff->Generate(MakeMaskSettings(false, 0.3f, 33));

	// Mask ON: corner = 0.0 (Smoothstep(0) = 0)
	TestEqual(TEXT("Mask ON: corner = 0.0"), GenOn->GetHeightAt(0, 0), 0.0f);

	// Mask OFF vs ON: corners are DIFFERENT
	// (GenOff corner is the normalized FBM value, GenOn corner is 0)
	// The FBM value at the corner without mask is likely not exactly 0,
	// but normalization can make it 0. So we compare the full array average.
	const TArray<float>& DataOn  = GenOn ->GetHeightmapData();
	const TArray<float>& DataOff = GenOff->GetHeightmapData();

	// With mask ON, the average should be smaller (mask pulled down the edges)
	float SumOn = 0.0f, SumOff = 0.0f;
	for (int32 i = 0; i < DataOn.Num(); ++i) { SumOn  += DataOn[i];  }
	for (int32 i = 0; i < DataOff.Num(); ++i){ SumOff += DataOff[i]; }
	const float AvgOn  = SumOn  / DataOn.Num();
	const float AvgOff = SumOff / DataOff.Num();

	TestTrue(TEXT("Mask ON average < Mask OFF average (mask pulls down edges)"),
		AvgOn < AvgOff);

	// Center values are nearly identical (mask has no effect there)
	const int32 CX = 16, CY = 16;
	TestTrue(TEXT("Center values are nearly equal with and without mask"),
		FMath::IsNearlyEqual(GenOn->GetHeightAt(CX, CY), GenOff->GetHeightAt(CX, CY), 0.01f));

	return true;
}

// -----------------------------------------------------------------------
// Test: Determinism -- same parameters produce same mask result
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ContinentMask_Determinism,
	"Priordium.MapGenerator.HeightmapGenerator.EdgeFalloff.Determinism",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ContinentMask_Determinism::RunTest(const FString& Parameters)
{
	UHeightmapGenerator* Gen1 = NewObject<UHeightmapGenerator>();
	UHeightmapGenerator* Gen2 = NewObject<UHeightmapGenerator>();
	if (!TestNotNull(TEXT("Gen1 is not null"), Gen1)) return false;
	if (!TestNotNull(TEXT("Gen2 is not null"), Gen2)) return false;

	Gen1->Generate(MakeMaskSettings(true, 0.3f, 33));
	Gen2->Generate(MakeMaskSettings(true, 0.3f, 33));

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
	TestTrue(TEXT("Continent mask is deterministic"), bIdentical);
	return true;
}

