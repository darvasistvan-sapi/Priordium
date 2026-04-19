// Copyright Priordium. All Rights Reserved.
//
// UBiomeManager.cpp
// UBiomeManager ActorComponent implementation.

#include "UBiomeManager.h"
#include "UHeightmapGenerator.h"
#include "UMapGeneratorSettings.h"
#include "FNoiseWrapper.h"

DEFINE_LOG_CATEGORY_STATIC(LogBiomeManager, Log, All);

UBiomeManager::UBiomeManager()
{
	PrimaryComponentTick.bCanEverTick = false;
	Resolution = FIntPoint(0, 0);
}

void UBiomeManager::BeginPlay()
{
	Super::BeginPlay();
}


bool UBiomeManager::IsValidCoord(int32 X, int32 Y) const
{
	return X >= 0 && X < Resolution.X && Y >= 0 && Y < Resolution.Y;
}

void UBiomeManager::InitializeBiomeMap(int32 SizeX, int32 SizeY)
{
	if (SizeX <= 0 || SizeY <= 0)
	{
		UE_LOG(LogBiomeManager, Log,
			TEXT("InitializeBiomeMap -- invalid size: %d x %d"), SizeX, SizeY);
		return;
	}

	Resolution = FIntPoint(SizeX, SizeY);
	BiomeMap.SetNum(SizeX * SizeY);

	for (FBiomeCell& Cell : BiomeMap)
	{
		Cell = FBiomeCell();
	}
}

FBiomeCell UBiomeManager::GetBiomeAt(int32 X, int32 Y) const
{
	if (!IsValidCoord(X, Y)) return FBiomeCell();
	return BiomeMap[Y * Resolution.X + X];
}

EBiomeType UBiomeManager::GetBiomeTypeAt(int32 X, int32 Y) const
{
	if (!IsValidCoord(X, Y)) return EBiomeType::Plains;
	return BiomeMap[Y * Resolution.X + X].BiomeType;
}


bool UBiomeManager::AssignBiomes(const UMapGeneratorSettings* Settings, UHeightmapGenerator* Heightmap)
{
	if (!Settings)
	{
		UE_LOG(LogBiomeManager, Warning, TEXT("AssignBiomes -- Settings null."));
		return false;
	}
	if (!Heightmap || !Heightmap->IsInitialized())
	{
		UE_LOG(LogBiomeManager, Warning, TEXT("AssignBiomes -- Heightmap null or not initialized."));
		return false;
	}

	const int32 Res = Settings->Resolution;
	if (Res != Heightmap->GetResolution().X || Res != Heightmap->GetResolution().Y)
	{
		UE_LOG(LogBiomeManager, Warning,
			TEXT("AssignBiomes -- Resolution mismatch: Settings=%d, Heightmap=%dx%d"),
			Res, Heightmap->GetResolution().X, Heightmap->GetResolution().Y);
		return false;
	}

	// BiomeMap, TemperatureMap, MoistureMap initialization
	InitializeBiomeMap(Res, Res);
	TemperatureMap.SetNumZeroed(Res * Res);
	MoistureMap.SetNumZeroed(Res * Res);

	GenerateTemperatureMap(Heightmap, Settings->Seed);

	GenerateMoistureMap(Heightmap, Settings->Seed);

	// Update Height, Temperature, Moisture values of cells
	for (int32 Y = 0; Y < Resolution.Y; ++Y)
	{
		for (int32 X = 0; X < Resolution.X; ++X)
		{
			const int32 Index           = Y * Resolution.X + X;
			BiomeMap[Index].Height      = Heightmap->GetHeightAt(X, Y);
			BiomeMap[Index].Temperature = TemperatureMap[Index];
			BiomeMap[Index].Moisture    = MoistureMap[Index];
		}
	}

	CalculateBiomes(Settings);

	CalculateBlendWeights();

	UE_LOG(LogBiomeManager, Display,
		TEXT("AssignBiomes -- done. Resolution: %dx%d | Seed: %d"), Res, Res, Settings->Seed);

	return true;
}

void UBiomeManager::GenerateTemperatureMap(UHeightmapGenerator* Heightmap, int32 Seed)
{
	const float ResY = static_cast<float>(Resolution.Y);

	// Noise seed for temperature variation (different from heightmap seed)
	const int32 TempSeed = Seed + 31337;

	float MinTemp =  FLT_MAX;
	float MaxTemp = -FLT_MAX;

	// Step 1: calculate raw values
	for (int32 Y = 0; Y < Resolution.Y; ++Y)
	{
		for (int32 X = 0; X < Resolution.X; ++X)
		{
			const int32 Index = Y * Resolution.X + X;

			// Latitude effect: Y=0 (north) -> cold (0), Y=max (south) -> warm (1)
			const float BaseTemp = 1.0f - (static_cast<float>(Y) / FMath::Max(ResY - 1.0f, 1.0f));

			// Altitude correction: high terrain means colder
			const float Height        = Heightmap->GetHeightAt(X, Y);
			const float HeightPenalty = Height * 0.5f;

			// Noise variation: low amplitude noise for more natural distribution
			const float NoiseScale = 0.03f;
			const float NoiseAmp   = 0.1f;
			const float Noise = FNoiseWrapper::SimplexNoise2D(
				static_cast<float>(X) * NoiseScale,
				static_cast<float>(Y) * NoiseScale,
				TempSeed) * NoiseAmp;

			const float RawTemp = BaseTemp - HeightPenalty + Noise;
			TemperatureMap[Index] = RawTemp;

			if (RawTemp < MinTemp) MinTemp = RawTemp;
			if (RawTemp > MaxTemp) MaxTemp = RawTemp;
		}
	}

	// Step 2: normalize to [0, 1] range
	const float Range = MaxTemp - MinTemp;
	if (Range < KINDA_SMALL_NUMBER)
	{
		for (float& T : TemperatureMap) { T = 0.5f; }
		return;
	}

	for (float& T : TemperatureMap)
	{
		T = (T - MinTemp) / Range;
	}
}

float UBiomeManager::GetTemperatureAt(int32 X, int32 Y) const
{
	if (!IsValidCoord(X, Y)) return 0.0f;
	return TemperatureMap[Y * Resolution.X + X];
}


void UBiomeManager::GenerateMoistureMap(UHeightmapGenerator* Heightmap, int32 Seed)
{
	const int32 MoistSeed = Seed + 99991;
	const float ResX = static_cast<float>(Resolution.X);
	const float ResY = static_cast<float>(Resolution.Y);

	float MinVal =  FLT_MAX;
	float MaxVal = -FLT_MAX;

	// Step 1: raw moisture values
	for (int32 Y = 0; Y < Resolution.Y; ++Y)
	{
		for (int32 X = 0; X < Resolution.X; ++X)
		{
			const int32 Index = Y * Resolution.X + X;

			// Base moisture: Simplex noise
			const float NoiseScale = 0.01f;
			const float BaseMoisture = FNoiseWrapper::SimplexNoise2D(
				static_cast<float>(X) * NoiseScale,
				static_cast<float>(Y) * NoiseScale,
				MoistSeed);

			// Ocean proximity: distance from map edge (normalized [0, 0.5])
			const float NX = static_cast<float>(X) / FMath::Max(ResX - 1.0f, 1.0f);
			const float NY = static_cast<float>(Y) / FMath::Max(ResY - 1.0f, 1.0f);
			const float DistFromEdge = FMath::Min(
				FMath::Min(NX, 1.0f - NX),
				FMath::Min(NY, 1.0f - NY));
			// Coastal effect: the closer to the edge, the more moisture
			const float OceanEffect = FMath::Max(0.0f, 0.3f - DistFromEdge * 2.0f);

			const float Raw = BaseMoisture + OceanEffect;
			MoistureMap[Index] = Raw;

			if (Raw < MinVal) MinVal = Raw;
			if (Raw > MaxVal) MaxVal = Raw;
		}
	}

	// Step 2: normalize to [0, 1]
	const float Range = MaxVal - MinVal;
	if (Range < KINDA_SMALL_NUMBER)
	{
		for (float& M : MoistureMap) { M = 0.5f; }
		return;
	}
	for (float& M : MoistureMap) { M = (M - MinVal) / Range; }
}

float UBiomeManager::GetMoistureAt(int32 X, int32 Y) const
{
	if (!IsValidCoord(X, Y)) return 0.0f;
	return MoistureMap[Y * Resolution.X + X];
}


EBiomeType UBiomeManager::DetermineBiomeType(float Height, float Temperature, float Moisture, float SeaLevel) const
{
	if (Height < SeaLevel)             return EBiomeType::Ocean;
	if (Height < SeaLevel + 0.05f)     return EBiomeType::Beach;
	if (Height > 0.75f)                return EBiomeType::Mountain;
	if (Height > 0.55f)                return EBiomeType::Hills;
	if (Height < 0.35f && Moisture > 0.65f) return EBiomeType::Swamp;
	if (Moisture > 0.55f)              return EBiomeType::Forest;
	return EBiomeType::Plains;
}

void UBiomeManager::CalculateBiomes(const UMapGeneratorSettings* Settings)
{
	const float SeaLevel = Settings->SeaLevel;

	for (int32 Y = 0; Y < Resolution.Y; ++Y)
	{
		for (int32 X = 0; X < Resolution.X; ++X)
		{
			const int32 Index = Y * Resolution.X + X;
			FBiomeCell& Cell  = BiomeMap[Index];
			Cell.BiomeType    = DetermineBiomeType(
				Cell.Height, Cell.Temperature, Cell.Moisture, SeaLevel);
		}
	}
}


void UBiomeManager::CalculateBlendWeights()
{
	static const int32 BlendRadius  = 3;
	static const float BlendRadiusF = static_cast<float>(BlendRadius);

	for (int32 Y = 0; Y < Resolution.Y; ++Y)
	{
		for (int32 X = 0; X < Resolution.X; ++X)
		{
			float Weights[FBiomeCell::BiomeTypeCount] = {};

			for (int32 DY = -BlendRadius; DY <= BlendRadius; ++DY)
			{
				for (int32 DX = -BlendRadius; DX <= BlendRadius; ++DX)
				{
					const float Distance = FMath::Sqrt(static_cast<float>(DX * DX + DY * DY));
					if (Distance > BlendRadiusF) continue;

					const int32 NX = X + DX;
					const int32 NY = Y + DY;
					if (!IsValidCoord(NX, NY)) continue;

					// Weight: 1.0 - (distance / radius), center cell has weight 1.0
					const float Weight = 1.0f - (Distance / BlendRadiusF);
					if (Weight <= 0.0f) continue;  // Exact radius boundary: skip zero-weight neighbor
					const EBiomeType NeighborBiome = BiomeMap[NY * Resolution.X + NX].BiomeType;
					Weights[static_cast<int32>(NeighborBiome)] += Weight;
				}
			}

			// Normalization: weights should sum to ~1.0
			float TotalWeight = 0.0f;
			for (float W : Weights) { TotalWeight += W; }

			if (TotalWeight > KINDA_SMALL_NUMBER)
			{
				for (float& W : Weights) { W /= TotalWeight; }
			}

			FMemory::Memcpy(BiomeMap[Y * Resolution.X + X].BiomeBlendWeights, Weights, sizeof(Weights));
		}
	}
}

TMap<EBiomeType, float> UBiomeManager::GetBlendWeightsAt(int32 X, int32 Y) const
{
	if (!IsValidCoord(X, Y)) return TMap<EBiomeType, float>();

	TMap<EBiomeType, float> Result;
	const float* Weights = BiomeMap[Y * Resolution.X + X].BiomeBlendWeights;
	for (int32 i = 0; i < FBiomeCell::BiomeTypeCount; ++i)
	{
		if (Weights[i] > 0.0f)
			Result.Add(static_cast<EBiomeType>(i), Weights[i]);
	}
	return Result;
}

