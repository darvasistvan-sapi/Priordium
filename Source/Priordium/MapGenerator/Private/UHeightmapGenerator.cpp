// Copyright Priordium. All Rights Reserved.
//
// UHeightmapGenerator.cpp
// UHeightmapGenerator ActorComponent implementation.

#include "UHeightmapGenerator.h"
#include "FHeightmapConfig.h"
#include "UMapGeneratorSettings.h"
#include "FNoiseWrapper.h"
#include "Math/RandomStream.h"
#include "LandscapeProxy.h"

DEFINE_LOG_CATEGORY_STATIC(LogHeightmapGenerator, Log, All);

UHeightmapGenerator::UHeightmapGenerator()
{
	PrimaryComponentTick.bCanEverTick = false;
	Resolution = FIntPoint(0, 0);
}

void UHeightmapGenerator::BeginPlay()
{
	Super::BeginPlay();
}


void UHeightmapGenerator::InitializeHeightmap(int32 SizeX, int32 SizeY)
{
	if (SizeX <= 0 || SizeY <= 0)
	{
		UE_LOG(LogHeightmapGenerator, Log,
			TEXT("InitializeHeightmap -- invalid size: %d x %d"), SizeX, SizeY);
		return;
	}

	Resolution = FIntPoint(SizeX, SizeY);
	HeightmapData.SetNumUninitialized(SizeX * SizeY);

	for (int32 i = 0; i < HeightmapData.Num(); ++i)
	{
		HeightmapData[i] = 0.5f;
	}
}

float UHeightmapGenerator::GetHeightAt(int32 X, int32 Y) const
{
	if (X < 0 || X >= Resolution.X || Y < 0 || Y >= Resolution.Y)
	{
		return 0.0f;
	}
	return HeightmapData[Y * Resolution.X + X];
}


bool UHeightmapGenerator::Generate(const UMapGeneratorSettings* Settings)
{
	if (!Settings)
	{
		UE_LOG(LogHeightmapGenerator, Warning, TEXT("Generate -- Settings null, generation aborted."));
		return false;
	}

	const int32 Res = Settings->Resolution;
	if (Res <= 0)
	{
		UE_LOG(LogHeightmapGenerator, Warning, TEXT("Generate -- invalid Resolution: %d"), Res);
		return false;
	}

	InitializeHeightmap(Res, Res);

	const FHeightmapConfig& Config = Settings->HeightmapConfig;
	const int32 Seed = Settings->Seed;

	for (int32 Y = 0; Y < Resolution.Y; ++Y)
	{
		for (int32 X = 0; X < Resolution.X; ++X)
		{
			HeightmapData[Y * Resolution.X + X] = GenerateNoiseValue(X, Y, Config, Seed);
		}
	}

	NormalizeHeightmap();

	if (Settings->bUseContinentMask)
	{
		ApplyContinentMask(Settings->EdgeFalloffDistance);
	}

	if (Config.bEnableErosion)
	{
		ApplyHydraulicErosion(Config, Seed);
	}

	UE_LOG(LogHeightmapGenerator, Display,
		TEXT("Generate -- done. Resolution: %dx%d | Seed: %d | Octaves: %d | ContinentMask: %s | Erosion: %s"),
		Res, Res, Seed, Config.Octaves,
		Settings->bUseContinentMask ? TEXT("ON") : TEXT("OFF"),
		Config.bEnableErosion ? TEXT("ON") : TEXT("OFF"));

	return true;
}

float UHeightmapGenerator::GenerateNoiseValue(int32 X, int32 Y, const FHeightmapConfig& Config, int32 Seed) const
{
	float Value        = 0.0f;
	float Amplitude    = Config.Amplitude;
	float Frequency    = Config.Frequency;
	float MaxAmplitude = 0.0f;

	for (int32 Octave = 0; Octave < Config.Octaves; ++Octave)
	{
		const int32 OctaveSeed = Seed + Octave * 1000;
		Value        += FNoiseWrapper::SimplexNoise2D(static_cast<float>(X) * Frequency,
		                                               static_cast<float>(Y) * Frequency, OctaveSeed) * Amplitude;
		MaxAmplitude += Amplitude;
		Amplitude    *= Config.Persistence;
		Frequency    *= Config.Lacunarity;
	}

	if (MaxAmplitude > 0.0f) Value /= MaxAmplitude;
	return Value;
}

void UHeightmapGenerator::NormalizeHeightmap()
{
	if (HeightmapData.IsEmpty()) return;

	float MinVal = HeightmapData[0];
	float MaxVal = HeightmapData[0];
	for (float H : HeightmapData)
	{
		if (H < MinVal) MinVal = H;
		if (H > MaxVal) MaxVal = H;
	}

	const float Range = MaxVal - MinVal;
	if (Range < KINDA_SMALL_NUMBER)
	{
		for (float& H : HeightmapData) { H = 0.5f; }
		return;
	}
	for (float& H : HeightmapData) { H = (H - MinVal) / Range; }
}


float UHeightmapGenerator::Smoothstep(float T)
{
	T = FMath::Clamp(T, 0.0f, 1.0f);
	return T * T * (3.0f - 2.0f * T);
}

void UHeightmapGenerator::ApplyContinentMask(float EdgeFalloffDistance)
{
	if (HeightmapData.IsEmpty()) return;

	const float W = static_cast<float>(Resolution.X);
	const float H = static_cast<float>(Resolution.Y);

	for (int32 Y = 0; Y < Resolution.Y; ++Y)
	{
		for (int32 X = 0; X < Resolution.X; ++X)
		{
			const float NX = static_cast<float>(X) / (W - 1.0f);
			const float NY = static_cast<float>(Y) / (H - 1.0f);

			const float DistFromEdge = FMath::Min(
				FMath::Min(NX, 1.0f - NX),
				FMath::Min(NY, 1.0f - NY));

			const float MaskVal = Smoothstep(DistFromEdge / FMath::Max(EdgeFalloffDistance, KINDA_SMALL_NUMBER));
			HeightmapData[Y * Resolution.X + X] *= MaskVal;
		}
	}
}


float UHeightmapGenerator::GetHeightBilinear(float X, float Y) const
{
	const int32 X0 = FMath::Clamp(static_cast<int32>(X),     0, Resolution.X - 1);
	const int32 X1 = FMath::Clamp(static_cast<int32>(X) + 1, 0, Resolution.X - 1);
	const int32 Y0 = FMath::Clamp(static_cast<int32>(Y),     0, Resolution.Y - 1);
	const int32 Y1 = FMath::Clamp(static_cast<int32>(Y) + 1, 0, Resolution.Y - 1);

	const float FracX = X - static_cast<int32>(X);
	const float FracY = Y - static_cast<int32>(Y);

	const float H00 = HeightmapData[Y0 * Resolution.X + X0];
	const float H10 = HeightmapData[Y0 * Resolution.X + X1];
	const float H01 = HeightmapData[Y1 * Resolution.X + X0];
	const float H11 = HeightmapData[Y1 * Resolution.X + X1];

	return FMath::Lerp(FMath::Lerp(H00, H10, FracX), FMath::Lerp(H01, H11, FracX), FracY);
}

void UHeightmapGenerator::GetGradient(float X, float Y, float& OutGradX, float& OutGradY) const
{
	const int32 IX = static_cast<int32>(X);
	const int32 IY = static_cast<int32>(Y);
	const float FracX = X - IX;
	const float FracY = Y - IY;

	const int32 X0 = FMath::Clamp(IX,     0, Resolution.X - 1);
	const int32 X1 = FMath::Clamp(IX + 1, 0, Resolution.X - 1);
	const int32 Y0 = FMath::Clamp(IY,     0, Resolution.Y - 1);
	const int32 Y1 = FMath::Clamp(IY + 1, 0, Resolution.Y - 1);

	const float H00 = HeightmapData[Y0 * Resolution.X + X0];
	const float H10 = HeightmapData[Y0 * Resolution.X + X1];
	const float H01 = HeightmapData[Y1 * Resolution.X + X0];
	const float H11 = HeightmapData[Y1 * Resolution.X + X1];

	// Bilinear gradient
	OutGradX = FMath::Lerp(H10 - H00, H11 - H01, FracY);
	OutGradY = FMath::Lerp(H01 - H00, H11 - H10, FracX);
}

void UHeightmapGenerator::AddHeightBilinear(float X, float Y, float Delta)
{
	const int32 X0 = FMath::Clamp(static_cast<int32>(X),     0, Resolution.X - 1);
	const int32 X1 = FMath::Clamp(static_cast<int32>(X) + 1, 0, Resolution.X - 1);
	const int32 Y0 = FMath::Clamp(static_cast<int32>(Y),     0, Resolution.Y - 1);
	const int32 Y1 = FMath::Clamp(static_cast<int32>(Y) + 1, 0, Resolution.Y - 1);

	const float FracX = X - static_cast<int32>(X);
	const float FracY = Y - static_cast<int32>(Y);

	// Bilinear weights: distribute Delta across the 4 neighbors
	HeightmapData[Y0 * Resolution.X + X0] += Delta * (1.0f - FracX) * (1.0f - FracY);
	HeightmapData[Y0 * Resolution.X + X1] += Delta * FracX           * (1.0f - FracY);
	HeightmapData[Y1 * Resolution.X + X0] += Delta * (1.0f - FracX) * FracY;
	HeightmapData[Y1 * Resolution.X + X1] += Delta * FracX           * FracY;
}

void UHeightmapGenerator::ApplyHydraulicErosion(const FHeightmapConfig& Config, int32 Seed)
{
	if (HeightmapData.IsEmpty() || Resolution.X < 2 || Resolution.Y < 2) return;

	// Deterministic random for raindrop spawn positions
	FRandomStream Rand(Seed + 77777);

	const float ErosionRate    = FMath::Clamp(Config.ErosionRate,    0.001f, 1.0f);
	const float DepositionRate = FMath::Clamp(Config.DepositionRate, 0.001f, 1.0f);
	const float EvaporationRate = FMath::Clamp(Config.EvaporationRate, 0.001f, 0.1f);
	const int32 MaxStepsPerDrop = 64; // Maximum number of steps per drop (early exit condition)

	for (int32 Iter = 0; Iter < Config.ErosionIterations; ++Iter)
	{
		// Spawn drop at random position (1 pixel inward from the edge)
		float PosX = Rand.FRandRange(1.0f, static_cast<float>(Resolution.X - 2));
		float PosY = Rand.FRandRange(1.0f, static_cast<float>(Resolution.Y - 2));

		float DirX    = 0.0f;
		float DirY    = 0.0f;
		float Water   = 1.0f;
		float Sediment = 0.0f;
		float Speed   = 0.0f;

		for (int32 Step = 0; Step < MaxStepsPerDrop; ++Step)
		{
			// Gradient at the drop's position
			float GradX, GradY;
			GetGradient(PosX, PosY, GradX, GradY);

			// Drop movement: along the slope, with inertia
			const float Inertia = 0.3f;
			DirX = DirX * Inertia - GradX * (1.0f - Inertia);
			DirY = DirY * Inertia - GradY * (1.0f - Inertia);

			// Normalization (if non-zero)
			const float DirLen = FMath::Sqrt(DirX * DirX + DirY * DirY);
			if (DirLen < KINDA_SMALL_NUMBER) break;
			DirX /= DirLen;
			DirY /= DirLen;

			// New position
			const float NewX = PosX + DirX;
			const float NewY = PosY + DirY;

			// Bounds check
			if (NewX < 0.0f || NewX >= Resolution.X - 1 || NewY < 0.0f || NewY >= Resolution.Y - 1) break;

			// Height difference (negative = moving downward)
			const float OldHeight = GetHeightBilinear(PosX, PosY);
			const float NewHeight = GetHeightBilinear(NewX, NewY);
			const float HeightDiff = NewHeight - OldHeight;

			// Speed update (gravity)
			Speed = FMath::Sqrt(FMath::Max(0.0f, Speed * Speed - HeightDiff));

			// Capacity: how much the drop can carry
			const float Capacity = FMath::Max(-HeightDiff, 0.01f) * Speed * Water;

			if (Sediment > Capacity)
			{
				// Deposition: the drop is saturated, deposits sediment
				const float Deposit = (Sediment - Capacity) * DepositionRate;
				Sediment -= Deposit;
				AddHeightBilinear(PosX, PosY, Deposit);
			}
			else
			{
				// Erosion: the drop picks up soil
				const float Erode = FMath::Min((Capacity - Sediment) * ErosionRate, -HeightDiff);
				if (Erode > 0.0f)
				{
					Sediment += Erode;
					AddHeightBilinear(PosX, PosY, -Erode);
				}
			}

			// Move the drop
			PosX = NewX;
			PosY = NewY;

			// Evaporation
			Water *= (1.0f - EvaporationRate);
			if (Water < 0.01f)
			{
				// Deposit remaining sediment
				AddHeightBilinear(PosX, PosY, Sediment);
				break;
			}
		}
	}

	// Normalize values to [0, 1] after erosion (erosion can push values out of range)
	NormalizeHeightmap();
}

// static
bool UHeightmapGenerator::GetTerrainHeight(
	UWorld*                      World,
	float                        X,
	float                        Y,
	float&                       OutZ,
	const UMapGeneratorSettings* Settings) const
{
	// --- Primary: multi-hit object trace, prefer the ALandscapeProxy surface ---
	// Using LineTraceMulti so we can sift through all hits and pick the actual landscape,
	// rather than stopping at the first thing above it (water plane, resource mesh, etc.).
	{
		TArray<FHitResult> HitResults;
		FCollisionQueryParams QueryParams;
		QueryParams.bTraceComplex = false;

		FCollisionObjectQueryParams ObjectQuery(FCollisionObjectQueryParams::AllStaticObjects);

		if (World->LineTraceMultiByObjectType(
			HitResults,
			FVector(X, Y, 500000.f),
			FVector(X, Y, -500000.f),
			ObjectQuery,
			QueryParams))
		{
			// First pass: look for the actual landscape surface
			for (const FHitResult& Hit : HitResults)
			{
				if (Hit.GetActor() && Hit.GetActor()->IsA<ALandscapeProxy>())
				{
					OutZ = Hit.Location.Z;
					return true;
				}
			}

			// Second pass: no landscape actor found, pick the deepest (lowest Z) hit.
			// Terrain is always below water bodies / foliage / placed actors.
			float LowestZ = TNumericLimits<float>::Max();
			for (const FHitResult& Hit : HitResults)
			{
				if (Hit.Location.Z < LowestZ)
				{
					LowestZ = Hit.Location.Z;
				}
			}
			OutZ = LowestZ;
			return true;
		}
	}

	UE_LOG(LogHeightmapGenerator, Verbose,
		TEXT("GetTerrainHeight: trace missed at (%.0f, %.0f) -- trying heightmap fallback."), X, Y);

	// --- Fallback: derive Z from the heightmap using the landscape height formula ---
	// ULandscapeBuilder remaps float[0,1] -> uint16 V/2+16384, then sets actor Z scale = 50.
	// UE landscape formula:  WorldZ = (uint16 - 32768) * (1/128) * ZScale
	// Combined:              WorldZ = (Height * 32767.5 - 16384) * (50 / 128)
	if (this->IsInitialized() && Settings)
	{
		const FIntPoint Res      = this->GetResolution();
		const float     CellSize = static_cast<float>(Settings->QuadSize);
		const FVector2D MapOrigin(
			-(Res.X - 1) * CellSize * 0.5f,
			-(Res.Y - 1) * CellSize * 0.5f);

		const int32 GridX = FMath::RoundToInt((X - MapOrigin.X) / CellSize);
		const int32 GridY = FMath::RoundToInt((Y - MapOrigin.Y) / CellSize);

		if (GridX >= 0 && GridX < Res.X && GridY >= 0 && GridY < Res.Y)
		{
			const float HeightNorm = this->GetHeightAt(GridX, GridY); // [0, 1]
			OutZ = (HeightNorm * 32767.5f - 16384.0f) * (50.0f / 128.0f);
			UE_LOG(LogHeightmapGenerator, Verbose,
				TEXT("GetTerrainHeight: heightmap fallback at (%d, %d) => Z=%.1f"), GridX, GridY, OutZ);
			return true;
		}
	}

	return false;
}
