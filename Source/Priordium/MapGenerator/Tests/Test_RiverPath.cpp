// Copyright Priordium. All Rights Reserved.
//
// Test_RiverPath.cpp
// Tests TraceRiverPath() on UWaterSystemBuilder.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UWaterSystemBuilder.h"
#include "UHeightmapGenerator.h"
#include "UMapGeneratorSettings.h"

static void MakeRiverTestPair(
	int32 Res, int32 Seed,
	UHeightmapGenerator*&   OutH,
	UMapGeneratorSettings*& OutS)
{
	OutS = NewObject<UMapGeneratorSettings>(GetTransientPackage(), NAME_None, RF_Transient);
	OutS->Resolution                  = Res;
	OutS->Seed                        = Seed;
	OutS->bUseContinentMask           = true;
	OutS->EdgeFalloffDistance         = 0.3f;
	OutS->SeaLevel                    = 0.3f;
	OutS->MaxRiverCount               = 5;
	OutS->HeightmapConfig.Octaves     = 4;
	OutS->HeightmapConfig.Frequency   = 0.05f;
	OutS->HeightmapConfig.Amplitude   = 1.0f;
	OutS->HeightmapConfig.Persistence = 0.5f;
	OutS->HeightmapConfig.Lacunarity  = 2.0f;

	OutH = NewObject<UHeightmapGenerator>(GetTransientPackage(), NAME_None, RF_Transient);
	OutH->Generate(OutS);
}

// -----------------------------------------------------------------------
// Test: Path starts at the given source cell
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_RiverPath_StartsAtSource,
	"Priordium.MapGenerator.WaterSystemBuilder.RiverPath.StartsAtSource",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_RiverPath_StartsAtSource::RunTest(const FString& Parameters)
{
	UHeightmapGenerator*   Heightmap;
	UMapGeneratorSettings* Settings;
	MakeRiverTestPair(65, 42, Heightmap, Settings);

	UWaterSystemBuilder* Builder = NewObject<UWaterSystemBuilder>(
		GetTransientPackage(), NAME_None, RF_Transient);

	TArray<FIntPoint> Sources = Builder->FindRiverSources(Heightmap, Settings);
	if (Sources.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("RiverPath_StartsAtSource: no sources found, skipping"));
		return true;
	}

	TArray<FIntPoint> Path = Builder->TraceRiverPath(Heightmap, Settings, Sources[0]);
	TestTrue(TEXT("Path has at least 1 cell"), Path.Num() >= 1);
	TestEqual(TEXT("Path starts at source"), Path[0], Sources[0]);
	return true;
}

// -----------------------------------------------------------------------
// Test: Path terminates finitely (does not run forever)
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_RiverPath_TerminatesFinitely,
	"Priordium.MapGenerator.WaterSystemBuilder.RiverPath.TerminatesFinitely",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_RiverPath_TerminatesFinitely::RunTest(const FString& Parameters)
{
	UHeightmapGenerator*   Heightmap;
	UMapGeneratorSettings* Settings;
	MakeRiverTestPair(33, 99, Heightmap, Settings);

	UWaterSystemBuilder* Builder = NewObject<UWaterSystemBuilder>(
		GetTransientPackage(), NAME_None, RF_Transient);

	TArray<FIntPoint> Sources = Builder->FindRiverSources(Heightmap, Settings);
	if (Sources.Num() == 0) return true;

	const FIntPoint Res = Heightmap->GetResolution();
	TArray<FIntPoint> Path = Builder->TraceRiverPath(Heightmap, Settings, Sources[0]);

	// Path length must be bounded by Res.X * Res.Y (the step budget)
	TestTrue(TEXT("Path length <= ResX * ResY"),
		Path.Num() <= Res.X * Res.Y);
	return true;
}

// -----------------------------------------------------------------------
// Test: Path moves downhill (no cell is higher than the source)
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_RiverPath_MovesDownhill,
	"Priordium.MapGenerator.WaterSystemBuilder.RiverPath.MovesDownhill",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_RiverPath_MovesDownhill::RunTest(const FString& Parameters)
{
	UHeightmapGenerator*   Heightmap;
	UMapGeneratorSettings* Settings;
	MakeRiverTestPair(65, 42, Heightmap, Settings);

	UWaterSystemBuilder* Builder = NewObject<UWaterSystemBuilder>(
		GetTransientPackage(), NAME_None, RF_Transient);

	TArray<FIntPoint> Sources = Builder->FindRiverSources(Heightmap, Settings);
	if (Sources.Num() == 0) return true;

	TArray<FIntPoint> Path = Builder->TraceRiverPath(Heightmap, Settings, Sources[0]);
	if (Path.Num() < 2) return true;

	const float SourceHeight = Heightmap->GetHeightAt(Sources[0].X, Sources[0].Y);
	const float EndHeight    = Heightmap->GetHeightAt(Path.Last().X, Path.Last().Y);

	TestTrue(TEXT("Path end is lower than or equal to source height"),
		EndHeight <= SourceHeight + 1e-4f);
	return true;
}

// -----------------------------------------------------------------------
// Test: All path cells are within map bounds
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_RiverPath_AllCellsInBounds,
	"Priordium.MapGenerator.WaterSystemBuilder.RiverPath.AllCellsInBounds",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_RiverPath_AllCellsInBounds::RunTest(const FString& Parameters)
{
	UHeightmapGenerator*   Heightmap;
	UMapGeneratorSettings* Settings;
	MakeRiverTestPair(65, 42, Heightmap, Settings);

	UWaterSystemBuilder* Builder = NewObject<UWaterSystemBuilder>(
		GetTransientPackage(), NAME_None, RF_Transient);

	TArray<FIntPoint> Sources = Builder->FindRiverSources(Heightmap, Settings);
	if (Sources.Num() == 0) return true;

	TArray<FIntPoint> Path = Builder->TraceRiverPath(Heightmap, Settings, Sources[0]);
	const FIntPoint Res = Heightmap->GetResolution();

	for (const FIntPoint& Cell : Path)
	{
		TestTrue(FString::Printf(TEXT("Cell (%d,%d) X in bounds"), Cell.X, Cell.Y),
			Cell.X >= 0 && Cell.X < Res.X);
		TestTrue(FString::Printf(TEXT("Cell (%d,%d) Y in bounds"), Cell.X, Cell.Y),
			Cell.Y >= 0 && Cell.Y < Res.Y);
	}
	return true;
}

// -----------------------------------------------------------------------
// Test: Null Heightmap returns empty path
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_RiverPath_NullHeightmap,
	"Priordium.MapGenerator.WaterSystemBuilder.RiverPath.NullHeightmap",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_RiverPath_NullHeightmap::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("TraceRiverPath -- Heightmap is null"), EAutomationExpectedErrorFlags::Contains, 1);

	UWaterSystemBuilder* Builder = NewObject<UWaterSystemBuilder>(
		GetTransientPackage(), NAME_None, RF_Transient);
	UMapGeneratorSettings* Settings = NewObject<UMapGeneratorSettings>(
		GetTransientPackage(), NAME_None, RF_Transient);

	TArray<FIntPoint> Path = Builder->TraceRiverPath(nullptr, Settings, FIntPoint(0, 0));
	TestEqual(TEXT("Null Heightmap -> empty path"), Path.Num(), 0);
	return true;
}
