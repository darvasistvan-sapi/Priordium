// Copyright Priordium. All Rights Reserved.
//
// UMapGenConsoleCommands.cpp
// MapGen.* console command implementations.

#include "UMapGenConsoleCommands.h"
#include "FNoiseWrapper.h"
#include "UHeightmapGenerator.h"
#include "UBiomeManager.h"
#include "UMapGeneratorSettings.h"
#include "ULandscapeBuilder.h"
#include "UClimateZoneManager.h"
#include "UWaterSystemBuilder.h"
#include "UResourceDistributor.h"
#include "HAL/IConsoleManager.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeProxy.h"

DEFINE_LOG_CATEGORY_STATIC(LogMapGenCmd, Log, All);

// Helper function: get PIE World
static UWorld* GetPIEWorld()
{
	if (!GEngine) return nullptr;
	for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
	{
		if (Ctx.WorldType == EWorldType::PIE && Ctx.World())
			return Ctx.World();
	}
	return nullptr;
}

// Helper function: create base Settings for console commands
static UMapGeneratorSettings* MakeCmdSettings(int32 Res, int32 Seed)
{
	UMapGeneratorSettings* S = NewObject<UMapGeneratorSettings>(
		GetTransientPackage(), NAME_None, RF_Transient);
	S->Resolution                  = Res;
	S->Seed                        = Seed;
	// Defaults match FHeightmapConfig C++ defaults so the command
	// behaves identically to the AMapGenerator actor out of the box.
	S->bUseContinentMask           = true;
	S->EdgeFalloffDistance         = 0.3f;
	S->HeightmapConfig.Octaves     = 6;
	S->HeightmapConfig.Frequency   = 0.01f;
	S->HeightmapConfig.Amplitude   = 1.0f;
	S->HeightmapConfig.Persistence = 0.5f;
	S->HeightmapConfig.Lacunarity  = 2.0f;
	return S;
}

void UMapGenConsoleCommands::Register()
{
	// -------------------------------------------------------------------------
	// MapGen.TestNoise [X=100] [Y=100] [Seed=42]
	// -------------------------------------------------------------------------
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("MapGen.TestNoise"),
		TEXT("Tests FNoiseWrapper::SimplexNoise2D. Arguments: X Y Seed (default: 100 100 42)"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			float X    = Args.IsValidIndex(0) ? FCString::Atof(*Args[0]) : 100.0f;
			float Y    = Args.IsValidIndex(1) ? FCString::Atof(*Args[1]) : 100.0f;
			int32 Seed = Args.IsValidIndex(2) ? FCString::Atoi(*Args[2]) : 42;

			float Value = FNoiseWrapper::SimplexNoise2D(X, Y, Seed);
			UE_LOG(LogMapGenCmd, Display,
				TEXT("MapGen.TestNoise | X=%.2f  Y=%.2f  Seed=%d  =>  Noise=%.6f  (range [-1, 1])"),
				X, Y, Seed, Value);

			if (GEngine)
				GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Cyan,
					FString::Printf(TEXT("MapGen.TestNoise | X=%.2f Y=%.2f Seed=%d => %.6f"), X, Y, Seed, Value));
		}),
		ECVF_Default
	);

	// -------------------------------------------------------------------------
	// MapGen.CheckEdge [Resolution=65] [Seed=42] [Falloff=0.3]
	// -------------------------------------------------------------------------
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("MapGen.CheckEdge"),
		TEXT("Checks continent mask. Arguments: Resolution Seed Falloff (default: 65 42 0.3)"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			const int32 Res     = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 65;
			const int32 Seed    = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 42;
			const float Falloff = Args.IsValidIndex(2) ? FCString::Atof(*Args[2]) : 0.3f;

			UMapGeneratorSettings* S = MakeCmdSettings(Res, Seed);
			S->EdgeFalloffDistance = Falloff;

			UHeightmapGenerator* Gen = NewObject<UHeightmapGenerator>(
				GetTransientPackage(), NAME_None, RF_Transient);
			Gen->Generate(S);

			const int32 MidX = Res / 2, MidY = Res / 2;
			UE_LOG(LogMapGenCmd, Display,
				TEXT("MapGen.CheckEdge | Res=%d Seed=%d Falloff=%.2f"), Res, Seed, Falloff);
			UE_LOG(LogMapGenCmd, Display,
				TEXT("  Corner  (0,0):    %.4f  [expected: ~0.0]"), Gen->GetHeightAt(0,    0   ));
			UE_LOG(LogMapGenCmd, Display,
				TEXT("  Edge T  (%d,0):   %.4f  [expected: < 0.05]"), MidX, Gen->GetHeightAt(MidX, 0   ));
			UE_LOG(LogMapGenCmd, Display,
				TEXT("  Edge L  (0,%d):   %.4f  [expected: < 0.05]"), MidY, Gen->GetHeightAt(0,    MidY));
			UE_LOG(LogMapGenCmd, Display,
				TEXT("  Center  (%d,%d):  %.4f  [expected: > 0.1]"),  MidX, MidY, Gen->GetHeightAt(MidX, MidY));

			if (GEngine)
				GEngine->AddOnScreenDebugMessage(-1, 12.0f, FColor::Green,
					FString::Printf(TEXT("Center: %.4f | EdgeTop: %.4f | Corner: %.4f"),
						Gen->GetHeightAt(MidX, MidY), Gen->GetHeightAt(MidX, 0), Gen->GetHeightAt(0, 0)));
		}),
		ECVF_Default
	);

	// -------------------------------------------------------------------------
	// MapGen.TestBiomeManager [Resolution=16] [Seed=42]
	// -------------------------------------------------------------------------
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("MapGen.TestBiomeManager"),
		TEXT("Tests the UBiomeManager base structure. Arguments: Resolution Seed (default: 16 42)"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			const int32 Res  = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 16;
			const int32 Seed = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 42;

			UBiomeManager* Manager = NewObject<UBiomeManager>(
				GetTransientPackage(), NAME_None, RF_Transient);
			Manager->InitializeBiomeMap(Res, Res);

			UE_LOG(LogMapGenCmd, Display,
				TEXT("MapGen.TestBiomeManager | Res=%d Seed=%d | IsInitialized=%s | BiomeMap.Num=%d"),
				Res, Seed,
				Manager->IsInitialized() ? TEXT("true") : TEXT("false"),
				Manager->GetBiomeMap().Num());

			const TArray<TPair<int32,int32>> Coords = {
				{0,0}, {Res/2,Res/2}, {Res-1,Res-1}, {-1,0}, {Res,0}
			};
			for (const auto& C : Coords)
			{
				FBiomeCell Cell   = Manager->GetBiomeAt(C.Key, C.Value);
				const bool bValid = C.Key >= 0 && C.Key < Res && C.Value >= 0 && C.Value < Res;
				UE_LOG(LogMapGenCmd, Display,
					TEXT("  (%2d,%2d) [%s] BiomeType=%d Height=%.2f Temp=%.2f Moist=%.2f"),
					C.Key, C.Value, bValid ? TEXT("valid") : TEXT("OOB  "),
					static_cast<int32>(Cell.BiomeType),
					Cell.Height, Cell.Temperature, Cell.Moisture);
			}

			if (GEngine)
				GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Yellow,
					FString::Printf(TEXT("BiomeManager | Res=%dx%d | Cells=%d | OK"), Res, Res, Manager->GetBiomeMap().Num()));
		}),
		ECVF_Default
	);

	// -------------------------------------------------------------------------
	// MapGen.ShowTemperatureMap [Resolution=65] [Seed=42]
	// Temperature map generation and statistics output.
	// Displays north/south averages and a few sample points.
	// -------------------------------------------------------------------------
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("MapGen.ShowTemperatureMap"),
		TEXT("Generates and checks temperature map. Arguments: Resolution Seed (default: 65 42)"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			const int32 Res  = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 65;
			const int32 Seed = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 42;

			UMapGeneratorSettings* Settings = MakeCmdSettings(Res, Seed);
			Settings->bUseContinentMask = false;

			UHeightmapGenerator* Heightmap = NewObject<UHeightmapGenerator>(
				GetTransientPackage(), NAME_None, RF_Transient);
			Heightmap->Generate(Settings);

			UBiomeManager* Manager = NewObject<UBiomeManager>(
				GetTransientPackage(), NAME_None, RF_Transient);
			const bool bOK = Manager->AssignBiomes(Settings, Heightmap);

			if (!bOK)
			{
				UE_LOG(LogMapGenCmd, Warning, TEXT("MapGen.ShowTemperatureMap  --  AssignBiomes failed."));
				return;
			}

			// Statistics
			const int32 Band = FMath::Max(1, Res / 8);
			float NorthAvg = 0.0f, SouthAvg = 0.0f, MidAvg = 0.0f;
			for (int32 X = 0; X < Res; ++X)
			{
				for (int32 dy = 0; dy < Band; ++dy)
				{
					NorthAvg += Manager->GetTemperatureAt(X, dy);
					SouthAvg += Manager->GetTemperatureAt(X, Res - 1 - dy);
					MidAvg   += Manager->GetTemperatureAt(X, Res / 2);
				}
			}
			const float N = static_cast<float>(Res * Band);
			NorthAvg /= N; SouthAvg /= N; MidAvg /= N;

			UE_LOG(LogMapGenCmd, Display,
				TEXT("MapGen.ShowTemperatureMap | Res=%d Seed=%d"), Res, Seed);
			UE_LOG(LogMapGenCmd, Display,
				TEXT("  Northern avg (Y=0..%d):    %.4f  [expected: < MidAvg]"), Band - 1, NorthAvg);
			UE_LOG(LogMapGenCmd, Display,
				TEXT("  Middle avg (Y=%d):         %.4f"), Res / 2, MidAvg);
			UE_LOG(LogMapGenCmd, Display,
				TEXT("  Southern avg (Y=%d..%d):   %.4f  [expected: > NorthAvg]"),
				Res - Band, Res - 1, SouthAvg);
			UE_LOG(LogMapGenCmd, Display,
				TEXT("  Gradient (south-north):    %.4f  [expected: > 0]"), SouthAvg - NorthAvg);

			// A few sample points
			const int32 MidX = Res / 2;
			UE_LOG(LogMapGenCmd, Display, TEXT("  Sample points:"));
			for (int32 Row : {0, Res/4, Res/2, 3*Res/4, Res-1})
			{
				UE_LOG(LogMapGenCmd, Display,
					TEXT("    (%2d,%2d) Temp=%.4f Height=%.4f"),
					MidX, Row,
					Manager->GetTemperatureAt(MidX, Row),
					Heightmap->GetHeightAt(MidX, Row));
			}

			if (GEngine)
				GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Orange,
					FString::Printf(TEXT("TempMap | North=%.3f | Mid=%.3f | South=%.3f | Gradient=%.3f"),
						NorthAvg, MidAvg, SouthAvg, SouthAvg - NorthAvg));

			// DrawDebug visualization: temperature map blue-red color scale
			// cold (Y=0) = blue, warm (Y=max) = red
			UWorld* World = GetPIEWorld();
			if (World)
			{
				const float CellSize  = 100.0f;
				const float DrawZ     = 10.0f;
				const float Duration  = 60.0f;
				const float PointSize = 40.0f;
				const float OffsetX   = -(Res * CellSize * 0.5f);
				const float OffsetY   = -(Res * CellSize * 0.5f);

				for (int32 Y = 0; Y < Res; ++Y)
				{
					for (int32 X = 0; X < Res; ++X)
					{
						const float T = Manager->GetTemperatureAt(X, Y);
						// Blue-red color scale: cold=blue (0,0,255), warm=red (255,0,0)
						const uint8 R = static_cast<uint8>(T * 255.0f);
						const uint8 G = 0;
						const uint8 B = static_cast<uint8>((1.0f - T) * 255.0f);
						// Y = north-south (in UE Y axis = side)
						// X = east-west (in UE X axis = forward)
						// Therefore: grid X -> UE Y, grid Y -> UE X
						const FVector Pos(
							OffsetY + Y * CellSize,  // grid Y (north-south) -> UE X (forward)
							OffsetX + X * CellSize,  // grid X -> UE Y (side)
							DrawZ);
						DrawDebugPoint(World, Pos, PointSize, FColor(R, G, B), false, Duration);
					}
				}
				UE_LOG(LogMapGenCmd, Display,
					TEXT("  Debug visualization: %dx%d points drawn (%.0f sec, CellSize=%.0f UU)"),
					Res, Res, Duration, CellSize);
			}
			else
			{
				UE_LOG(LogMapGenCmd, Warning,
					TEXT("  WARNING: Debug visualization only works in PIE (World null)."));
			}
		}),
		ECVF_Default
	);

	// -------------------------------------------------------------------------
	// MapGen.ShowMoistureMap [Resolution=65] [Seed=42]
	// Moisture map generation and statistics output.
	// -------------------------------------------------------------------------
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("MapGen.ShowMoistureMap"),
		TEXT("Generates and checks moisture map. Arguments: Resolution Seed (default: 65 42)"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			const int32 Res  = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 65;
			const int32 Seed = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 42;

			UMapGeneratorSettings* Settings = MakeCmdSettings(Res, Seed);
			Settings->bUseContinentMask = false;

			UHeightmapGenerator* Heightmap = NewObject<UHeightmapGenerator>(
				GetTransientPackage(), NAME_None, RF_Transient);
			Heightmap->Generate(Settings);

			UBiomeManager* Manager = NewObject<UBiomeManager>(
				GetTransientPackage(), NAME_None, RF_Transient);
			const bool bOK = Manager->AssignBiomes(Settings, Heightmap);

			if (!bOK)
			{
				UE_LOG(LogMapGenCmd, Warning, TEXT("MapGen.ShowMoistureMap  --  AssignBiomes failed."));
				return;
			}

			// Statistics
			const int32 Band = FMath::Max(1, Res / 8);
			float MinMoist = FLT_MAX, MaxMoist = -FLT_MAX, SumMoist = 0.0f;
			for (int32 Y = 0; Y < Res; ++Y)
			{
				for (int32 X = 0; X < Res; ++X)
				{
					const float M = Manager->GetMoistureAt(X, Y);
					SumMoist += M;
					if (M < MinMoist) MinMoist = M;
					if (M > MaxMoist) MaxMoist = M;
				}
			}
			const float AvgMoist = SumMoist / static_cast<float>(Res * Res);

			// Edge vs center average
			float EdgeSum = 0.0f; int32 EdgeCount = 0;
			float CenterSum = 0.0f; int32 CenterCount = 0;
			for (int32 X = 0; X < Res; ++X)
			{
				for (int32 dy = 0; dy < Band; ++dy)
				{
					EdgeSum += Manager->GetMoistureAt(X, dy);
					EdgeSum += Manager->GetMoistureAt(X, Res - 1 - dy);
					EdgeCount += 2;
				}
			}
			for (int32 Y = Res / 4; Y < 3 * Res / 4; ++Y)
				for (int32 X = Res / 4; X < 3 * Res / 4; ++X)
				{ CenterSum += Manager->GetMoistureAt(X, Y); ++CenterCount; }

			const float EdgeAvg   = EdgeCount   > 0 ? EdgeSum   / EdgeCount   : 0.0f;
			const float CenterAvg = CenterCount > 0 ? CenterSum / CenterCount : 0.0f;

			UE_LOG(LogMapGenCmd, Display,
				TEXT("MapGen.ShowMoistureMap | Res=%d Seed=%d"), Res, Seed);
			UE_LOG(LogMapGenCmd, Display,
				TEXT("  Min: %.4f | Max: %.4f | Avg: %.4f"), MinMoist, MaxMoist, AvgMoist);
			UE_LOG(LogMapGenCmd, Display,
				TEXT("  Edge avg (Y=0..%d + Y=%d..%d): %.4f  [expected: > CenterAvg]"),
				Band - 1, Res - Band, Res - 1, EdgeAvg);
			UE_LOG(LogMapGenCmd, Display,
				TEXT("  Center avg (middle 50%%):        %.4f"), CenterAvg);

			// Sample points
			const int32 MidX = Res / 2;
			UE_LOG(LogMapGenCmd, Display, TEXT("  Sample points:"));
			for (int32 Row : {0, Res/4, Res/2, 3*Res/4, Res-1})
			{
				UE_LOG(LogMapGenCmd, Display,
					TEXT("    (%2d,%2d) Moist=%.4f Height=%.4f"),
					MidX, Row,
					Manager->GetMoistureAt(MidX, Row),
					Heightmap->GetHeightAt(MidX, Row));
			}

			if (GEngine)
				GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Cyan,
					FString::Printf(
						TEXT("MoistureMap | Min=%.3f Max=%.3f Avg=%.3f | Edge=%.3f Center=%.3f"),
						MinMoist, MaxMoist, AvgMoist, EdgeAvg, CenterAvg));

			// DrawDebug visualization: moisture map green color scale
			// Each cell is represented by a point in the world (100 UU/cell)
			UWorld* World = GetPIEWorld();
			if (World)
			{
				const float CellSize   = 100.0f;  // 100 UU/cell
				const float DrawZ      = 10.0f;   // On the ground, just above it
				const float Duration   = 60.0f;
				const float PointSize  = 40.0f;   // Screen pixel size, not world unit
				const float OffsetX    = -(Res * CellSize * 0.5f);
				const float OffsetY    = -(Res * CellSize * 0.5f);

				for (int32 Y = 0; Y < Res; ++Y)
				{
					for (int32 X = 0; X < Res; ++X)
					{
						const float M = Manager->GetMoistureAt(X, Y);
						// Green color scale: dry=dark green (0,60,0), wet=light green (50,255,50)
						const uint8 G = static_cast<uint8>(60  + M * 195.0f);
						const uint8 R = static_cast<uint8>(       M *  50.0f);
						const uint8 B = static_cast<uint8>(       M *  50.0f);
						const FVector Pos(
							OffsetY + Y * CellSize,  // grid Y (north-south) -> UE X
							OffsetX + X * CellSize,  // grid X -> UE Y
							DrawZ);
						DrawDebugPoint(World, Pos, PointSize, FColor(R, G, B), false, Duration);
					}
				}
				UE_LOG(LogMapGenCmd, Display,
					TEXT("  Debug visualization: %dx%d points drawn (%.0f sec, CellSize=%.0f UU)"),
					Res, Res, Duration, CellSize);
			}
			else
			{
				UE_LOG(LogMapGenCmd, Warning,
					TEXT("  WARNING: Debug visualization only works in PIE (World null)."));
			}
		}),
		ECVF_Default
	);

	// -------------------------------------------------------------------------
	// MapGen.ShowBiomeMap [Resolution=65] [Seed=42]
	// Biome map generation and statistics output.
	// -------------------------------------------------------------------------
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("MapGen.ShowBiomeMap"),
		TEXT("Generates and checks biome map. Arguments: Resolution Seed (default: 65 42)"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			const int32 Res  = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 65;
			const int32 Seed = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 42;

			UMapGeneratorSettings* Settings = MakeCmdSettings(Res, Seed);
			Settings->bUseContinentMask = true;

			UHeightmapGenerator* Heightmap = NewObject<UHeightmapGenerator>(
				GetTransientPackage(), NAME_None, RF_Transient);
			Heightmap->Generate(Settings);

			UBiomeManager* Manager = NewObject<UBiomeManager>(
				GetTransientPackage(), NAME_None, RF_Transient);
			const bool bOK = Manager->AssignBiomes(Settings, Heightmap);

			if (!bOK)
			{
				UE_LOG(LogMapGenCmd, Warning, TEXT("MapGen.ShowBiomeMap  --  AssignBiomes failed."));
				return;
			}

			// Count biome distribution
			TMap<EBiomeType, int32> BiomeCounts;
			BiomeCounts.Add(EBiomeType::Ocean,    0);
			BiomeCounts.Add(EBiomeType::Beach,    0);
			BiomeCounts.Add(EBiomeType::Plains,   0);
			BiomeCounts.Add(EBiomeType::Forest,   0);
			BiomeCounts.Add(EBiomeType::Hills,    0);
			BiomeCounts.Add(EBiomeType::Mountain, 0);
			BiomeCounts.Add(EBiomeType::Swamp,    0);

			const TArray<FBiomeCell>& BiomeMap = Manager->GetBiomeMap();
			for (const FBiomeCell& Cell : BiomeMap)
			{
				if (int32* Count = BiomeCounts.Find(Cell.BiomeType))
					(*Count)++;
			}
			const int32 Total = BiomeMap.Num();

			UE_LOG(LogMapGenCmd, Display,
				TEXT("MapGen.ShowBiomeMap | Res=%d Seed=%d | Total cells=%d"), Res, Seed, Total);
			UE_LOG(LogMapGenCmd, Display, TEXT("  Biome distribution:"));
			UE_LOG(LogMapGenCmd, Display, TEXT("    Ocean:    %4d  (%.1f%%)"),
				BiomeCounts[EBiomeType::Ocean],    100.0f * BiomeCounts[EBiomeType::Ocean]    / Total);
			UE_LOG(LogMapGenCmd, Display, TEXT("    Beach:    %4d  (%.1f%%)"),
				BiomeCounts[EBiomeType::Beach],    100.0f * BiomeCounts[EBiomeType::Beach]    / Total);
			UE_LOG(LogMapGenCmd, Display, TEXT("    Plains:   %4d  (%.1f%%)"),
				BiomeCounts[EBiomeType::Plains],   100.0f * BiomeCounts[EBiomeType::Plains]   / Total);
			UE_LOG(LogMapGenCmd, Display, TEXT("    Forest:   %4d  (%.1f%%)"),
				BiomeCounts[EBiomeType::Forest],   100.0f * BiomeCounts[EBiomeType::Forest]   / Total);
			UE_LOG(LogMapGenCmd, Display, TEXT("    Hills:    %4d  (%.1f%%)"),
				BiomeCounts[EBiomeType::Hills],    100.0f * BiomeCounts[EBiomeType::Hills]    / Total);
			UE_LOG(LogMapGenCmd, Display, TEXT("    Mountain: %4d  (%.1f%%)"),
				BiomeCounts[EBiomeType::Mountain], 100.0f * BiomeCounts[EBiomeType::Mountain] / Total);
			UE_LOG(LogMapGenCmd, Display, TEXT("    Swamp:    %4d  (%.1f%%)"),
				BiomeCounts[EBiomeType::Swamp],    100.0f * BiomeCounts[EBiomeType::Swamp]    / Total);

			// A few sample points
			const int32 MidX = Res / 2, MidY = Res / 2;
			const TArray<TPair<int32,int32>> SampleCoords = {
				{0, 0}, {MidX, 0}, {MidX, MidY}, {MidX, Res - 1}, {Res - 1, Res - 1}
			};

			const auto BiomeToStr = [](EBiomeType T) -> FString
			{
				switch (T)
				{
				case EBiomeType::Ocean:    return TEXT("Ocean");
				case EBiomeType::Beach:    return TEXT("Beach");
				case EBiomeType::Plains:   return TEXT("Plains");
				case EBiomeType::Forest:   return TEXT("Forest");
				case EBiomeType::Hills:    return TEXT("Hills");
				case EBiomeType::Mountain: return TEXT("Mountain");
				case EBiomeType::Swamp:    return TEXT("Swamp");
				default:                   return TEXT("Unknown");
				}
			};

			UE_LOG(LogMapGenCmd, Display, TEXT("  Sample points:"));
			for (const auto& C : SampleCoords)
			{
				FBiomeCell Cell = Manager->GetBiomeAt(C.Key, C.Value);
				UE_LOG(LogMapGenCmd, Display,
					TEXT("    (%2d,%2d) %-9s H=%.3f T=%.3f M=%.3f"),
					C.Key, C.Value,
					*BiomeToStr(Cell.BiomeType),
					Cell.Height, Cell.Temperature, Cell.Moisture);
			}

			const bool bHasOcean    = BiomeCounts[EBiomeType::Ocean]    > 0;
			const bool bHasLand     = (BiomeCounts[EBiomeType::Plains] + BiomeCounts[EBiomeType::Forest]
									 + BiomeCounts[EBiomeType::Hills]  + BiomeCounts[EBiomeType::Mountain]) > 0;

			if (GEngine)
				GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green,
					FString::Printf(
						TEXT("BiomeMap | Ocean=%.0f%% Beach=%.0f%% Forest=%.0f%% Mountain=%.0f%% Plains=%.0f%%"),
						100.0f * BiomeCounts[EBiomeType::Ocean]    / Total,
						100.0f * BiomeCounts[EBiomeType::Beach]    / Total,
						100.0f * BiomeCounts[EBiomeType::Forest]   / Total,
						100.0f * BiomeCounts[EBiomeType::Mountain] / Total,
						100.0f * BiomeCounts[EBiomeType::Plains]   / Total));

			// DrawDebug visualization: biome map with biome-specific colors
			// Ocean=Blue, Beach=Yellow, Plains=Green, Forest=Dark Green,
			// Hills=Light Green, Mountain=White, Swamp=Brown
			UWorld* World = GetPIEWorld();
			if (World)
			{
				const float CellSize  = 100.0f;
				const float DrawZ     = 10.0f;
				const float Duration  = 60.0f;
				const float PointSize = 40.0f;
				const float OffsetX   = -(Res * CellSize * 0.5f);
				const float OffsetY   = -(Res * CellSize * 0.5f);

				const auto BiomeColor = [](EBiomeType T) -> FColor
				{
					switch (T)
					{
					case EBiomeType::Ocean:    return FColor(  0,  80, 200); // Blue
					case EBiomeType::Beach:    return FColor(240, 220, 100); // Yellow
					case EBiomeType::Plains:   return FColor(100, 200,  80); // Green
					case EBiomeType::Forest:   return FColor( 20, 100,  20); // Dark Green
					case EBiomeType::Hills:    return FColor(150, 200, 100); // Light Green
					case EBiomeType::Mountain: return FColor(220, 220, 220); // White/Grey
					case EBiomeType::Swamp:    return FColor( 80,  60,  20); // Brown
					default:                   return FColor(255,   0, 255); // Magenta (unknown)
					}
				};

				for (int32 Y = 0; Y < Res; ++Y)
				{
					for (int32 X = 0; X < Res; ++X)
					{
						const FBiomeCell Cell = Manager->GetBiomeAt(X, Y);
						const FVector Pos(
							OffsetY + Y * CellSize,  // grid Y (north-south) -> UE X
							OffsetX + X * CellSize,  // grid X -> UE Y
							DrawZ);
						DrawDebugPoint(World, Pos, PointSize, BiomeColor(Cell.BiomeType), false, Duration);
					}
				}
				UE_LOG(LogMapGenCmd, Display,
					TEXT("  Debug visualization: %dx%d points drawn (%.0f sec, CellSize=%.0f UU)"),
					Res, Res, Duration, CellSize);
			}
			else
			{
				UE_LOG(LogMapGenCmd, Warning,
					TEXT("  WARNING: Debug visualization only works in PIE (World null).")	);
			}
		}),
		ECVF_Default
	);

	// -------------------------------------------------------------------------
	// MapGen.ShowHeightmap [Resolution=65] [Seed=42] [Erosion=0]
	// Heightmap visualization on white-black scale.
	// With Erosion=1 uses erosion, 0 without  --  for comparison.
	// -------------------------------------------------------------------------
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("MapGen.ShowHeightmap"),
		TEXT("Heightmap visualization on white-black scale. Arguments: Resolution Seed Erosion (default: 65 42 0)"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			const int32 Res      = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 65;
			const int32 Seed     = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 42;
			const bool bErosion  = Args.IsValidIndex(2) ? FCString::Atoi(*Args[2]) != 0 : false;

			UMapGeneratorSettings* Settings = MakeCmdSettings(Res, Seed);
			Settings->bUseContinentMask              = false;
			Settings->HeightmapConfig.bEnableErosion = bErosion;
			Settings->HeightmapConfig.ErosionIterations = 5000;

			UHeightmapGenerator* Gen = NewObject<UHeightmapGenerator>(
				GetTransientPackage(), NAME_None, RF_Transient);
			const bool bOK = Gen->Generate(Settings);

			if (!bOK)
			{
				UE_LOG(LogMapGenCmd, Warning, TEXT("MapGen.ShowHeightmap  --  Generate failed."));
				return;
			}

			// Statistics
			float MinH = FLT_MAX, MaxH = -FLT_MAX, SumH = 0.0f;
			const TArray<float>& Data = Gen->GetHeightmapData();
			for (float H : Data) { SumH += H; if (H < MinH) MinH = H; if (H > MaxH) MaxH = H; }
			const float AvgH = SumH / Data.Num();

			UE_LOG(LogMapGenCmd, Display,
				TEXT("MapGen.ShowHeightmap | Res=%d Seed=%d Erosion=%s"),
				Res, Seed, bErosion ? TEXT("ON") : TEXT("OFF"));
			UE_LOG(LogMapGenCmd, Display,
				TEXT("  Min=%.4f | Max=%.4f | Avg=%.4f"), MinH, MaxH, AvgH);

			// DrawDebug visualization: white=high, black=deep
			UWorld* World = GetPIEWorld();
			if (World)
			{
				const float CellSize  = 100.0f;
				const float DrawZ     = 10.0f;
				const float Duration  = 60.0f;
				const float PointSize = 40.0f;
				const float OffsetX   = -(Res * CellSize * 0.5f);
				const float OffsetY   = -(Res * CellSize * 0.5f);

				for (int32 Y = 0; Y < Res; ++Y)
				{
					for (int32 X = 0; X < Res; ++X)
					{
						const float H  = Gen->GetHeightAt(X, Y);
						const uint8 V  = static_cast<uint8>(H * 255.0f); // white=high
						const FVector Pos(
							OffsetY + Y * CellSize,
							OffsetX + X * CellSize,
							DrawZ);
						DrawDebugPoint(World, Pos, PointSize, FColor(V, V, V), false, Duration);
					}
				}
				UE_LOG(LogMapGenCmd, Display,
					TEXT("  Debug visualization: %dx%d points drawn | Erosion=%s"),
					Res, Res, bErosion ? TEXT("ON") : TEXT("OFF"));

				if (GEngine)
					GEngine->AddOnScreenDebugMessage(-1, 15.0f, bErosion ? FColor::Cyan : FColor::White,
						FString::Printf(
							TEXT("Heightmap | Erosion=%s | Min=%.3f Max=%.3f Avg=%.3f"),
							bErosion ? TEXT("ON") : TEXT("OFF"), MinH, MaxH, AvgH));
			}
			else
			{
				UE_LOG(LogMapGenCmd, Warning,
					TEXT("  WARNING: Debug visualization only works in PIE (World null)."));
			}
		}),
		ECVF_Default
	);

	// -------------------------------------------------------------------------
	// MapGen.TestHeightmapConversion
	// Tests the ConvertHeightmapToUint16 conversion with a few sample values.
	// -------------------------------------------------------------------------
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("MapGen.TestHeightmapConversion"),
		TEXT("Tests heightmap uint16 conversion. No arguments."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			ULandscapeBuilder* Builder = NewObject<ULandscapeBuilder>(
				GetTransientPackage(), NAME_None, RF_Transient);

			// Test samples
			const TArray<float> TestValues = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f, -0.5f, 1.5f };
			const TArray<uint16> Result = Builder->ConvertHeightmapToUint16(TestValues);

			UE_LOG(LogMapGenCmd, Display, TEXT("MapGen.TestHeightmapConversion:"));
			UE_LOG(LogMapGenCmd, Display, TEXT("  Float  Ã¢â€ '  uint16   [expected]"));
			UE_LOG(LogMapGenCmd, Display, TEXT("  0.00   Ã¢â€ '  %5d    [0]"),       Result[0]);
			UE_LOG(LogMapGenCmd, Display, TEXT("  0.25   Ã¢â€ '  %5d    [16383]"),   Result[1]);
			UE_LOG(LogMapGenCmd, Display, TEXT("  0.50   Ã¢â€ '  %5d    [32767]"),   Result[2]);
			UE_LOG(LogMapGenCmd, Display, TEXT("  0.75   Ã¢â€ '  %5d    [49151]"),   Result[3]);
			UE_LOG(LogMapGenCmd, Display, TEXT("  1.00   Ã¢â€ '  %5d    [65535]"),   Result[4]);
			UE_LOG(LogMapGenCmd, Display, TEXT("  -0.50  Ã¢â€ '  %5d    [0 clamp]"), Result[5]);
			UE_LOG(LogMapGenCmd, Display, TEXT("  1.50   Ã¢â€ '  %5d    [65535 clamp]"), Result[6]);

			// Checks
			const bool bOK =
				Result[0] == 0     &&
				Result[4] == 65535 &&
				Result[5] == 0     &&
				Result[6] == 65535;

			UE_LOG(LogMapGenCmd, Display, TEXT("  Result: %s"),
				bOK ? TEXT("OK  --  conversion correct") : TEXT("ERROR  --  conversion incorrect!"));

			if (GEngine)
				GEngine->AddOnScreenDebugMessage(-1, 8.0f,
					bOK ? FColor::Green : FColor::Red,
					FString::Printf(TEXT("HeightmapConversion: %s | 0Ã¢â€ '%d | 0.5Ã¢â€ '%d | 1Ã¢â€ '%d"),
						bOK ? TEXT("OK") : TEXT("ERROR"),
						Result[0], Result[2], Result[4]));
		}),
		ECVF_Default
	);

	// -------------------------------------------------------------------------
	// MapGen.ShowBlendWeights [X=32] [Y=32] [Resolution=65] [Seed=42]
	// Output the BiomeBlendWeights values of a given cell.
	// -------------------------------------------------------------------------
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("MapGen.ShowBlendWeights"),
		TEXT("Outputs blend weight values of a cell. Arguments: X Y Resolution Seed (default: 32 32 65 42)"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			const int32 QueryX = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 32;
			const int32 QueryY = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 32;
			const int32 Res    = Args.IsValidIndex(2) ? FCString::Atoi(*Args[2]) : 65;
			const int32 Seed   = Args.IsValidIndex(3) ? FCString::Atoi(*Args[3]) : 42;

			UMapGeneratorSettings* Settings = MakeCmdSettings(Res, Seed);
			Settings->bUseContinentMask = true;
			Settings->SeaLevel          = 0.3f;

			UHeightmapGenerator* Heightmap = NewObject<UHeightmapGenerator>(
				GetTransientPackage(), NAME_None, RF_Transient);
			Heightmap->Generate(Settings);

			UBiomeManager* Manager = NewObject<UBiomeManager>(
				GetTransientPackage(), NAME_None, RF_Transient);
			const bool bOK = Manager->AssignBiomes(Settings, Heightmap);

			if (!bOK)
			{
				UE_LOG(LogMapGenCmd, Warning, TEXT("MapGen.ShowBlendWeights -- AssignBiomes failed."));
				return;
			}

			const auto BiomeToStr = [](EBiomeType T) -> FString
			{
				switch (T)
				{
				case EBiomeType::Ocean:    return TEXT("Ocean");
				case EBiomeType::Beach:    return TEXT("Beach");
				case EBiomeType::Plains:   return TEXT("Plains");
				case EBiomeType::Forest:   return TEXT("Forest");
				case EBiomeType::Hills:    return TEXT("Hills");
				case EBiomeType::Mountain: return TEXT("Mountain");
				case EBiomeType::Swamp:    return TEXT("Swamp");
				default:                   return TEXT("Unknown");
				}
			};

			const TMap<EBiomeType, float> Weights = Manager->GetBlendWeightsAt(QueryX, QueryY);
			const FBiomeCell              Cell    = Manager->GetBiomeAt(QueryX, QueryY);

			UE_LOG(LogMapGenCmd, Display,
				TEXT("MapGen.ShowBlendWeights | (%d,%d) Res=%d Seed=%d"),
				QueryX, QueryY, Res, Seed);
			UE_LOG(LogMapGenCmd, Display,
				TEXT("  Primary biome: %s | H=%.3f T=%.3f M=%.3f"),
				*BiomeToStr(Cell.BiomeType), Cell.Height, Cell.Temperature, Cell.Moisture);
			UE_LOG(LogMapGenCmd, Display, TEXT("  BlendWeights (%d biomes):"), Weights.Num());

			// Output weights in descending order
			TArray<TPair<EBiomeType, float>> Sorted(Weights.Array());
			Sorted.Sort([](const TPair<EBiomeType,float>& A, const TPair<EBiomeType,float>& B)
			{
				return A.Value > B.Value;
			});

			float WeightSum = 0.0f;
			for (const auto& Pair : Sorted)
			{
				UE_LOG(LogMapGenCmd, Display,
					TEXT("    %-9s  %.4f  (%.1f%%)"),
					*BiomeToStr(Pair.Key), Pair.Value, Pair.Value * 100.0f);
				WeightSum += Pair.Value;
			}
			UE_LOG(LogMapGenCmd, Display, TEXT("  Sum: %.6f (expected: ~1.0)"), WeightSum);

			if (GEngine)
			{
				FString WeightStr;
				for (const auto& Pair : Sorted)
				{
					WeightStr += FString::Printf(TEXT("%s:%.2f "), *BiomeToStr(Pair.Key), Pair.Value);
				}
				GEngine->AddOnScreenDebugMessage(-1, 12.0f, FColor::Yellow,
					FString::Printf(TEXT("BlendWeights (%d,%d): %s| Sum=%.4f"),
						QueryX, QueryY, *WeightStr, WeightSum));
			}
		}),
		ECVF_Default
	);

	// -------------------------------------------------------------------------
	// MapGen.BuildLandscape [Resolution=65] [Seed=42]
	// Create Landscape actor in PIE with the full pipeline.
	// -------------------------------------------------------------------------
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("MapGen.BuildLandscape"),
		TEXT("Create Landscape in PIE. Arguments: Resolution Seed (default: 65 42)"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			const int32 Res  = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 128;
			const int32 Seed = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 42;

			UWorld* World = GetPIEWorld();
			if (!World)
			{
				UE_LOG(LogMapGenCmd, Warning, TEXT("MapGen.BuildLandscape -- can only run in PIE."));
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Red,
					TEXT("BuildLandscape: No PIE world! Start the game."));
				return;
			}

			UMapGeneratorSettings* Settings = MakeCmdSettings(Res, Seed);
			Settings->QuadSize = 100;  // 1m/quad -> 127m wide landscape

			UHeightmapGenerator* Heightmap = NewObject<UHeightmapGenerator>(
				GetTransientPackage(), NAME_None, RF_Transient);
			const bool bGenOK = Heightmap->Generate(Settings);
			if (!bGenOK)
			{
				UE_LOG(LogMapGenCmd, Warning, TEXT("MapGen.BuildLandscape -- Generate failed."));
				return;
			}

			// Attach ULandscapeBuilder to a PIE actor,
			// so that GetWorld() returns the correct world context.
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AActor* HostActor = World->SpawnActor<AActor>(
				AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
			if (!HostActor)
			{
				UE_LOG(LogMapGenCmd, Warning, TEXT("MapGen.BuildLandscape -- HostActor spawn failed."));
				return;
			}

			ULandscapeBuilder* Builder = NewObject<ULandscapeBuilder>(
				HostActor, NAME_None, RF_Transient);
			Builder->RegisterComponent();

			const bool bOK = Builder->BuildLandscape(Settings, Heightmap);

			if (bOK)
			{
				UE_LOG(LogMapGenCmd, Display,
					TEXT("MapGen.BuildLandscape -- Landscape created. Res=%d Seed=%d QuadSize=%d"),
					Res, Seed, Settings->QuadSize);
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green,
					FString::Printf(TEXT("BuildLandscape OK | Res=%d Seed=%d"), Res, Seed));

				// Move camera: to landscape center, from above
				if (APlayerController* PC = World->GetFirstPlayerController())
				{
					// Landscape position after ConfigureLandscapeTransform:
					// Actor loc = (-HalfSize, -HalfSize, 0), extent: (-HalfSize..+HalfSize)
					// XY center: (0, 0)
					const float HalfSize = (Res - 1) * Settings->QuadSize * 0.5f;
					const FVector LandscapeCenter(0.0f, 0.0f, 0.0f);
					const FVector TargetPos(LandscapeCenter.X, LandscapeCenter.Y, HalfSize * 3.0f);
					const FRotator TargetRot(-80.0f, 0.0f, 0.0f);
					if (APawn* Pawn = PC->GetPawn())
					{
						Pawn->SetActorLocation(TargetPos, false, nullptr, ETeleportType::TeleportPhysics);
						PC->SetControlRotation(TargetRot);
						const FVector NewLoc = Pawn->GetActorLocation();
						if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow,
							FString::Printf(TEXT("Camera: (%.0f, %.0f, %.0f) | Landscape center: (0, 0, 0)"),
								NewLoc.X, NewLoc.Y, NewLoc.Z));
					}
				}
			}
			else
			{
				UE_LOG(LogMapGenCmd, Warning, TEXT("MapGen.BuildLandscape -- BuildLandscape failed."));
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Red,
					TEXT("BuildLandscape ERROR -- see Output Log"));
				HostActor->Destroy();
			}
		}),
		ECVF_Default
	);

	// -------------------------------------------------------------------------
	// MapGen.BuildFullPipeline [Resolution=65] [Seed=42] [Frequency=0.2] [Octaves=6] [Persistence=0.5]
	// -------------------------------------------------------------------------
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("MapGen.BuildFullPipeline"),
		TEXT("Full pipeline in PIE: Generate -> AssignBiomes -> BuildLandscape. Arguments: Resolution Seed Frequency Octaves Persistence (default: 253 42 0.2 6 0.5)"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			const int32 Res         = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 253;
			const int32 Seed        = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 42;
			const float Freq        = Args.IsValidIndex(2) ? FCString::Atof(*Args[2]) : 0.01f;
			const int32 Octaves     = Args.IsValidIndex(3) ? FCString::Atoi(*Args[3]) : 6;
			const float Persistence = Args.IsValidIndex(4) ? FCString::Atof(*Args[4]) : 0.5f;

			UWorld* World = GetPIEWorld();
			if (!World)
			{
				UE_LOG(LogMapGenCmd, Warning, TEXT("MapGen.BuildFullPipeline -- can only run in PIE."));
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Red,
					TEXT("BuildFullPipeline: No PIE world! Start the game."));
				return;
			}

			// 0. Destroy any previously generated landscape and host actors
			{
				TArray<AActor*> ToDestroy;
				for (TActorIterator<ALandscape> It(World); It; ++It)
					ToDestroy.Add(*It);
				for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
					ToDestroy.Add(*It);
				int32 Destroyed = 0;
				for (AActor* A : ToDestroy)
				{
					if (A && !A->IsPendingKillPending())
					{
						A->Destroy();
						++Destroyed;
					}
				}
				if (Destroyed > 0)
					UE_LOG(LogMapGenCmd, Display,
						TEXT("MapGen.BuildFullPipeline -- cleared %d previous landscape actor(s)."), Destroyed);
			}

			// 1. Settings
			UMapGeneratorSettings* Settings = MakeCmdSettings(Res, Seed);
			Settings->QuadSize = 100;
			Settings->HeightmapConfig.Frequency    = FMath::Clamp(Freq,        0.001f, 10.0f);
			Settings->HeightmapConfig.Octaves      = FMath::Clamp(Octaves,     1,      10);
			Settings->HeightmapConfig.Persistence  = FMath::Clamp(Persistence, 0.1f,   1.0f);

			// 2. HeightmapGenerator.Generate()
			UHeightmapGenerator* Heightmap = NewObject<UHeightmapGenerator>(
				GetTransientPackage(), NAME_None, RF_Transient);
			const bool bGenOK = Heightmap->Generate(Settings);
			if (!bGenOK)
			{
				UE_LOG(LogMapGenCmd, Warning, TEXT("MapGen.BuildFullPipeline -- HeightmapGenerator.Generate() failed."));
				return;
			}
			UE_LOG(LogMapGenCmd, Display,
				TEXT("MapGen.BuildFullPipeline -- [1/3] Generate() done. Res=%dx%d Seed=%d Freq=%.3f Octaves=%d Persistence=%.2f"),
				Res, Res, Seed, Settings->HeightmapConfig.Frequency, Settings->HeightmapConfig.Octaves, Settings->HeightmapConfig.Persistence);

			// 3. BiomeManager.AssignBiomes()
			UBiomeManager* Biome = NewObject<UBiomeManager>(
				GetTransientPackage(), NAME_None, RF_Transient);
			const bool bBiomeOK = Biome->AssignBiomes(Settings, Heightmap);
			if (!bBiomeOK)
			{
				UE_LOG(LogMapGenCmd, Warning, TEXT("MapGen.BuildFullPipeline -- BiomeManager.AssignBiomes() failed."));
				return;
			}
			UE_LOG(LogMapGenCmd, Display,
				TEXT("MapGen.BuildFullPipeline -- [2/3] AssignBiomes() done. BiomeMap: %d cells"),
				Biome->GetBiomeMap().Num());

			// 4. LandscapeBuilder attached to PIE actor (for GetWorld())
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AActor* HostActor = World->SpawnActor<AActor>(
				AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
			if (!HostActor)
			{
				UE_LOG(LogMapGenCmd, Warning, TEXT("MapGen.BuildFullPipeline -- HostActor spawn failed."));
				return;
			}

			ULandscapeBuilder* Builder = NewObject<ULandscapeBuilder>(
				HostActor, NAME_None, RF_Transient);
			Builder->RegisterComponent();

			// Load Layer Info assets from /Game/Landscape/Layers/
			const bool bLayersLoaded = Builder->LoadLayerInfoAssets();
			UE_LOG(LogMapGenCmd, Display,
				TEXT("MapGen.BuildFullPipeline -- Layer Info loaded: %s (%d / 7 biomes)"),
				bLayersLoaded ? TEXT("COMPLETE") : TEXT("INCOMPLETE"),
				Builder->LayerInfoAssets.Num());

			const bool bBuildOK = Builder->BuildLandscape(Settings, Heightmap, Biome);

			if (bBuildOK)
			{
				const int32 CellCount = Res * Res;
				UE_LOG(LogMapGenCmd, Display,
					TEXT("MapGen.BuildFullPipeline -- [3/3] BuildLandscape() done. Landscape created!"));
				UE_LOG(LogMapGenCmd, Display,
					TEXT("  Res=%dx%d | Cells: %d | Seed=%d | QuadSize=%d cm | LayerInfo: %d biomes"),
					Res, Res, CellCount, Seed, Settings->QuadSize, Builder->LayerInfoAssets.Num());
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green,
					FString::Printf(TEXT("BuildFullPipeline OK | Res=%d Seed=%d | Biomes: %d"),
						Res, Seed, Builder->LayerInfoAssets.Num()));

				}
			else
			{
				UE_LOG(LogMapGenCmd, Warning, TEXT("MapGen.BuildFullPipeline -- BuildLandscape() failed. See Output Log."));
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Red,
					TEXT("BuildFullPipeline ERROR -- see Output Log"));
				HostActor->Destroy();
			}
		}),
		ECVF_Default
	);

	// -------------------------------------------------------------------------
	// MapGen.TestClimateZone
	// Tests DetermineClimateZone() with representative cells for each zone type
	// -------------------------------------------------------------------------
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("MapGen.TestClimateZone"),
		TEXT("Tests DetermineClimateZone() with representative cells. Prints results to Output Log and screen."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
				GetTransientPackage(), NAME_None, RF_Transient);

			// Helper: zone enum to display string
			auto ZoneName = [](EClimateZone Z) -> FString
			{
				switch (Z)
				{
					case EClimateZone::Tropical:    return TEXT("Tropical");
					case EClimateZone::Temperate:   return TEXT("Temperate");
					case EClimateZone::Continental: return TEXT("Continental");
					case EClimateZone::Subarctic:   return TEXT("Subarctic");
					default:                        return TEXT("Unknown");
				}
			};

			// Test cells covering all four zones
			struct FTestCase { FString Label; float Temp; float Moisture; float Height; EClimateZone Expected; };
			const TArray<FTestCase> Cases =
			{
				{ TEXT("Tropical   (hot+wet)"),       0.9f, 0.8f, 0.2f, EClimateZone::Tropical    },
				{ TEXT("Temperate  (hot+dry)"),       0.9f, 0.2f, 0.2f, EClimateZone::Temperate   },
				{ TEXT("Temperate  (medium)"),        0.6f, 0.4f, 0.2f, EClimateZone::Temperate   },
				{ TEXT("Continental(low temp)"),      0.3f, 0.4f, 0.2f, EClimateZone::Continental },
				{ TEXT("Subarctic  (very cold)"),     0.1f, 0.3f, 0.2f, EClimateZone::Subarctic   },
				{ TEXT("Height fix (0.5->0.3 cold)"), 0.5f, 0.4f, 0.8f, EClimateZone::Continental },
				{ TEXT("Height fix (0.9->0.7 hot)"),  0.9f, 0.8f, 0.8f, EClimateZone::Temperate   },
			};

			UE_LOG(LogMapGenCmd, Display, TEXT("=== MapGen.TestClimateZone ==="));
			int32 Passed = 0;
			for (const FTestCase& C : Cases)
			{
				FBiomeCell Cell;
				Cell.Temperature = C.Temp;
				Cell.Moisture    = C.Moisture;
				Cell.Height      = C.Height;

				EClimateZone Result = Manager->DetermineClimateZone(Cell);
				bool bOK = (Result == C.Expected);
				if (bOK) Passed++;

				UE_LOG(LogMapGenCmd, Display,
					TEXT("  [%s] %-30s T=%.2f M=%.2f H=%.2f  =>  %-12s  %s"),
					bOK ? TEXT("OK") : TEXT("!!"),
					*C.Label, C.Temp, C.Moisture, C.Height,
					*ZoneName(Result),
					bOK ? TEXT("") : *FString::Printf(TEXT("(expected: %s)"), *ZoneName(C.Expected)));
			}

			UE_LOG(LogMapGenCmd, Display, TEXT("=== Result: %d / %d passed ==="), Passed, Cases.Num());

			FColor Color = (Passed == Cases.Num()) ? FColor::Green : FColor::Orange;
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.0f, Color,
				FString::Printf(TEXT("MapGen.TestClimateZone: %d/%d passed"), Passed, Cases.Num()));
		}),
		ECVF_Default
	);

	// -------------------------------------------------------------------------
	// MapGen.ShowClimateZones [Resolution=32] [Seed=42] [Frequency=0.5]
	// Runs the full pipeline and visualizes climate zones with debug spheres
	// Tropical=Red, Temperate=Green, Continental=Blue, Subarctic=White
	// -------------------------------------------------------------------------
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("MapGen.ShowClimateZones"),
		TEXT("Generates a climate zone map and visualizes it with debug spheres in PIE. Args: Resolution Seed Frequency (default: 32 42 0.5). Tropical=Red, Temperate=Green, Continental=Blue, Subarctic=White."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			const int32 Res   = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 32;
			const int32 Seed  = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 42;
			const float Freq  = Args.IsValidIndex(2) ? FCString::Atof(*Args[2]) : 0.5f;

			UWorld* World = GetPIEWorld();
			if (!World)
			{
				UE_LOG(LogMapGenCmd, Warning, TEXT("MapGen.ShowClimateZones -- PIE world required."));
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor::Red,
					TEXT("ShowClimateZones: Start PIE first."));
				return;
			}

			// 1. Generate heightmap
			UMapGeneratorSettings* Settings = MakeCmdSettings(Res, Seed);
			Settings->HeightmapConfig.Frequency = FMath::Clamp(Freq, 0.01f, 10.0f);

			UHeightmapGenerator* Heightmap = NewObject<UHeightmapGenerator>(
				GetTransientPackage(), NAME_None, RF_Transient);
			if (!Heightmap->Generate(Settings))
			{
				UE_LOG(LogMapGenCmd, Warning, TEXT("MapGen.ShowClimateZones -- Generate() failed."));
				return;
			}

			// 2. Assign biomes
			UBiomeManager* Biome = NewObject<UBiomeManager>(
				GetTransientPackage(), NAME_None, RF_Transient);
			if (!Biome->AssignBiomes(Settings, Heightmap))
			{
				UE_LOG(LogMapGenCmd, Warning, TEXT("MapGen.ShowClimateZones -- AssignBiomes() failed."));
				return;
			}

			// 3. Calculate climate zones
			UClimateZoneManager* Climate = NewObject<UClimateZoneManager>(
				GetTransientPackage(), NAME_None, RF_Transient);
			if (!Climate->CalculateClimateZones(Biome->GetBiomeMap(), Settings))
			{
				UE_LOG(LogMapGenCmd, Warning, TEXT("MapGen.ShowClimateZones -- CalculateClimateZones() failed."));
				return;
			}

			// 4. Zone color map
			auto ZoneColor = [](EClimateZone Z) -> FColor
			{
				switch (Z)
				{
					case EClimateZone::Tropical:    return FColor::Red;
					case EClimateZone::Temperate:   return FColor::Green;
					case EClimateZone::Continental: return FColor::Blue;
					case EClimateZone::Subarctic:   return FColor::White;
					default:                        return FColor::Black;
				}
			};

			// 5. Draw debug spheres
			const float CellSize  = 200.0f;
			const float Radius    = CellSize * 0.4f;
			const float Duration  = 30.0f;
			const float OriginX   = -(Res - 1) * CellSize * 0.5f;
			const float OriginY   = -(Res - 1) * CellSize * 0.5f;

			TMap<EClimateZone, int32> ZoneCounts;
			for (int32 y = 0; y < Res; ++y)
			{
				for (int32 x = 0; x < Res; ++x)
				{
					const int32 Idx = y * Res + x;
					EClimateZone Zone = Climate->GetClimateZoneAt(Idx);
					ZoneCounts.FindOrAdd(Zone)++;

					FVector Pos(OriginX + x * CellSize, OriginY + y * CellSize, 50.0f);
					DrawDebugSphere(World, Pos, Radius, 6, ZoneColor(Zone), false, Duration);
				}
			}

			// 6. Summary log
			UE_LOG(LogMapGenCmd, Display,
				TEXT("MapGen.ShowClimateZones -- Res=%dx%d | Tropical=%d Temperate=%d Continental=%d Subarctic=%d"),
				Res, Res,
				ZoneCounts.FindOrAdd(EClimateZone::Tropical),
				ZoneCounts.FindOrAdd(EClimateZone::Temperate),
				ZoneCounts.FindOrAdd(EClimateZone::Continental),
				ZoneCounts.FindOrAdd(EClimateZone::Subarctic));

			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Cyan,
				FString::Printf(TEXT("ShowClimateZones | T=%d Te=%d C=%d S=%d"),
					ZoneCounts.FindOrAdd(EClimateZone::Tropical),
					ZoneCounts.FindOrAdd(EClimateZone::Temperate),
					ZoneCounts.FindOrAdd(EClimateZone::Continental),
					ZoneCounts.FindOrAdd(EClimateZone::Subarctic)));
		}),
		ECVF_Default
	);

	// -------------------------------------------------------------------------
	// MapGen.ShowTemperatureGradient [Resolution=32] [Seed=42] [TimeOfYear=0.5]
	// Runs the full pipeline and visualizes seasonal temperature as a color gradient.
	// Cold=Blue (0,0,255), Hot=Red (255,0,0). Uses GetTemperatureAt() with TimeOfYear.
	// -------------------------------------------------------------------------
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("MapGen.ShowTemperatureGradient"),
		TEXT("Visualizes seasonal temperature gradient in PIE. Args: Resolution Seed TimeOfYear (default: 32 42 0.5). Cold=Blue, Hot=Red."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			const int32 Res        = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 32;
			const int32 Seed       = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 42;
			const float TimeOfYear = Args.IsValidIndex(2) ? FCString::Atof(*Args[2]) : 0.5f;

			UWorld* World = GetPIEWorld();
			if (!World)
			{
				UE_LOG(LogMapGenCmd, Warning, TEXT("MapGen.ShowTemperatureGradient -- PIE world required."));
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor::Red,
					TEXT("ShowTemperatureGradient: Start PIE first."));
				return;
			}

			// 1. Generate heightmap
			UMapGeneratorSettings* Settings = MakeCmdSettings(Res, Seed);

			UHeightmapGenerator* Heightmap = NewObject<UHeightmapGenerator>(
				GetTransientPackage(), NAME_None, RF_Transient);
			if (!Heightmap->Generate(Settings))
			{
				UE_LOG(LogMapGenCmd, Warning, TEXT("MapGen.ShowTemperatureGradient -- Generate() failed."));
				return;
			}

			// 2. Assign biomes
			UBiomeManager* Biome = NewObject<UBiomeManager>(
				GetTransientPackage(), NAME_None, RF_Transient);
			if (!Biome->AssignBiomes(Settings, Heightmap))
			{
				UE_LOG(LogMapGenCmd, Warning, TEXT("MapGen.ShowTemperatureGradient -- AssignBiomes() failed."));
				return;
			}

			// 3. Calculate climate zones (also fills TemperatureMap)
			UClimateZoneManager* Climate = NewObject<UClimateZoneManager>(
				GetTransientPackage(), NAME_None, RF_Transient);
			if (!Climate->CalculateClimateZones(Biome->GetBiomeMap(), Settings))
			{
				UE_LOG(LogMapGenCmd, Warning, TEXT("MapGen.ShowTemperatureGradient -- CalculateClimateZones() failed."));
				return;
			}

			// 4. Draw debug points: Cold=Blue (0,0,255), Hot=Red (255,0,0)
			const float CellSize  = 200.0f;
			const float Duration  = 30.0f;
			const float PointSize = 40.0f;
			const float OriginX   = -(Res - 1) * CellSize * 0.5f;
			const float OriginY   = -(Res - 1) * CellSize * 0.5f;

			float MinTemp = 1.0f, MaxTemp = 0.0f, SumTemp = 0.0f;

			for (int32 y = 0; y < Res; ++y)
			{
				for (int32 x = 0; x < Res; ++x)
				{
					const int32 Idx = y * Res + x;
					const float T   = Climate->GetTemperatureAt(Idx, TimeOfYear);
					if (T < MinTemp) MinTemp = T;
					if (T > MaxTemp) MaxTemp = T;
					SumTemp += T;

					// Cold=Blue -> Hot=Red linear interpolation
					const uint8 R = static_cast<uint8>(T * 255.0f);
					const uint8 B = static_cast<uint8>((1.0f - T) * 255.0f);

					FVector Pos(OriginX + x * CellSize, OriginY + y * CellSize, 50.0f);
					DrawDebugPoint(World, Pos, PointSize, FColor(R, 0, B), false, Duration);
				}
			}

			const float AvgTemp = SumTemp / static_cast<float>(Res * Res);
			const FString Season = (TimeOfYear < 0.13f || TimeOfYear > 0.87f) ? TEXT("Winter")
			                     : (TimeOfYear < 0.38f)                        ? TEXT("Spring")
			                     : (TimeOfYear < 0.63f)                        ? TEXT("Summer")
			                                                                   : TEXT("Autumn");

			UE_LOG(LogMapGenCmd, Display,
				TEXT("MapGen.ShowTemperatureGradient | Res=%dx%d Seed=%d TimeOfYear=%.2f (%s)"),
				Res, Res, Seed, TimeOfYear, *Season);
			UE_LOG(LogMapGenCmd, Display,
				TEXT("  Min=%.4f | Max=%.4f | Avg=%.4f | Cold=Blue Hot=Red"),
				MinTemp, MaxTemp, AvgTemp);

			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow,
				FString::Printf(
					TEXT("TempGradient %s | Min=%.3f Max=%.3f Avg=%.3f"),
					*Season, MinTemp, MaxTemp, AvgTemp));
		}),
		ECVF_Default
	);

	// -------------------------------------------------------------------------
	// MapGen.TestSeasons [BaseTemp=0.5]
	// Tests CalculateSeasonalTemperature() with default SeasonProfile.
	// Prints winter, spring, summer, autumn temperatures for a given base value.
	// -------------------------------------------------------------------------
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("MapGen.TestSeasons"),
		TEXT("Tests seasonal temperature interpolation. Argument: BaseTemp (default: 0.5). "
		     "Prints winter/spring/summer/autumn temperatures to Output Log and screen."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			const float BaseTemp = Args.IsValidIndex(0) ? FCString::Atof(*Args[0]) : 0.5f;

			UClimateZoneManager* Manager = NewObject<UClimateZoneManager>(
				GetTransientPackage(), NAME_None, RF_Transient);

			// Four evenly spaced season samples
			// TimeOfYear: 0.0=winter, 0.25=spring, 0.5=summer, 0.75=autumn
			const float TempWinter = Manager->CalculateSeasonalTemperature(BaseTemp, 0.00f);
			const float TempSpring = Manager->CalculateSeasonalTemperature(BaseTemp, 0.25f);
			const float TempSummer = Manager->CalculateSeasonalTemperature(BaseTemp, 0.50f);
			const float TempAutumn = Manager->CalculateSeasonalTemperature(BaseTemp, 0.75f);

			const bool bSummerWarmest = (TempSummer >= TempSpring) && (TempSummer >= TempAutumn) && (TempSummer >= TempWinter);
			const bool bWinterColdest = (TempWinter <= TempSpring) && (TempWinter <= TempAutumn) && (TempWinter <= TempSummer);
			const bool bAllInRange    = (TempWinter >= 0.0f && TempWinter <= 1.0f)
			                         && (TempSpring >= 0.0f && TempSpring <= 1.0f)
			                         && (TempSummer >= 0.0f && TempSummer <= 1.0f)
			                         && (TempAutumn >= 0.0f && TempAutumn <= 1.0f);

			UE_LOG(LogMapGenCmd, Display, TEXT("=== MapGen.TestSeasons | BaseTemp=%.3f ==="), BaseTemp);
			UE_LOG(LogMapGenCmd, Display, TEXT("  SeasonProfile: Summer=%.2f  Winter=%.2f"),
				Manager->SeasonProfile.SummerTempModifier,
				Manager->SeasonProfile.WinterTempModifier);
			UE_LOG(LogMapGenCmd, Display, TEXT("  Winter (0.00): %.4f"), TempWinter);
			UE_LOG(LogMapGenCmd, Display, TEXT("  Spring (0.25): %.4f"), TempSpring);
			UE_LOG(LogMapGenCmd, Display, TEXT("  Summer (0.50): %.4f"), TempSummer);
			UE_LOG(LogMapGenCmd, Display, TEXT("  Autumn (0.75): %.4f"), TempAutumn);
			UE_LOG(LogMapGenCmd, Display, TEXT("  Checks: SummerWarmest=%s  WinterColdest=%s  AllInRange=%s"),
				bSummerWarmest ? TEXT("OK") : TEXT("FAIL"),
				bWinterColdest ? TEXT("OK") : TEXT("FAIL"),
				bAllInRange    ? TEXT("OK") : TEXT("FAIL"));

			const bool bAllOK = bSummerWarmest && bWinterColdest && bAllInRange;
			UE_LOG(LogMapGenCmd, Display, TEXT("=== Result: %s ==="),
				bAllOK ? TEXT("OK") : TEXT("FAIL"));

			FColor Color = bAllOK ? FColor::Green : FColor::Orange;
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 12.0f, Color,
				FString::Printf(
					TEXT("TestSeasons Base=%.2f | W=%.3f Sp=%.3f Su=%.3f Au=%.3f | %s"),
					BaseTemp, TempWinter, TempSpring, TempSummer, TempAutumn,
					bAllOK ? TEXT("OK") : TEXT("FAIL")));
		}),
		ECVF_Default
	);

	// -------------------------------------------------------------------------
	// MapGen.ShowRiverPaths [Resolution=65] [Seed=42] [RiverCount=5]
	// Traces river paths from sources and draws them as blue lines.
	// -------------------------------------------------------------------------
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("MapGen.ShowRiverPaths"),
		TEXT("Traces river paths from sources and draws them as blue lines in PIE. Args: Resolution Seed RiverCount (default: 65 42 5)."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			const int32 Res        = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 65;
			const int32 Seed       = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 42;
			const int32 RiverCount = Args.IsValidIndex(2) ? FCString::Atoi(*Args[2]) : 5;

			UWorld* World = GetPIEWorld();
			if (!World)
			{
				UE_LOG(LogMapGenCmd, Warning, TEXT("MapGen.ShowRiverPaths -- PIE world required."));
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor::Red, TEXT("ShowRiverPaths: Start PIE first."));
				return;
			}

			UMapGeneratorSettings* Settings = MakeCmdSettings(Res, Seed);
			Settings->MaxRiverCount = FMath::Max(1, RiverCount);

			UHeightmapGenerator* Heightmap = NewObject<UHeightmapGenerator>(GetTransientPackage(), NAME_None, RF_Transient);
			if (!Heightmap->Generate(Settings)) { UE_LOG(LogMapGenCmd, Warning, TEXT("ShowRiverPaths -- Generate() failed.")); return; }

			UWaterSystemBuilder* Water = NewObject<UWaterSystemBuilder>(GetTransientPackage(), NAME_None, RF_Transient);
			TArray<FIntPoint> Sources = Water->FindRiverSources(Heightmap, Settings);

			const float CellSize = 100.0f;
			const float OriginX  = -(Res - 1) * CellSize * 0.5f;
			const float OriginY  = -(Res - 1) * CellSize * 0.5f;
			const float Duration = 30.0f;

			int32 TotalPathCells = 0;
			for (const FIntPoint& Source : Sources)
			{
				TArray<FIntPoint> Path = Water->TraceRiverPath(Heightmap, Settings, Source);
				TotalPathCells += Path.Num();

				for (int32 i = 0; i < Path.Num() - 1; ++i)
				{
					const FVector A(OriginX + Path[i].X   * CellSize, OriginY + Path[i].Y   * CellSize, 50.0f);
					const FVector B(OriginX + Path[i+1].X * CellSize, OriginY + Path[i+1].Y * CellSize, 50.0f);
					DrawDebugLine(World, A, B, FColor::Blue, false, Duration, 0, 8.0f);
				}

				if (Path.Num() > 0)
				{
					const FVector SrcPos(OriginX + Source.X * CellSize, OriginY + Source.Y * CellSize, 80.0f);
					DrawDebugSphere(World, SrcPos, CellSize * 0.5f, 6, FColor::Red, false, Duration);
				}
			}

			UE_LOG(LogMapGenCmd, Display, TEXT("MapGen.ShowRiverPaths -- %d rivers | %d total path cells | Res=%d Seed=%d"),
				Sources.Num(), TotalPathCells, Res, Seed);
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 12.0f, FColor::Blue,
				FString::Printf(TEXT("RiverPaths: %d rivers | %d cells"), Sources.Num(), TotalPathCells));
		}),
		ECVF_Default
	);

	// -------------------------------------------------------------------------
	// MapGen.ShowLakes [Resolution=65] [Seed=42] [LakeCount=5]
	// Finds lake locations and draws them as green spheres.
	// -------------------------------------------------------------------------
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("MapGen.ShowLakes"),
		TEXT("Finds and visualizes lake locations in PIE as green spheres. Args: Resolution Seed LakeCount (default: 65 42 5)."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			const int32 Res       = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 65;
			const int32 Seed      = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 42;
			const int32 LakeCount = Args.IsValidIndex(2) ? FCString::Atoi(*Args[2]) : 5;

			UWorld* World = GetPIEWorld();
			if (!World)
			{
				UE_LOG(LogMapGenCmd, Warning, TEXT("MapGen.ShowLakes -- PIE world required."));
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor::Red, TEXT("ShowLakes: Start PIE first."));
				return;
			}

			UMapGeneratorSettings* Settings = MakeCmdSettings(Res, Seed);
			Settings->MaxLakeCount = FMath::Max(1, LakeCount);

			UHeightmapGenerator* Heightmap = NewObject<UHeightmapGenerator>(GetTransientPackage(), NAME_None, RF_Transient);
			if (!Heightmap->Generate(Settings)) { UE_LOG(LogMapGenCmd, Warning, TEXT("ShowLakes -- Generate() failed.")); return; }

			UWaterSystemBuilder* Water = NewObject<UWaterSystemBuilder>(GetTransientPackage(), NAME_None, RF_Transient);
			TArray<FIntPoint> Lakes = Water->FindLakeLocations(Heightmap, Settings);

			const float CellSize = 100.0f;
			const float OriginX  = -(Res - 1) * CellSize * 0.5f;
			const float OriginY  = -(Res - 1) * CellSize * 0.5f;
			const float Duration = 30.0f;

			for (const FIntPoint& L : Lakes)
			{
				const float H = Heightmap->GetHeightAt(L.X, L.Y);
				const FVector Pos(OriginX + L.X * CellSize, OriginY + L.Y * CellSize, H * 5000.0f + 50.0f);
				DrawDebugSphere(World, Pos, CellSize * 0.8f, 8, FColor::Green, false, Duration);
			}

			UE_LOG(LogMapGenCmd, Display, TEXT("MapGen.ShowLakes -- %d/%d lakes found | Res=%d Seed=%d"),
				Lakes.Num(), LakeCount, Res, Seed);
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 12.0f, FColor::Green,
				FString::Printf(TEXT("Lakes: %d/%d found"), Lakes.Num(), LakeCount));
		}),
		ECVF_Default
	);

	// -------------------------------------------------------------------------
	// MapGen.BuildWaterSystem [Resolution=65] [Seed=42]
	// Full water pipeline: rivers (blue) + lakes (green).
	// -------------------------------------------------------------------------
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("MapGen.BuildWaterSystem"),
		TEXT("Full water generation pipeline visualized in PIE. Args: Resolution Seed (default: 65 42). Rivers=Blue Lakes=Green."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			const int32 Res  = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 65;
			const int32 Seed = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 42;

			UWorld* World = GetPIEWorld();
			if (!World)
			{
				UE_LOG(LogMapGenCmd, Warning, TEXT("MapGen.BuildWaterSystem -- PIE world required."));
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor::Red, TEXT("BuildWaterSystem: Start PIE first."));
				return;
			}

			UMapGeneratorSettings* Settings = MakeCmdSettings(Res, Seed);

			UHeightmapGenerator* Heightmap = NewObject<UHeightmapGenerator>(GetTransientPackage(), NAME_None, RF_Transient);
			if (!Heightmap->Generate(Settings)) { UE_LOG(LogMapGenCmd, Warning, TEXT("BuildWaterSystem -- Generate() failed.")); return; }

			UWaterSystemBuilder* Water = NewObject<UWaterSystemBuilder>(GetTransientPackage(), NAME_None, RF_Transient);
			const bool bOK = Water->BuildWaterBodies(Heightmap, Settings, 100.0f);

			if (!bOK) { UE_LOG(LogMapGenCmd, Warning, TEXT("BuildWaterSystem -- BuildWaterBodies() failed.")); return; }

			const float Duration = 30.0f;
			int32 Rivers = 0, Lakes = 0;

			for (const FWaterBodyDefinition& Def : Water->GetWaterBodyDefinitions())
			{
				FColor Color;
				switch (Def.WaterType)
				{
					case EWaterBodyType::River: Color = FColor::Blue;  ++Rivers; break;
					case EWaterBodyType::Lake:  Color = FColor::Green; ++Lakes;  break;
					default:                    Color = FColor::White; break;
				}

				for (int32 i = 0; i < Def.SplinePoints.Num() - 1; ++i)
					DrawDebugLine(World, Def.SplinePoints[i], Def.SplinePoints[i+1], Color, false, Duration, 0, 8.0f);

				if (Def.SplinePoints.Num() > 0)
					DrawDebugSphere(World, Def.SplinePoints[0], 60.0f, 6, Color, false, Duration);
			}

			UE_LOG(LogMapGenCmd, Display,
				TEXT("MapGen.BuildWaterSystem -- %d total defs | Rivers=%d Lakes=%d | Res=%d Seed=%d"),
				Water->GetWaterBodyDefinitions().Num(), Rivers, Lakes, Res, Seed);
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::White,
				FString::Printf(TEXT("WaterSystem | Rivers=%d Lakes=%d | Total=%d"),
					Rivers, Lakes, Water->GetWaterBodyDefinitions().Num()));
		}),
		ECVF_Default
	);

	// -------------------------------------------------------------------------
	// MapGen.ShowRiverSources [Resolution=65] [Seed=42] [RiverCount=5] [MinDist=8]
	// Generates heightmap and visualizes river source candidates with red spheres.
	// -------------------------------------------------------------------------
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("MapGen.ShowRiverSources"),
		TEXT("Finds and visualizes river source candidates in PIE. Args: Resolution Seed RiverCount MinDist (default: 65 42 5 8)."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			const int32 Res        = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 65;
			const int32 Seed       = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 42;
			const int32 RiverCount = Args.IsValidIndex(2) ? FCString::Atoi(*Args[2]) : 5;
			const float MinDist    = Args.IsValidIndex(3) ? FCString::Atof(*Args[3]) : 8.0f;

			UWorld* World = GetPIEWorld();
			if (!World)
			{
				UE_LOG(LogMapGenCmd, Warning, TEXT("MapGen.ShowRiverSources -- PIE world required."));
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor::Red,
					TEXT("ShowRiverSources: Start PIE first."));
				return;
			}

			// 1. Generate heightmap
			UMapGeneratorSettings* Settings = MakeCmdSettings(Res, Seed);
			Settings->MaxRiverCount = FMath::Max(1, RiverCount);

			UHeightmapGenerator* Heightmap = NewObject<UHeightmapGenerator>(
				GetTransientPackage(), NAME_None, RF_Transient);
			if (!Heightmap->Generate(Settings))
			{
				UE_LOG(LogMapGenCmd, Warning, TEXT("MapGen.ShowRiverSources -- Generate() failed."));
				return;
			}

			// 2. Find river sources
			UWaterSystemBuilder* Water = NewObject<UWaterSystemBuilder>(
				GetTransientPackage(), NAME_None, RF_Transient);
			Water->MinRiverSourceDistance = FMath::Max(1.0f, MinDist);

			TArray<FIntPoint> Sources = Water->FindRiverSources(Heightmap, Settings);

			// 3. Draw debug spheres at source points (red)
			const float CellSize = 100.0f;
			const float OriginX  = -(Res - 1) * CellSize * 0.5f;
			const float OriginY  = -(Res - 1) * CellSize * 0.5f;
			const float Duration = 30.0f;
			const float Radius   = CellSize * 0.6f;

			for (const FIntPoint& S : Sources)
			{
				const float H   = Heightmap->GetHeightAt(S.X, S.Y);
				const FVector Pos(
					OriginX + S.X * CellSize,
					OriginY + S.Y * CellSize,
					H * 5000.0f + 50.0f);   // lift proportional to height for visibility
				DrawDebugSphere(World, Pos, Radius, 8, FColor::Red, false, Duration);
			}

			UE_LOG(LogMapGenCmd, Display,
				TEXT("MapGen.ShowRiverSources -- Res=%d Seed=%d | %d/%d sources found | MinDist=%.1f"),
				Res, Seed, Sources.Num(), RiverCount, MinDist);

			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 12.0f, FColor::Red,
				FString::Printf(TEXT("RiverSources: %d/%d found | Res=%d Seed=%d MinDist=%.1f"),
					Sources.Num(), RiverCount, Res, Seed, MinDist));
		}),
		ECVF_Default
	);

	// -------------------------------------------------------------------------
	// MapGen.TestResourceFilters [Res=65] [Seed=42]
	// Tests CheckFilters() for all biome/climate combinations and logs results.
	// -------------------------------------------------------------------------
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("MapGen.TestResourceFilters"),
		TEXT("Tests CheckFilters() across all biome/climate combinations. Args: Res Seed (default: 65 42)"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			const int32 Res  = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 65;
			const int32 Seed = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 42;

			UMapGeneratorSettings* S = MakeCmdSettings(Res, Seed);
			S->SeaLevel = 0.3f;

			// Rule: Forest biome, Temperate climate, mid-height
			FResourceSpawnRule TestRule;
			TestRule.AllowedBiomes.Add(EBiomeType::Forest);
			TestRule.AllowedClimateZones.Add(EClimateZone::Temperate);
			TestRule.MinHeight = 0.3f;
			TestRule.MaxHeight = 0.8f;

			UResourceDistributor* Dist = NewObject<UResourceDistributor>(
				GetTransientPackage(), NAME_None, RF_Transient);

			struct FTestCase { EBiomeType B; EClimateZone C; float H; };
			const TArray<FTestCase> Cases = {
				{ EBiomeType::Forest,   EClimateZone::Temperate, 0.5f },
				{ EBiomeType::Mountain, EClimateZone::Temperate, 0.5f },
				{ EBiomeType::Forest,   EClimateZone::Tropical,  0.5f },
				{ EBiomeType::Forest,   EClimateZone::Temperate, 0.1f },
				{ EBiomeType::Forest,   EClimateZone::Temperate, 0.9f },
			};

			UE_LOG(LogMapGenCmd, Display, TEXT("MapGen.TestResourceFilters -- Res=%d Seed=%d"), Res, Seed);
			for (const FTestCase& TC : Cases)
			{
				const bool bPass = Dist->CheckFilters(TestRule, TC.B, TC.C, TC.H);
				UE_LOG(LogMapGenCmd, Display,
					TEXT("  Biome=%d Climate=%d Height=%.1f => %s"),
					(int32)TC.B, (int32)TC.C, TC.H, bPass ? TEXT("PASS") : TEXT("FAIL"));
			}

			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Yellow,
				TEXT("MapGen.TestResourceFilters -- see Output Log for results"));
		}),
		ECVF_Default
	);

	// -------------------------------------------------------------------------
	// MapGen.ShowPlacementAreas [Res=65] [Seed=42] [BiomeIndex=3 (Forest)]
	// Visualizes valid placement cells for a Forest/Temperate rule as green spheres.
	// -------------------------------------------------------------------------
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("MapGen.ShowPlacementAreas"),
		TEXT("Shows valid placement cells for a Forest rule. Args: Res Seed BiomeIndex (default: 65 42 3)"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			UWorld* World = GetPIEWorld();
			if (!World) { UE_LOG(LogMapGenCmd, Warning, TEXT("MapGen.ShowPlacementAreas -- no PIE world")); return; }

			const int32 Res        = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 65;
			const int32 Seed       = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 42;
			const int32 BiomeIndex = Args.IsValidIndex(2) ? FCString::Atoi(*Args[2]) : 3; // Forest=3
			const float CellSize   = 100.0f;
			const float Duration   = 20.0f;

			UMapGeneratorSettings* S = MakeCmdSettings(Res, Seed);
			S->SeaLevel = 0.3f;
			S->bGenerateRivers = true; S->MaxRiverCount = 3;
			S->bGenerateLakes  = true; S->MaxLakeCount  = 5;

			UHeightmapGenerator* Gen = NewObject<UHeightmapGenerator>(GetTransientPackage(), NAME_None, RF_Transient);
			Gen->Generate(S);

			UBiomeManager* Biomes = NewObject<UBiomeManager>(GetTransientPackage(), NAME_None, RF_Transient);
			Biomes->AssignBiomes(S, Gen);

			UWaterSystemBuilder* Water = NewObject<UWaterSystemBuilder>(GetTransientPackage(), NAME_None, RF_Transient);
			Water->BuildWaterBodies(Gen, S, CellSize);

			FResourceSpawnRule Rule;
			Rule.AllowedBiomes.Add(static_cast<EBiomeType>(BiomeIndex));
			Rule.MinHeight = 0.2f;
			Rule.MaxHeight = 0.85f;

			UResourceDistributor* Dist = NewObject<UResourceDistributor>(GetTransientPackage(), NAME_None, RF_Transient);
			TArray<FIntPoint> Cells = Dist->CalculatePlacementAreas(Rule, Gen, Biomes, nullptr, Water, CellSize);

			const float HalfW = (Res - 1) * CellSize * 0.5f;
			const float HalfH = (Res - 1) * CellSize * 0.5f;

			for (const FIntPoint& C : Cells)
			{
				const float H = Gen->GetHeightAt(C.X, C.Y);
				const FVector Pos(
					-HalfW + C.X * CellSize,
					-HalfH + C.Y * CellSize,
					H * 5000.0f + 50.0f);
				DrawDebugSphere(World, Pos, 30.0f, 6, FColor::Green, false, Duration);
			}

			UE_LOG(LogMapGenCmd, Display,
				TEXT("MapGen.ShowPlacementAreas -- Res=%d Seed=%d | %d valid cells for biome %d"),
				Res, Seed, Cells.Num(), BiomeIndex);

			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 12.0f, FColor::Green,
				FString::Printf(TEXT("PlacementAreas: %d cells | Res=%d Seed=%d Biome=%d"),
					Cells.Num(), Res, Seed, BiomeIndex));
		}),
		ECVF_Default
	);

	// Needed for header includes: FBiomeCell
	UE_LOG(LogMapGenCmd, Log, TEXT("MapGen console commands registered."));
}

