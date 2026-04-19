// Copyright Priordium. All Rights Reserved.
//
// Test_FindRiverSources.cpp
// Tests FindRiverSources() on UWaterSystemBuilder.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UWaterSystemBuilder.h"
#include "UHeightmapGenerator.h"
#include "UMapGeneratorSettings.h"

// Helper: create and generate a standard heightmap + settings pair
static void MakeWaterTestPair(
	int32 Res, int32 Seed,
	UHeightmapGenerator*&   OutHeightmap,
	UMapGeneratorSettings*& OutSettings)
{
	OutSettings = NewObject<UMapGeneratorSettings>(GetTransientPackage(), NAME_None, RF_Transient);
	OutSettings->Resolution                  = Res;
	OutSettings->Seed                        = Seed;
	OutSettings->bUseContinentMask           = true;
	OutSettings->EdgeFalloffDistance         = 0.3f;
	OutSettings->SeaLevel                    = 0.3f;
	OutSettings->MaxRiverCount               = 5;
	OutSettings->HeightmapConfig.Octaves     = 4;
	OutSettings->HeightmapConfig.Frequency   = 0.05f;
	OutSettings->HeightmapConfig.Amplitude   = 1.0f;
	OutSettings->HeightmapConfig.Persistence = 0.5f;
	OutSettings->HeightmapConfig.Lacunarity  = 2.0f;

	OutHeightmap = NewObject<UHeightmapGenerator>(GetTransientPackage(), NAME_None, RF_Transient);
	OutHeightmap->Generate(OutSettings);
}

// -----------------------------------------------------------------------
// Test: Returns at most MaxRiverCount sources
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_FindRiverSources_CountRespected,
	"Priordium.MapGenerator.WaterSystemBuilder.FindRiverSources.CountRespected",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_FindRiverSources_CountRespected::RunTest(const FString& Parameters)
{
	UHeightmapGenerator*   Heightmap;
	UMapGeneratorSettings* Settings;
	MakeWaterTestPair(65, 42, Heightmap, Settings);
	Settings->MaxRiverCount = 3;

	UWaterSystemBuilder* Builder = NewObject<UWaterSystemBuilder>(
		GetTransientPackage(), NAME_None, RF_Transient);

	TArray<FIntPoint> Sources = Builder->FindRiverSources(Heightmap, Settings);
	TestTrue(TEXT("Source count <= MaxRiverCount"), Sources.Num() <= 3);
	return true;
}

// -----------------------------------------------------------------------
// Test: All sources are above sea level
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_FindRiverSources_SourcesAboveSeaLevel,
	"Priordium.MapGenerator.WaterSystemBuilder.FindRiverSources.SourcesAboveSeaLevel",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_FindRiverSources_SourcesAboveSeaLevel::RunTest(const FString& Parameters)
{
	UHeightmapGenerator*   Heightmap;
	UMapGeneratorSettings* Settings;
	MakeWaterTestPair(65, 42, Heightmap, Settings);

	UWaterSystemBuilder* Builder = NewObject<UWaterSystemBuilder>(
		GetTransientPackage(), NAME_None, RF_Transient);

	TArray<FIntPoint> Sources = Builder->FindRiverSources(Heightmap, Settings);

	for (const FIntPoint& S : Sources)
	{
		const float H = Heightmap->GetHeightAt(S.X, S.Y);
		TestTrue(
			FString::Printf(TEXT("Source (%d,%d) height %.4f > SeaLevel %.4f"), S.X, S.Y, H, Settings->SeaLevel),
			H > Settings->SeaLevel);
	}
	return true;
}

// -----------------------------------------------------------------------
// Test: All sources are local maxima (no neighbor is higher)
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_FindRiverSources_SourcesAreLocalMaxima,
	"Priordium.MapGenerator.WaterSystemBuilder.FindRiverSources.SourcesAreLocalMaxima",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_FindRiverSources_SourcesAreLocalMaxima::RunTest(const FString& Parameters)
{
	UHeightmapGenerator*   Heightmap;
	UMapGeneratorSettings* Settings;
	MakeWaterTestPair(65, 42, Heightmap, Settings);

	UWaterSystemBuilder* Builder = NewObject<UWaterSystemBuilder>(
		GetTransientPackage(), NAME_None, RF_Transient);

	TArray<FIntPoint> Sources = Builder->FindRiverSources(Heightmap, Settings);

	for (const FIntPoint& S : Sources)
	{
		const float H = Heightmap->GetHeightAt(S.X, S.Y);
		bool bIsMax = true;
		for (int32 dy = -1; dy <= 1 && bIsMax; ++dy)
			for (int32 dx = -1; dx <= 1 && bIsMax; ++dx)
			{
				if (dx == 0 && dy == 0) continue;
				if (Heightmap->GetHeightAt(S.X + dx, S.Y + dy) >= H)
					bIsMax = false;
			}
		TestTrue(FString::Printf(TEXT("Source (%d,%d) is a local maximum"), S.X, S.Y), bIsMax);
	}
	return true;
}

// -----------------------------------------------------------------------
// Test: Minimum distance between sources is respected
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_FindRiverSources_MinDistance,
	"Priordium.MapGenerator.WaterSystemBuilder.FindRiverSources.MinDistance",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_FindRiverSources_MinDistance::RunTest(const FString& Parameters)
{
	UHeightmapGenerator*   Heightmap;
	UMapGeneratorSettings* Settings;
	MakeWaterTestPair(65, 42, Heightmap, Settings);
	Settings->MaxRiverCount = 5;

	UWaterSystemBuilder* Builder = NewObject<UWaterSystemBuilder>(
		GetTransientPackage(), NAME_None, RF_Transient);
	Builder->MinRiverSourceDistance = 6.0f;

	TArray<FIntPoint> Sources = Builder->FindRiverSources(Heightmap, Settings);
	const float MinDistSq = Builder->MinRiverSourceDistance * Builder->MinRiverSourceDistance;

	for (int32 i = 0; i < Sources.Num(); ++i)
	{
		for (int32 j = i + 1; j < Sources.Num(); ++j)
		{
			const float DX = static_cast<float>(Sources[i].X - Sources[j].X);
			const float DY = static_cast<float>(Sources[i].Y - Sources[j].Y);
			const float DistSq = DX * DX + DY * DY;
			TestTrue(
				FString::Printf(TEXT("Sources [%d] and [%d] are at least %.1f cells apart"),
					i, j, Builder->MinRiverSourceDistance),
				DistSq >= MinDistSq);
		}
	}
	return true;
}

// -----------------------------------------------------------------------
// Test: Determinism -- same seed produces the same sources
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_FindRiverSources_Determinism,
	"Priordium.MapGenerator.WaterSystemBuilder.FindRiverSources.Determinism",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_FindRiverSources_Determinism::RunTest(const FString& Parameters)
{
	UHeightmapGenerator*   H1; UMapGeneratorSettings* S1;
	UHeightmapGenerator*   H2; UMapGeneratorSettings* S2;
	MakeWaterTestPair(65, 42, H1, S1);
	MakeWaterTestPair(65, 42, H2, S2);

	UWaterSystemBuilder* B1 = NewObject<UWaterSystemBuilder>(GetTransientPackage(), NAME_None, RF_Transient);
	UWaterSystemBuilder* B2 = NewObject<UWaterSystemBuilder>(GetTransientPackage(), NAME_None, RF_Transient);

	TArray<FIntPoint> Src1 = B1->FindRiverSources(H1, S1);
	TArray<FIntPoint> Src2 = B2->FindRiverSources(H2, S2);

	TestEqual(TEXT("Same seed -> same number of sources"), Src1.Num(), Src2.Num());
	for (int32 i = 0; i < Src1.Num(); ++i)
	{
		TestEqual(FString::Printf(TEXT("Source[%d].X identical"), i), Src1[i].X, Src2[i].X);
		TestEqual(FString::Printf(TEXT("Source[%d].Y identical"), i), Src1[i].Y, Src2[i].Y);
	}
	return true;
}

// -----------------------------------------------------------------------
// Test: Null Heightmap returns empty array
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_FindRiverSources_NullHeightmap,
	"Priordium.MapGenerator.WaterSystemBuilder.FindRiverSources.NullHeightmap",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_FindRiverSources_NullHeightmap::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("FindRiverSources -- Heightmap is null"), EAutomationExpectedErrorFlags::Contains, 1);

	UWaterSystemBuilder* Builder = NewObject<UWaterSystemBuilder>(
		GetTransientPackage(), NAME_None, RF_Transient);
	UMapGeneratorSettings* Settings = NewObject<UMapGeneratorSettings>(
		GetTransientPackage(), NAME_None, RF_Transient);

	TArray<FIntPoint> Sources = Builder->FindRiverSources(nullptr, Settings);
	TestEqual(TEXT("Null Heightmap -> empty result"), Sources.Num(), 0);
	return true;
}

// -----------------------------------------------------------------------
// Test: Null Settings returns empty array
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_FindRiverSources_NullSettings,
	"Priordium.MapGenerator.WaterSystemBuilder.FindRiverSources.NullSettings",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_FindRiverSources_NullSettings::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("FindRiverSources -- Settings is null"), EAutomationExpectedErrorFlags::Contains, 1);

	UHeightmapGenerator* Heightmap = NewObject<UHeightmapGenerator>(
		GetTransientPackage(), NAME_None, RF_Transient);
	Heightmap->InitializeHeightmap(16, 16);

	UWaterSystemBuilder* Builder = NewObject<UWaterSystemBuilder>(
		GetTransientPackage(), NAME_None, RF_Transient);

	TArray<FIntPoint> Sources = Builder->FindRiverSources(Heightmap, nullptr);
	TestEqual(TEXT("Null Settings -> empty result"), Sources.Num(), 0);
	return true;
}
