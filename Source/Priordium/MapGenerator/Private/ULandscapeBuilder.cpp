// Copyright Priordium. All Rights Reserved.
//
// ULandscapeBuilder.cpp
// ULandscapeBuilder ActorComponent implementation.

#include "ULandscapeBuilder.h"
#include "LandscapeProxy.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeDataAccess.h"
#include "UHeightmapGenerator.h"
#include "UMapGeneratorSettings.h"
#include "UBiomeManager.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "UObject/UnrealType.h"

#if WITH_EDITOR
#include "LandscapeEditorUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogLandscapeBuilder, Log, All);

namespace
{
#if WITH_EDITOR
	void EnsureWaterEditLayer(ALandscape* Landscape)
	{
		if (!Landscape)
		{
			return;
		}

		const FName WaterLayerName(TEXT("Water"));

		// Prevent duplicate layer creation.
		if (Landscape->GetLayerIndex(WaterLayerName) != INDEX_NONE)
		{
			return;
		}

		Landscape->CreateLayer(WaterLayerName);
		UE_LOG(LogLandscapeBuilder, Display, TEXT("BuildLandscape -- pre-created edit layer: Water"));
	}
#endif
}

ULandscapeBuilder::ULandscapeBuilder()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void ULandscapeBuilder::BeginPlay()
{
	Super::BeginPlay();
}



bool ULandscapeBuilder::LoadLayerInfoAssets()
{
	// Biome -> asset name mapping
	const TArray<TPair<EBiomeType, FString>> BiomeAssetNames =
	{
		{ EBiomeType::Ocean,    TEXT("/Game/MapGenerator/Landscape/Layers/LI_Ocean")    },
		{ EBiomeType::Beach,    TEXT("/Game/MapGenerator/Landscape/Layers/LI_Beach")    },
		{ EBiomeType::Plains,   TEXT("/Game/MapGenerator/Landscape/Layers/LI_Plains")   },
		{ EBiomeType::Forest,   TEXT("/Game/MapGenerator/Landscape/Layers/LI_Forest")   },
		{ EBiomeType::Hills,    TEXT("/Game/MapGenerator/Landscape/Layers/LI_Hills")    },
		{ EBiomeType::Mountain, TEXT("/Game/MapGenerator/Landscape/Layers/LI_Mountain") },
		{ EBiomeType::Swamp,    TEXT("/Game/MapGenerator/Landscape/Layers/LI_Swamp")    },
	};

	int32 LoadedCount = 0;

	for (const auto& Pair : BiomeAssetNames)
	{
		ULandscapeLayerInfoObject* LayerInfo = Cast<ULandscapeLayerInfoObject>(
			StaticLoadObject(ULandscapeLayerInfoObject::StaticClass(), nullptr, *Pair.Value));

		if (LayerInfo)
		{
			LayerInfoAssets.Add(Pair.Key, LayerInfo);
			++LoadedCount;
			UE_LOG(LogLandscapeBuilder, Log,
				TEXT("LoadLayerInfoAssets  --  loaded: %s"), *Pair.Value);
		}
		else
		{
			UE_LOG(LogLandscapeBuilder, Log,
				TEXT("LoadLayerInfoAssets  --  not found: %s"), *Pair.Value);
		}
	}

	UE_LOG(LogLandscapeBuilder, Display,
		TEXT("LoadLayerInfoAssets  --  %d / %d layer info loaded."),
		LoadedCount, BiomeAssetNames.Num());

	return LoadedCount == BiomeAssetNames.Num();
}

bool ULandscapeBuilder::AreAllLayersLoaded() const
{
	const TArray<EBiomeType> RequiredBiomes =
	{
		EBiomeType::Ocean, EBiomeType::Beach, EBiomeType::Plains,
		EBiomeType::Forest, EBiomeType::Hills, EBiomeType::Mountain,
		EBiomeType::Swamp
	};

	for (EBiomeType Biome : RequiredBiomes)
	{
		const TObjectPtr<ULandscapeLayerInfoObject>* Found = LayerInfoAssets.Find(Biome);
		if (!Found || !(*Found))
			return false;
	}
	return true;
}


bool ULandscapeBuilder::BuildLandscape(const UMapGeneratorSettings* Settings, UHeightmapGenerator* Heightmap, UBiomeManager* Biome)
{
	if (!Settings)
	{
		UE_LOG(LogLandscapeBuilder, Warning, TEXT("BuildLandscape  --  Settings null."));
		return false;
	}
	if (!Heightmap || !Heightmap->IsInitialized())
	{
		UE_LOG(LogLandscapeBuilder, Warning, TEXT("BuildLandscape  --  Heightmap null or not initialized."));
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogLandscapeBuilder, Warning, TEXT("BuildLandscape  --  World null."));
		return false;
	}
	const int32 Res = Settings->Resolution;

	// 1. Heightmap conversion
	TArray<uint16> HeightData = ConvertHeightmapToUint16(Heightmap->GetHeightmapData());
	// Remap to center around uint16=32768 (landscape zero plane, ZScale=100).
	// float 0 (ocean) -> uint16 16384 -> -128m
	// float 0.5 (coast) -> uint16 32768 -> 0m (sea level)
	// float 1.0 (peak)  -> uint16 49152 -> +128m
	for (uint16& V : HeightData)
	{
		V = static_cast<uint16>(static_cast<int32>(V) / 2 + 16384);
	}
	if (HeightData.Num() != Res * Res)
	{
		UE_LOG(LogLandscapeBuilder, Warning,
			TEXT("BuildLandscape  --  HeightData size (%d) != Res*Res (%d)."),
			HeightData.Num(), Res * Res);
		return false;
	}

	// 2. Transform: center positioning
	const float HalfSize = (Res - 1) * Settings->QuadSize * 0.5f;
	const FTransform LandscapeTransform(
		FRotator::ZeroRotator,
		FVector(-HalfSize, -HalfSize, 0.0f),
		FVector(Settings->QuadSize, Settings->QuadSize, 50.0f)
	);

	// 3. Landscape creation
	ALandscape* Landscape = CreateLandscape(World, LandscapeTransform);
	if (!Landscape)
	{
		UE_LOG(LogLandscapeBuilder, Warning, TEXT("BuildLandscape  --  CreateLandscape failed."));
		return false;
	}

	// Water/brush workflows rely on edit layers being enabled on the landscape.
	if (FBoolProperty* CanHaveLayersProp = FindFProperty<FBoolProperty>(Landscape->GetClass(), TEXT("bCanHaveLayersContent")))
	{
		CanHaveLayersProp->SetPropertyValue_InContainer(Landscape, true);
		UE_LOG(LogLandscapeBuilder, Display, TEXT("BuildLandscape -- enabled bCanHaveLayersContent (Edit Layers)."));
	}
	else
	{
		UE_LOG(LogLandscapeBuilder, Warning, TEXT("BuildLandscape -- bCanHaveLayersContent property not found."));
	}

	// 4a. Assign grass material before Import so all components inherit it
	{
		UMaterialInterface* LandscapeMat = GetOrCreateGrassMaterial();
		if (LandscapeMat)
		{
			Landscape->LandscapeMaterial = LandscapeMat;
			UE_LOG(LogLandscapeBuilder, Display, TEXT("BuildLandscape -- Grass material assigned."));
		}
		else
		{
			UE_LOG(LogLandscapeBuilder, Warning, TEXT("BuildLandscape -- Could not load or create grass material."));
		}
	}

	// 4b. Heightmap import
	if (!ImportHeightmap(Landscape, HeightData, Res))
	{
		UE_LOG(LogLandscapeBuilder, Warning, TEXT("BuildLandscape -- ImportHeightmap failed."));
		return false;
	}

	#if WITH_EDITOR
	EnsureWaterEditLayer(Landscape);
	#endif

	// 5. Reset transform: Import() may override the spawn transform.
	// Center positioning: landscape (-HalfSize,-HalfSize,0) -> (+HalfSize,+HalfSize)
	ConfigureLandscapeTransform(Landscape, Settings);

	const FVector ActualLoc = Landscape->GetActorLocation();
	UE_LOG(LogLandscapeBuilder, Display, TEXT("BuildLandscape -- Landscape world position: %s"),
		*FString::Printf(TEXT("(%.0f, %.0f, %.0f)"), ActualLoc.X, ActualLoc.Y, ActualLoc.Z));

	GeneratedLandscape = Landscape;

	if (Biome && Biome->IsInitialized())
	{
		ApplyMaterialLayers(Landscape, Biome, Res);
	}

	UE_LOG(LogLandscapeBuilder, Display,
		TEXT("BuildLandscape  --  done. Resolution: %dx%d | QuadSize: %d cm"),
		Res, Res, Settings->QuadSize);

	return true;
}

ALandscape* ULandscapeBuilder::CreateLandscape(UWorld* World, const FTransform& Transform)
{
	if (!World) return nullptr;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ALandscape* Landscape = World->SpawnActor<ALandscape>(
		ALandscape::StaticClass(),
		Transform,
		SpawnParams
	);

	if (!Landscape)
	{
		UE_LOG(LogLandscapeBuilder, Warning, TEXT("CreateLandscape  --  SpawnActor failed."));
		return nullptr;
	}

	UE_LOG(LogLandscapeBuilder, Log, TEXT("CreateLandscape  --  ALandscape spawned."));
	return Landscape;
}

bool ULandscapeBuilder::ImportHeightmap(ALandscape* Landscape, const TArray<uint16>& HeightData, int32 Resolution)
{
	if (!Landscape) return false;

	// Component and section count calculation
	// UE Landscape valid SubsectionSizeQuads values: 255, 127, 63, 31, 15, 7, 3, 1.
	// We pick the largest valid size that fits within TotalQuads (no exact divisibility
	// required -- UE's Import uses integer division to determine NumComponents, so any
	// remainder quads are simply left uncovered, which is visually imperceptible).
	// This avoids the catastrophic fallback to QuadsPerSection=1 which creates
	// TotalQuads^2 components (e.g. 512^2 = 262144 for Resolution=513).
	const int32 TotalQuads = Resolution - 1;
	static const int32 ValidSizes[] = {255, 127, 63, 31, 15, 7, 3, 1};
	int32 QuadsPerSection = 1;
	for (int32 Size : ValidSizes)
	{
		if (TotalQuads >= Size)
		{
			QuadsPerSection = Size;
			break;
		}
	}
	const int32 SectionsPerComp = 1;

	// Assemble import data  --  TMap-based API (UE 5.6)
	TMap<FGuid, TArray<uint16>> HeightmapDataPerLayer;
	HeightmapDataPerLayer.Add(FGuid(), HeightData);

	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> LayerInfosPerLayer;
	LayerInfosPerLayer.Add(FGuid(), TArray<FLandscapeImportLayerInfo>());

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	Landscape->Import(
		FGuid::NewGuid(),
		0, 0,
		Resolution - 1, Resolution - 1,
		SectionsPerComp,
		QuadsPerSection,
		HeightmapDataPerLayer,
		nullptr,
		LayerInfosPerLayer,
		ELandscapeImportAlphamapType::Additive
	);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// LandscapeInfo registration
	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	if (LandscapeInfo)
	{
		LandscapeInfo->UpdateLayerInfoMap(Landscape);
	}

	UE_LOG(LogLandscapeBuilder, Log,
		TEXT("ImportHeightmap  --  done. %dx%d vertices."), Resolution, Resolution);

	return true;
}

void ULandscapeBuilder::ConfigureLandscapeTransform(ALandscape* Landscape, const UMapGeneratorSettings* Settings)
{
	if (!Landscape || !Settings) return;

	const float HalfSize = (Settings->Resolution - 1) * Settings->QuadSize * 0.5f;
	if (USceneComponent* Root = Landscape->GetRootComponent())
	{
		// Temporarily movable so transform updates won't be rejected during runtime generation.
		Root->SetMobility(EComponentMobility::Movable);

		Landscape->SetActorLocation(FVector(-HalfSize, -HalfSize, 0.0f));
		Landscape->SetActorScale3D(FVector(
			Settings->QuadSize,
			Settings->QuadSize,
			50.0f  // Z scale: 512 UU = 1 height unit in the Landscape
		));

		// Water/Landscape integration is more stable when the final landscape mobility is static.
		Root->SetMobility(EComponentMobility::Static);
	}
	else
	{
		Landscape->SetActorLocation(FVector(-HalfSize, -HalfSize, 0.0f));
		Landscape->SetActorScale3D(FVector(
			Settings->QuadSize,
			Settings->QuadSize,
			50.0f  // Z scale: 512 UU = 1 height unit in the Landscape
		));
	}

	UE_LOG(LogLandscapeBuilder, Log,
		TEXT("ConfigureLandscapeTransform  --  Location=(%.0f, %.0f, 0) Scale=(%d, %d, 512)"),
		-HalfSize, -HalfSize, Settings->QuadSize, Settings->QuadSize);
}


TArray<uint16> ULandscapeBuilder::ConvertHeightmapToUint16(const TArray<float>& Heightmap) const
{
	TArray<uint16> Result;
	Result.SetNumUninitialized(Heightmap.Num());

	for (int32 i = 0; i < Heightmap.Num(); ++i)
	{
		const float Clamped = FMath::Clamp(Heightmap[i], 0.0f, 1.0f);
		Result[i] = static_cast<uint16>(Clamped * 65535.0f);
	}

	UE_LOG(LogLandscapeBuilder, Verbose,
		TEXT("ConvertHeightmapToUint16  --  %d values converted."), Result.Num());

	return Result;
}


TMap<EBiomeType, TArray<uint8>> ULandscapeBuilder::CalculateWeightMaps(UBiomeManager* Biome, int32 Resolution) const
{
	TMap<EBiomeType, TArray<uint8>> WeightMaps;

	if (!Biome || !Biome->IsInitialized() || Resolution <= 0)
	{
		return WeightMaps;
	}

	// Initialize a zero-array for each biome
	const TArray<EBiomeType> SupportedBiomes =
	{
		EBiomeType::Ocean, EBiomeType::Beach, EBiomeType::Plains,
		EBiomeType::Forest, EBiomeType::Hills, EBiomeType::Mountain,
		EBiomeType::Swamp
	};

	for (EBiomeType BiomeType : SupportedBiomes)
	{
		WeightMaps.Add(BiomeType).SetNumZeroed(Resolution * Resolution);
	}

	// Convert blend weights from float [0,1] to uint8 [0, 255]
	for (int32 Y = 0; Y < Resolution; ++Y)
	{
		for (int32 X = 0; X < Resolution; ++X)
		{
			const int32 Index = Y * Resolution + X;
			const TMap<EBiomeType, float> BlendWeights = Biome->GetBlendWeightsAt(X, Y);

			for (EBiomeType BiomeType : SupportedBiomes)
			{
				const float* WeightPtr = BlendWeights.Find(BiomeType);
				const float Weight = WeightPtr ? *WeightPtr : 0.0f;
				WeightMaps[BiomeType][Index] = static_cast<uint8>(
					FMath::Clamp(FMath::RoundToInt(Weight * 255.0f), 0, 255));
			}
		}
	}

	UE_LOG(LogLandscapeBuilder, Log,
		TEXT("CalculateWeightMaps  --  %d biome weight maps created. Resolution: %dx%d"),
		WeightMaps.Num(), Resolution, Resolution);

	return WeightMaps;
}

bool ULandscapeBuilder::ApplyMaterialLayers(ALandscape* Landscape, UBiomeManager* Biome, int32 Resolution)
{
	if (LayerInfoAssets.Num() == 0)
	{
		UE_LOG(LogLandscapeBuilder, Log,
			TEXT("ApplyMaterialLayers  --  no LayerInfoAssets assigned, skipping."));
		return false;
	}
	if (!Landscape)
	{
		UE_LOG(LogLandscapeBuilder, Warning, TEXT("ApplyMaterialLayers  --  Landscape null."));
		return false;
	}
	if (!Biome || !Biome->IsInitialized())
	{
		UE_LOG(LogLandscapeBuilder, Warning, TEXT("ApplyMaterialLayers  --  Biome null or not initialized."));
		return false;
	}

	TMap<EBiomeType, TArray<uint8>> WeightMaps = CalculateWeightMaps(Biome, Resolution);

#if WITH_EDITOR
	int32 AppliedCount = 0;

	for (const auto& LayerPair : LayerInfoAssets)
	{
		const EBiomeType BiomeType   = LayerPair.Key;
		ULandscapeLayerInfoObject* LayerInfo = LayerPair.Value.Get();

		if (!LayerInfo) continue;

		const TArray<uint8>* WeightData = WeightMaps.Find(BiomeType);
		if (!WeightData || WeightData->Num() == 0) continue;

		LandscapeEditorUtils::SetWeightmapData(Landscape, LayerInfo, *WeightData);
		++AppliedCount;

		UE_LOG(LogLandscapeBuilder, Log,
			TEXT("ApplyMaterialLayers  --  %s layer applied."),
			*UEnum::GetValueAsString(BiomeType));
	}

	UE_LOG(LogLandscapeBuilder, Display,
		TEXT("ApplyMaterialLayers  --  %d / %d layers applied."),
		AppliedCount, LayerInfoAssets.Num());

	return AppliedCount > 0;
#else
	UE_LOG(LogLandscapeBuilder, Log,
		TEXT("ApplyMaterialLayers  --  only available in editor builds."));
	return false;
#endif
}

// Landscape material loading

UMaterialInterface* ULandscapeBuilder::GetOrCreateGrassMaterial()
{
	static const TCHAR* LandscapeMatPath = TEXT("/Game/MapGenerator/Landscape/M_LandscapeMaterial.M_LandscapeMaterial");
	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, LandscapeMatPath);
	if (Mat)
	{
		UE_LOG(LogLandscapeBuilder, Display, TEXT("GetOrCreateGrassMaterial -- Using M_LandscapeMaterial."));
	}
	else
	{
		UE_LOG(LogLandscapeBuilder, Warning,
			TEXT("GetOrCreateGrassMaterial -- M_LandscapeMaterial not found at %s. No material will be applied."),
			LandscapeMatPath);
	}
	return Mat;
}

