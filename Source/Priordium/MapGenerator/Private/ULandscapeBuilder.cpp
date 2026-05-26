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

#include "LandscapeComponent.h"       // ELandscapeLayerUpdateMode (runtime header, needed for FlattenArea)
#include "RenderCommandFence.h"       // FlushRenderingCommands() – forces render thread sync after GPU texture uploads
#include "NavigationSystem.h"         // UNavigationSystemV1::AddDirtyArea – navmesh rebuild after landscape edit

#if WITH_EDITOR
#include "LandscapeEdit.h"
#include "LandscapeEditorUtils.h"
#include "LandscapeEditLayer.h"       // ULandscapeEditLayerBase::GetGuid(), FScopedSetLandscapeEditingLayer
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
#if !WITH_EDITOR
	return false;  // Landscape import is editor-only; this path is never reached in packaged builds.
#else
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
#endif // WITH_EDITOR
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


void ULandscapeBuilder::FlattenArea(float CenterX, float CenterY, float HalfExtent, const UMapGeneratorSettings* Settings)
{
#if WITH_EDITOR
	if (!GeneratedLandscape || !Settings)
	{
		UE_LOG(LogLandscapeBuilder, Warning, TEXT("FlattenArea: GeneratedLandscape or Settings is null."));
		return;
	}

	ALandscape* Landscape = Cast<ALandscape>(GeneratedLandscape);
	if (!Landscape)
	{
		UE_LOG(LogLandscapeBuilder, Warning, TEXT("FlattenArea: GeneratedLandscape is not an ALandscape."));
		return;
	}

	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	if (!LandscapeInfo)
	{
		UE_LOG(LogLandscapeBuilder, Warning, TEXT("FlattenArea: LandscapeInfo is null."));
		return;
	}

	// Get actual landscape extent (vertex index bounds from the import).
	int32 LandMinX, LandMinY, LandMaxX, LandMaxY;
	if (!LandscapeInfo->GetLandscapeExtent(LandMinX, LandMinY, LandMaxX, LandMaxY))
	{
		UE_LOG(LogLandscapeBuilder, Warning, TEXT("FlattenArea: GetLandscapeExtent failed."));
		return;
	}

	// Expand the flattened area by 20% so the terrain around the building
	// has a wider, gentler transition — reduces navmesh fragmentation at the edges.
	const float ExpandedHalfExtent = HalfExtent * 1.2f;

	// Convert world-space footprint bounds to landscape vertex indices.
	const FVector LandLoc   = Landscape->GetActorLocation();
	const FVector LandScale = Landscape->GetActorScale3D();

	if (FMath::IsNearlyZero(LandScale.X) || FMath::IsNearlyZero(LandScale.Y))
	{
		UE_LOG(LogLandscapeBuilder, Warning, TEXT("FlattenArea: Landscape scale is ~zero."));
		return;
	}

	// Non-const: FLandscapeEditDataInterface::GetHeightData takes int32& (modifies the coords to actual extent)
	int32 MinVertX = FMath::Clamp(FMath::FloorToInt((CenterX - ExpandedHalfExtent - LandLoc.X) / LandScale.X), LandMinX, LandMaxX);
	int32 MinVertY = FMath::Clamp(FMath::FloorToInt((CenterY - ExpandedHalfExtent - LandLoc.Y) / LandScale.Y), LandMinY, LandMaxY);
	int32 MaxVertX = FMath::Clamp(FMath::CeilToInt( (CenterX + ExpandedHalfExtent - LandLoc.X) / LandScale.X), LandMinX, LandMaxX);
	int32 MaxVertY = FMath::Clamp(FMath::CeilToInt( (CenterY + ExpandedHalfExtent - LandLoc.Y) / LandScale.Y), LandMinY, LandMaxY);

	if (MinVertX >= MaxVertX || MinVertY >= MaxVertY)
	{
		UE_LOG(LogLandscapeBuilder, Warning,
			TEXT("FlattenArea: degenerate vertex range [%d,%d]-[%d,%d] -- footprint outside landscape?"),
			MinVertX, MinVertY, MaxVertX, MaxVertY);
		return;
	}

	const int32 SizeX = MaxVertX - MinVertX + 1;
	const int32 SizeY = MaxVertY - MinVertY + 1;

	TArray<uint16> HeightData;
	HeightData.SetNumZeroed(SizeX * SizeY);

	UWorld* World = GetWorld();
	const bool bIsEditorWorld = World && World->WorldType == EWorldType::Editor;

	// ─── EDITOR: write through the edit-layer system (non-destructive, persists) ──
	if (bIsEditorWorld)
	{
		// Determine which edit layer to target (layer 0 = base layer from the import).
		FGuid BaseLayerGuid;
		if (Landscape->GetLayersConst().Num() > 0)
		{
			if (const FLandscapeLayer* BaseLayer = Landscape->GetLayerConst(0))
			{
				if (BaseLayer->EditLayer)
				{
					BaseLayerGuid = BaseLayer->EditLayer->GetGuid();
					UE_LOG(LogLandscapeBuilder, Log,
						TEXT("FlattenArea: targeting edit layer '%s' (guid %s)."),
						*BaseLayer->EditLayer->GetName().ToString(),
						*BaseLayerGuid.ToString());
				}
			}
		}

		// GetHeightData AND SetHeightData must be in the same FScopedSetLandscapeEditingLayer
		// so both target the same layer. The lambda fires on scope exit and queues the GPU
		// recomposition of all layers into the final heightmap.
		{
			FScopedSetLandscapeEditingLayer ScopedLayer(Landscape, BaseLayerGuid, [Landscape]()
			{
				Landscape->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_All);
			});

			FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
			LandscapeEdit.GetHeightData(MinVertX, MinVertY, MaxVertX, MaxVertY, HeightData.GetData(), 0);

			uint16 MinHeight = TNumericLimits<uint16>::Max();
			for (const uint16 H : HeightData)
			{
				if (H > 0) MinHeight = FMath::Min(MinHeight, H);
			}

			if (MinHeight == TNumericLimits<uint16>::Max())
			{
				UE_LOG(LogLandscapeBuilder, Warning,
					TEXT("FlattenArea: GetHeightData returned only zeroes -- layer GUID may be wrong."));
				return;
			}

			// Blend edges toward the original terrain height to avoid steep walls
			// that fragment the navmesh. Interior vertices are fully flat; border
			// vertices lerp smoothly back to their pre-flatten height.
			{
				const TArray<uint16> OriginalHeights = HeightData;
				constexpr int32 BlendVerts = 4;
				for (int32 VY = 0; VY < SizeY; ++VY)
				{
					for (int32 VX = 0; VX < SizeX; ++VX)
					{
						const int32 DistFromEdge = FMath::Min(FMath::Min(VX, SizeX - 1 - VX), FMath::Min(VY, SizeY - 1 - VY));
						const float Alpha = FMath::Clamp(static_cast<float>(DistFromEdge) / BlendVerts, 0.f, 1.f);
						const uint16 Orig = OriginalHeights[VY * SizeX + VX];
						HeightData[VY * SizeX + VX] = static_cast<uint16>(
							FMath::Lerp(static_cast<float>(Orig), static_cast<float>(MinHeight), Alpha));
					}
				}
			}

			LandscapeEdit.SetHeightData(MinVertX, MinVertY, MaxVertX, MaxVertY,
				HeightData.GetData(), 0, /*CalcNormals=*/true);

			UE_LOG(LogLandscapeBuilder, Display,
				TEXT("FlattenArea: vertices [%d,%d]-[%d,%d] (%dx%d) flattened to uint16=%d (BlendVerts=4)."),
				MinVertX, MinVertY, MaxVertX, MaxVertY, SizeX, SizeY, MinHeight);

		} // ~FScopedSetLandscapeEditingLayer → RequestLayersContentUpdate fires here

		Landscape->MarkPackageDirty();
	}
	// ─── PIE: bypass the async layer pipeline, write directly to the composited
	//          heightmap so the change is immediately visible without waiting for
	//          the editor's deferred layer-recomposition tick. ──────────────────
	else
	{
		// Without FScopedSetLandscapeEditingLayer the edit-data interface operates
		// on the raw / composited heightmap texture (the final rendered data) rather
		// than a source edit-layer texture, giving us an immediate CPU-side write.
		FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
		LandscapeEdit.GetHeightData(MinVertX, MinVertY, MaxVertX, MaxVertY, HeightData.GetData(), 0);

		uint16 MinHeight = TNumericLimits<uint16>::Max();
		for (const uint16 H : HeightData)
		{
			if (H > 0) MinHeight = FMath::Min(MinHeight, H);
		}

		if (MinHeight == TNumericLimits<uint16>::Max())
		{
			UE_LOG(LogLandscapeBuilder, Warning,
				TEXT("FlattenArea (PIE): GetHeightData returned only zeroes."));
			return;
		}

		{
			const TArray<uint16> OriginalHeights = HeightData;
			constexpr int32 BlendVerts = 4;
			for (int32 VY = 0; VY < SizeY; ++VY)
			{
				for (int32 VX = 0; VX < SizeX; ++VX)
				{
					const int32 DistFromEdge = FMath::Min(FMath::Min(VX, SizeX - 1 - VX), FMath::Min(VY, SizeY - 1 - VY));
					const float Alpha = FMath::Clamp(static_cast<float>(DistFromEdge) / BlendVerts, 0.f, 1.f);
					const uint16 Orig = OriginalHeights[VY * SizeX + VX];
					HeightData[VY * SizeX + VX] = static_cast<uint16>(
						FMath::Lerp(static_cast<float>(Orig), static_cast<float>(MinHeight), Alpha));
				}
			}
		}

		LandscapeEdit.SetHeightData(MinVertX, MinVertY, MaxVertX, MaxVertY,
			HeightData.GetData(), 0, /*CalcNormals=*/true);
	}

	// ─── Both paths: force GPU texture re-upload and collision rebuild ─────────
	// After the CPU-side write (either via edit layer or direct), re-upload the
	// composited heightmap texture to the GPU and invalidate the render state so
	// the mesh is reconstructed immediately. FlushRenderingCommands() blocks until
	// the render thread has processed all uploads — guaranteeing PIE sees the
	// correct terrain on the very next rendered frame.
	for (ULandscapeComponent* Component : Landscape->LandscapeComponents)
	{
		if (!Component) continue;

		const int32 CompMinX = Component->SectionBaseX;
		const int32 CompMinY = Component->SectionBaseY;
		const int32 CompMaxX = CompMinX + Component->ComponentSizeQuads;
		const int32 CompMaxY = CompMinY + Component->ComponentSizeQuads;

		if (CompMaxX < MinVertX || CompMinX > MaxVertX ||
			CompMaxY < MinVertY || CompMinY > MaxVertY)
		{
			continue;
		}

		Component->RequestHeightmapUpdate(/*bUpdateAll=*/false, /*bUpdateCollision=*/true);

		if (UTexture2D* HeightmapTex = Component->GetHeightmap())
		{
			HeightmapTex->UpdateResource();
		}

		Component->MarkRenderStateDirty();
	}

	FlushRenderingCommands();

	// Notify the navmesh system that the landscape geometry changed in this region.
	// Without this, navmesh tiles over the flattened area stay stale and fragment.
	// This must come AFTER FlushRenderingCommands() so the collision body is up-to-date
	// before the navmesh queries the new geometry.
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		const FBox DirtyArea(
			FVector(CenterX - ExpandedHalfExtent, CenterY - ExpandedHalfExtent, -HALF_WORLD_MAX),
			FVector(CenterX + ExpandedHalfExtent, CenterY + ExpandedHalfExtent,  HALF_WORLD_MAX)
		);
		NavSys->AddDirtyArea(DirtyArea, ENavigationDirtyFlag::All);
	}
#endif
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

