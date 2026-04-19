// Copyright Priordium. All Rights Reserved.
//
// Test_TemperatureMap.cpp
// Tests the temperature map generation of UBiomeManager.
//
// Coordinate convention (as designed):
//   Y=0     -> south -> warm (BaseTemp = 1.0 - 0/ResY = 1.0)
//   Y=max   -> north -> cold (BaseTemp = 1.0 - 1.0 = 0.0)

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "UBiomeManager.h"
#include "UHeightmapGenerator.h"
#include "UMapGeneratorSettings.h"

// -----------------------------------------------------------------------
// Helper function: create and generate Settings + Heightmap
// -----------------------------------------------------------------------
static void MakeTempTestPair(
	int32 Res, int32 Seed,
	UMapGeneratorSettings*& OutSettings,
	UHeightmapGenerator*&   OutHeightmap)
{
	OutSettings = NewObject<UMapGeneratorSettings>();
	OutSettings->Resolution                  = Res;
	OutSettings->Seed                        = Seed;
	OutSettings->bUseContinentMask           = false;
	OutSettings->HeightmapConfig.Octaves     = 4;
	OutSettings->HeightmapConfig.Frequency   = 0.05f;
	OutSettings->HeightmapConfig.Amplitude   = 1.0f;
	OutSettings->HeightmapConfig.Persistence = 0.5f;
	OutSettings->HeightmapConfig.Lacunarity  = 2.0f;

	OutHeightmap = NewObject<UHeightmapGenerator>();
	OutHeightmap->Generate(OutSettings);
}

// -----------------------------------------------------------------------
// Test: AssignBiomes returns false with null Settings
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_TemperatureMap_NullSettings,
	"Priordium.MapGenerator.BiomeManager.TemperatureMap.NullSettings",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_TemperatureMap_NullSettings::RunTest(const FString& Parameters)
{
	UBiomeManager*       Manager   = NewObject<UBiomeManager>();
	UHeightmapGenerator* Heightmap = NewObject<UHeightmapGenerator>();
	Heightmap->InitializeHeightmap(16, 16);

	AddExpectedError(TEXT("Settings null"), EAutomationExpectedMessageFlags::Contains, 1);
	TestFalse(TEXT("AssignBiomes(null, Heightmap) false"), Manager->AssignBiomes(nullptr, Heightmap));
	return true;
}

// -----------------------------------------------------------------------
// Test: AssignBiomes returns false with null/uninitialized Heightmap
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_TemperatureMap_NullHeightmap,
	"Priordium.MapGenerator.BiomeManager.TemperatureMap.NullHeightmap",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_TemperatureMap_NullHeightmap::RunTest(const FString& Parameters)
{
	UBiomeManager*         Manager  = NewObject<UBiomeManager>();
	UMapGeneratorSettings* Settings = NewObject<UMapGeneratorSettings>();
	Settings->Resolution = 16;

	AddExpectedError(TEXT("Heightmap null"), EAutomationExpectedMessageFlags::Contains, 1);
	TestFalse(TEXT("AssignBiomes(Settings, null) false"), Manager->AssignBiomes(Settings, nullptr));
	return true;
}

// -----------------------------------------------------------------------
// Test: TemperatureMap is populated after a successful AssignBiomes call
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_TemperatureMap_Populated,
	"Priordium.MapGenerator.BiomeManager.TemperatureMap.Populated",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_TemperatureMap_Populated::RunTest(const FString& Parameters)
{
	UMapGeneratorSettings* Settings;
	UHeightmapGenerator*   Heightmap;
	MakeTempTestPair(33, 42, Settings, Heightmap);

	UBiomeManager* Manager = NewObject<UBiomeManager>();
	bool bResult = Manager->AssignBiomes(Settings, Heightmap);

	TestTrue(TEXT("AssignBiomes returns true"), bResult);
	TestEqual(TEXT("TemperatureMap size is 33*33"),
		Manager->GetTemperatureMap().Num(), 33 * 33);
	return true;
}

// -----------------------------------------------------------------------
// Test: All temperature values are in the [0, 1] range
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_TemperatureMap_ValuesInRange,
	"Priordium.MapGenerator.BiomeManager.TemperatureMap.ValuesInRange",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_TemperatureMap_ValuesInRange::RunTest(const FString& Parameters)
{
	UMapGeneratorSettings* Settings;
	UHeightmapGenerator*   Heightmap;
	MakeTempTestPair(65, 42, Settings, Heightmap);

	UBiomeManager* Manager = NewObject<UBiomeManager>();
	Manager->AssignBiomes(Settings, Heightmap);

	const TArray<float>& TempMap = Manager->GetTemperatureMap();
	bool bAllInRange = true;
	for (int32 i = 0; i < TempMap.Num(); ++i)
	{
		if (TempMap[i] < 0.0f || TempMap[i] > 1.0f)
		{
			AddError(FString::Printf(TEXT("TempMap[%d] = %.6f -- outside [0,1] range"), i, TempMap[i]));
			bAllInRange = false;
			break;
		}
	}
	TestTrue(TEXT("All temperature values are in the [0, 1] range"), bAllInRange);
	return true;
}

// -----------------------------------------------------------------------
// Test: Y=0 (south) is warmer than Y=max (north)
//
// Coordinate convention as designed:
//   BaseTemp = 1.0 - (Y / ResY)
//   Y=0   -> BaseTemp=1.0 -> warm (south)
//   Y=max -> BaseTemp=0.0 -> cold (north)
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_TemperatureMap_NorthColdSouthWarm,
	"Priordium.MapGenerator.BiomeManager.TemperatureMap.NorthColdSouthWarm",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_TemperatureMap_NorthColdSouthWarm::RunTest(const FString& Parameters)
{
	UMapGeneratorSettings* Settings;
	UHeightmapGenerator*   Heightmap;
	MakeTempTestPair(65, 42, Settings, Heightmap);

	UBiomeManager* Manager = NewObject<UBiomeManager>();
	Manager->AssignBiomes(Settings, Heightmap);

	const int32 Res  = 65;
	const int32 Band = 10;

	// Y=0..Band-1  -> south (high BaseTemp -> warm)
	// Y=Res-Band.. -> north (low BaseTemp -> cold)
	float SouthAvg = 0.0f;
	float NorthAvg = 0.0f;

	for (int32 X = 0; X < Res; ++X)
	{
		for (int32 dy = 0; dy < Band; ++dy)
		{
			SouthAvg += Manager->GetTemperatureAt(X, dy);
			NorthAvg += Manager->GetTemperatureAt(X, Res - 1 - dy);
		}
	}
	SouthAvg /= static_cast<float>(Res * Band);
	NorthAvg /= static_cast<float>(Res * Band);

	// Y=0 band (south) should be warmer than Y=max band (north)
	TestTrue(TEXT("Y=0 band (south) is warmer than Y=max band (north)"), SouthAvg > NorthAvg);
	return true;
}

// -----------------------------------------------------------------------
// Test: Higher cells are colder (altitude correction)
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_TemperatureMap_HeightCorrelation,
	"Priordium.MapGenerator.BiomeManager.TemperatureMap.HeightCorrelation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_TemperatureMap_HeightCorrelation::RunTest(const FString& Parameters)
{
	UMapGeneratorSettings* Settings;
	UHeightmapGenerator*   Heightmap;
	MakeTempTestPair(65, 42, Settings, Heightmap);

	UBiomeManager* Manager = NewObject<UBiomeManager>();
	Manager->AssignBiomes(Settings, Heightmap);

	const int32 MidY = 32;
	const int32 Res  = 65;

	float MaxHeight = -1.0f, MinHeight = 2.0f;
	int32 MaxX = 0, MinX = 0;

	for (int32 X = 0; X < Res; ++X)
	{
		const float H = Heightmap->GetHeightAt(X, MidY);
		if (H > MaxHeight) { MaxHeight = H; MaxX = X; }
		if (H < MinHeight) { MinHeight = H; MinX = X; }
	}

	if (MaxHeight - MinHeight > 0.1f)
	{
		const float TempAtHigh = Manager->GetTemperatureAt(MaxX, MidY);
		const float TempAtLow  = Manager->GetTemperatureAt(MinX, MidY);
		TestTrue(TEXT("Higher cell produces lower temperature in the middle row"),
			TempAtHigh < TempAtLow + 0.05f);
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("HeightCorrelation: small height difference, test skipped"));
	}

	return true;
}

// -----------------------------------------------------------------------
// Test: Determinism -- same seed produces the same temperature map
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_TemperatureMap_Determinism,
	"Priordium.MapGenerator.BiomeManager.TemperatureMap.Determinism",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_TemperatureMap_Determinism::RunTest(const FString& Parameters)
{
	UMapGeneratorSettings* S1; UHeightmapGenerator* H1;
	UMapGeneratorSettings* S2; UHeightmapGenerator* H2;
	MakeTempTestPair(33, 42, S1, H1);
	MakeTempTestPair(33, 42, S2, H2);

	UBiomeManager* M1 = NewObject<UBiomeManager>();
	UBiomeManager* M2 = NewObject<UBiomeManager>();
	M1->AssignBiomes(S1, H1);
	M2->AssignBiomes(S2, H2);

	const TArray<float>& T1 = M1->GetTemperatureMap();
	const TArray<float>& T2 = M2->GetTemperatureMap();

	bool bIdentical = true;
	for (int32 i = 0; i < T1.Num(); ++i)
	{
		if (!FMath::IsNearlyEqual(T1[i], T2[i], 1e-6f))
		{
			AddError(FString::Printf(TEXT("T1[%d]=%.6f != T2[%d]=%.6f"), i, T1[i], i, T2[i]));
			bIdentical = false;
			break;
		}
	}
	TestTrue(TEXT("Same seed produces the same temperature map"), bIdentical);
	return true;
}

// -----------------------------------------------------------------------
// Test: BiomeCell Height and Temperature values are in sync
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_TemperatureMap_CellsUpdated,
	"Priordium.MapGenerator.BiomeManager.TemperatureMap.CellsUpdated",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_TemperatureMap_CellsUpdated::RunTest(const FString& Parameters)
{
	UMapGeneratorSettings* Settings;
	UHeightmapGenerator*   Heightmap;
	MakeTempTestPair(33, 42, Settings, Heightmap);

	UBiomeManager* Manager = NewObject<UBiomeManager>();
	Manager->AssignBiomes(Settings, Heightmap);

	for (int32 Y = 0; Y < 33; Y += 8)
	{
		for (int32 X = 0; X < 33; X += 8)
		{
			FBiomeCell Cell = Manager->GetBiomeAt(X, Y);
			TestEqual(
				*FString::Printf(TEXT("Cell(%d,%d).Height == HeightAt"), X, Y),
				Cell.Height, Heightmap->GetHeightAt(X, Y));
			TestEqual(
				*FString::Printf(TEXT("Cell(%d,%d).Temperature == TempAt"), X, Y),
				Cell.Temperature, Manager->GetTemperatureAt(X, Y));
		}
	}
	return true;
}
