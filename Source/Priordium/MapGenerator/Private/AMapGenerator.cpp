// Copyright Priordium. All Rights Reserved.
//
// AMapGenerator.cpp
// Map generation pipeline orchestration.

#include "AMapGenerator.h"
#include "UHeightmapGenerator.h"
#include "UBiomeManager.h"
#include "ULandscapeBuilder.h"
#include "UWaterSystemBuilder.h"
#include "UClimateZoneManager.h"
#include "UResourceDistributor.h"
#include "UTribeGenerator.h"
#include "UMapGeneratorSettings.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"

#if WITH_EDITOR
#include "Misc/ScopedSlowTask.h"
#include "Editor.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogMapGenerator, Log, All);

AMapGenerator::AMapGenerator()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

	HeightmapGenerator  = CreateDefaultSubobject<UHeightmapGenerator>(TEXT("HeightmapGenerator"));
	BiomeManager        = CreateDefaultSubobject<UBiomeManager>(TEXT("BiomeManager"));
	LandscapeBuilder    = CreateDefaultSubobject<ULandscapeBuilder>(TEXT("LandscapeBuilder"));
	WaterSystemBuilder  = CreateDefaultSubobject<UWaterSystemBuilder>(TEXT("WaterSystemBuilder"));
	ClimateZoneManager  = CreateDefaultSubobject<UClimateZoneManager>(TEXT("ClimateZoneManager"));
	ResourceDistributor = CreateDefaultSubobject<UResourceDistributor>(TEXT("ResourceDistributor"));
	TribeGenerator      = CreateDefaultSubobject<UTribeGenerator>(TEXT("TribeGenerator"));
}

void AMapGenerator::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoGenerateOnBeginPlay)
	{
		UE_LOG(LogMapGenerator, Display, TEXT("AMapGenerator::BeginPlay -- auto-generate triggered."));
		GenerateWorld();
	}
}

bool AMapGenerator::ExecuteGenerationStep(int32 StepIndex)
{
	using FStepFn = bool (AMapGenerator::*)();
	static const FStepFn Steps[TotalSteps] =
	{
		&AMapGenerator::ExecuteStepHeightmap,
		&AMapGenerator::ExecuteStepBiomes,
		&AMapGenerator::ExecuteStepClimate,
		&AMapGenerator::ExecuteStepWater,
		&AMapGenerator::ExecuteStepLandscape,
		&AMapGenerator::ExecuteStepResources,
		&AMapGenerator::ExecuteStepTribes
	};

	if (StepIndex < 0 || StepIndex >= TotalSteps)
	{
		UE_LOG(LogMapGenerator, Warning,
			TEXT("ExecuteGenerationStep -- invalid step index: %d (valid: 0-%d)"),
			StepIndex, TotalSteps - 1);
		return false;
	}

	return (this->*Steps[StepIndex])();
}

void AMapGenerator::GenerateWorld()
{
	if (bIsGenerating)
	{
		OnGenerationError(TEXT("GenerateWorld called while already generating."));
		return;
	}

	if (!ValidateSettings())
	{
		OnGenerationError(TEXT("GeneratorSettings is null or invalid."));
		return;
	}

	bIsGenerating   = true;
	CurrentProgress = 0.0f;

	UE_LOG(LogMapGenerator, Display, TEXT("AMapGenerator::GenerateWorld -- starting %d-step pipeline."), TotalSteps);

#if WITH_EDITOR
	static const TCHAR* StepNames[TotalSteps] = {
		TEXT("Generating heightmap"),
		TEXT("Assigning biomes"),
		TEXT("Calculating climate zones"),
		TEXT("Building water system"),
		TEXT("Creating landscape"),
		TEXT("Distributing resources"),
		TEXT("Generating Tribes"),
	};
	FScopedSlowTask SlowTask(static_cast<float>(TotalSteps),
		FText::FromString(TEXT("Generating World...")));
	SlowTask.MakeDialog(/*bShowCancelButton=*/true);
#endif

	for (int32 Step = 0; Step < TotalSteps; ++Step)
	{
		CurrentProgress = static_cast<float>(Step) / static_cast<float>(TotalSteps);

#if WITH_EDITOR
		if (SlowTask.ShouldCancel())
		{
			bIsGenerating = false;
			UE_LOG(LogMapGenerator, Warning, TEXT("AMapGenerator::GenerateWorld -- cancelled by user at step %d."), Step);
			return;
		}
		SlowTask.EnterProgressFrame(1.0f, FText::FromString(StepNames[Step]));
#endif

		if (!ExecuteGenerationStep(Step))
		{
			bIsGenerating = false;
			OnGenerationError(FString::Printf(TEXT("Pipeline step %d failed."), Step));
			return;
		}

		OnStepCompleted.Broadcast(Step);
	}

	CurrentProgress = 1.0f;
	bIsGenerating   = false;

	OnGenerationSuccess();
}

void AMapGenerator::ClearGeneratedWorld()
{
	UWorld* World = GetWorld();
	auto DestroyActorSafe = [World](AActor* Actor) -> bool
	{
		if (!IsValid(Actor))
			return false;

#if WITH_EDITOR
		if (GEditor && World && (World->WorldType == EWorldType::Editor || World->WorldType == EWorldType::EditorPreview))
		{
			return World->DestroyActor(Actor, true, true);
		}
#endif

		return Actor->Destroy();
	};

	int32 DestroyedWaterBodies = 0;
	int32 DestroyedLandscapeActors = 0;
	int32 DestroyedFallbackActors = 0;

	// Destroy spawned tribes: find every actor tagged "MapGen_tribe" and destroy it.
	// Tag-based lookup is reliable even when the generator's tracking arrays are stale.
	if (World)
	{
		TArray<AActor*> TribeActors;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->Tags.Contains(FName(TEXT("MapGen_tribe"))))
			{
				TribeActors.Add(*It);
			}
		}
		for (AActor* Actor : TribeActors)
		{
			DestroyActorSafe(Actor);
		}
	}
	// Also reset the generator's tracking arrays
	if (TribeGenerator)
	{
		TribeGenerator->ClearTribes();
	}

	// Destroy spawned resources
	if (ResourceDistributor)
		ResourceDistributor->ClearResources();

	// Destroy spawned water body actors
	if (WaterSystemBuilder)
	{
		for (const TObjectPtr<AActor>& W : WaterSystemBuilder->GetWaterBodies())
		{
			if (DestroyActorSafe(W.Get()))
				++DestroyedWaterBodies;
		}
		WaterSystemBuilder->ClearSpawnedWaterBodies();
	}

	// Destroy generated landscape
	if (LandscapeBuilder && LandscapeBuilder->IsLandscapeCreated())
	{
		ALandscapeProxy* Landscape = LandscapeBuilder->GetGeneratedLandscape();
		if (DestroyActorSafe(Landscape))
			++DestroyedLandscapeActors;
	}

	// Fallback cleanup for editor sessions where generated actors were not tracked.
	if (World)
	{
		auto DestroyByClassPath = [&](const TCHAR* ClassPath) -> int32
		{
			UClass* Class = UClass::TryFindTypeSlow<UClass>(ClassPath);
			if (!Class)
				return 0;

			TArray<AActor*> ActorsToDestroy;
			for (TActorIterator<AActor> It(World, Class); It; ++It)
				ActorsToDestroy.Add(*It);

			int32 LocalDestroyed = 0;
			for (AActor* Actor : ActorsToDestroy)
			{
				if (DestroyActorSafe(Actor))
					++LocalDestroyed;
			}

			return LocalDestroyed;
		};

		DestroyedFallbackActors += DestroyByClassPath(TEXT("/Script/Water.WaterBodyLake"));
		DestroyedFallbackActors += DestroyByClassPath(TEXT("/Script/Water.WaterBodyRiver"));
		DestroyedFallbackActors += DestroyByClassPath(TEXT("/Script/Water.WaterZone"));
		DestroyedFallbackActors += DestroyByClassPath(TEXT("/Script/WaterEditor.WaterBrushManager"));
		DestroyedFallbackActors += DestroyByClassPath(TEXT("/Script/Landscape.LandscapeProxy"));
	}

	// Reset state
	bIsGenerating   = false;
	CurrentProgress = 0.0f;

	UE_LOG(LogMapGenerator, Display,
		TEXT("AMapGenerator::ClearGeneratedWorld -- world cleared. tracked water=%d, tracked landscape=%d, fallback=%d"),
		DestroyedWaterBodies, DestroyedLandscapeActors, DestroyedFallbackActors);
}

void AMapGenerator::GenerateTribesOnly()
{
	if (bIsGenerating)
	{
		UE_LOG(LogMapGenerator, Warning, TEXT("GenerateTribesOnly -- generation already in progress."));
		return;
	}

	if (!TribeGenerator)
	{
		UE_LOG(LogMapGenerator, Warning, TEXT("GenerateTribesOnly -- TribeGenerator is null."));
		return;
	}

	if (!HeightmapGenerator || !HeightmapGenerator->IsInitialized())
	{
		UE_LOG(LogMapGenerator, Warning,
			TEXT("GenerateTribesOnly -- HeightmapGenerator is null or not initialized. Run GenerateWorld() first."));
		return;
	}

	if (!ValidateSettings())
	{
		UE_LOG(LogMapGenerator, Warning, TEXT("GenerateTribesOnly -- settings invalid."));
		return;
	}

	bIsGenerating = true;

#if WITH_EDITOR
	FScopedSlowTask SlowTask(2.f, FText::FromString(TEXT("Generating Tribes...")));
	SlowTask.MakeDialog(false);
	SlowTask.EnterProgressFrame(1.f, FText::FromString(TEXT("Clearing existing tribes...")));
#endif

	TribeGenerator->ClearTribes();

#if WITH_EDITOR
	SlowTask.EnterProgressFrame(1.f, FText::FromString(TEXT("Spawning tribes...")));
#endif

	const bool bGenerated = TribeGenerator->GenerateTribes(GeneratorSettings, HeightmapGenerator, LandscapeBuilder);

	bIsGenerating = false;

	if (bGenerated)
	{
		UE_LOG(LogMapGenerator, Display, TEXT("GenerateTribesOnly -- done."));
		OnGenerationCompleted.Broadcast();
	}
	else
	{
		UE_LOG(LogMapGenerator, Warning,
			TEXT("GenerateTribesOnly -- GenerateTribes() spawned no tribes. Check TribeManClass/StorageClass in settings."));
	}
}

bool AMapGenerator::ValidateSettings() const
{
	if (!GeneratorSettings)
	{
		UE_LOG(LogMapGenerator, Warning, TEXT("ValidateSettings -- GeneratorSettings is null."));
		return false;
	}

	FString ValidationError;
	const bool bValid = GeneratorSettings->Validate(&ValidationError);
	if (!bValid)
	{
		UE_LOG(LogMapGenerator, Warning, TEXT("ValidateSettings -- %s"), *ValidationError);
	}
	return bValid;
}

bool AMapGenerator::ExecuteStepHeightmap()
{
	if (!HeightmapGenerator)
	{
		UE_LOG(LogMapGenerator, Warning, TEXT("Step 0 failed: HeightmapGenerator is null"));
		return false;
	}

	UE_LOG(LogMapGenerator, Display, TEXT("Step 0 -- HeightmapGenerator::Generate()"));
	const bool bOk = HeightmapGenerator->Generate(GeneratorSettings);
	if (!bOk)
	{
		UE_LOG(LogMapGenerator, Warning, TEXT("Step 0 failed: HeightmapGenerator::Generate"));
	}
	return bOk;
}

bool AMapGenerator::ExecuteStepBiomes()
{
	if (!BiomeManager || !HeightmapGenerator)
	{
		UE_LOG(LogMapGenerator, Warning, TEXT("Step 1 failed: BiomeManager or HeightmapGenerator is null"));
		return false;
	}

	UE_LOG(LogMapGenerator, Display, TEXT("Step 1 -- BiomeManager::AssignBiomes()"));
	const bool bOk = BiomeManager->AssignBiomes(GeneratorSettings, HeightmapGenerator);
	if (!bOk)
	{
		UE_LOG(LogMapGenerator, Warning, TEXT("Step 1 failed: BiomeManager::AssignBiomes"));
	}
	return bOk;
}

bool AMapGenerator::ExecuteStepClimate()
{
	if (!ClimateZoneManager || !BiomeManager)
	{
		UE_LOG(LogMapGenerator, Warning, TEXT("Step 2 failed: ClimateZoneManager or BiomeManager is null"));
		return false;
	}

	UE_LOG(LogMapGenerator, Display, TEXT("Step 2 -- ClimateZoneManager::CalculateClimateZones()"));
	const bool bOk = ClimateZoneManager->CalculateClimateZones(BiomeManager->GetBiomeMap(), GeneratorSettings);
	if (!bOk)
	{
		UE_LOG(LogMapGenerator, Warning, TEXT("Step 2 failed: ClimateZoneManager::CalculateClimateZones"));
	}
	return bOk;
}

bool AMapGenerator::ExecuteStepWater()
{
	if (!WaterSystemBuilder || !HeightmapGenerator)
	{
		UE_LOG(LogMapGenerator, Warning, TEXT("Step 3 failed: WaterSystemBuilder or HeightmapGenerator is null"));
		return false;
	}

	const float CellSize = GeneratorSettings ? static_cast<float>(GeneratorSettings->QuadSize) : 100.0f;
	UE_LOG(LogMapGenerator, Display, TEXT("Step 3 -- WaterSystemBuilder::BuildWaterBodies() [definitions only]"));
	const bool bBuilt = WaterSystemBuilder->BuildWaterBodies(HeightmapGenerator, GeneratorSettings, CellSize, false);
	if (!bBuilt)
	{
		// 0 water bodies is valid when bGenerateRivers/bGenerateLakes are disabled,
		// or when no suitable river sources / lake locations exist on a small map.
		// Consistent with Step 5 (DistributeResources): absence of content is not a
		// pipeline failure -- only a missing/null component is treated as an error.
		UE_LOG(LogMapGenerator, Display,
			TEXT("Step 3 -- WaterSystemBuilder generated 0 bodies (water disabled or no valid locations). Treated as success."));
	}
	return true;
}

bool AMapGenerator::ExecuteStepLandscape()
{
	if (!LandscapeBuilder || !HeightmapGenerator || !BiomeManager)
	{
		UE_LOG(LogMapGenerator, Warning, TEXT("Step 4 failed: LandscapeBuilder, HeightmapGenerator or BiomeManager is null"));
		return false;
	}

	UE_LOG(LogMapGenerator, Display, TEXT("Step 4 -- LandscapeBuilder::BuildLandscape()"));
	const bool bOk = LandscapeBuilder->BuildLandscape(GeneratorSettings, HeightmapGenerator, BiomeManager);
	if (!bOk)
	{
		UE_LOG(LogMapGenerator, Warning, TEXT("Step 4 failed: LandscapeBuilder::BuildLandscape"));
		return false;
	}

	if (WaterSystemBuilder)
	{
		const float CellSize = GeneratorSettings ? static_cast<float>(GeneratorSettings->QuadSize) : 100.0f;
		UE_LOG(LogMapGenerator, Display, TEXT("Step 4 -- WaterSystemBuilder::SpawnGeneratedWaterBodies()"));
		WaterSystemBuilder->SpawnGeneratedWaterBodies(HeightmapGenerator, CellSize);
	}

	return bOk;
}

bool AMapGenerator::ExecuteStepResources()
{
	if (!ResourceDistributor || !HeightmapGenerator || !BiomeManager || !ClimateZoneManager || !WaterSystemBuilder)
	{
		UE_LOG(LogMapGenerator, Warning,
			TEXT("Step 5 failed: one or more required components are null"));
		return false;
	}

	const float CellSize = GeneratorSettings ? static_cast<float>(GeneratorSettings->QuadSize) : 100.0f;
	UE_LOG(LogMapGenerator, Display, TEXT("Step 5 -- ResourceDistributor::DistributeResources()"));
	const bool bDistributed = ResourceDistributor->DistributeResources(
		GeneratorSettings, HeightmapGenerator, BiomeManager,
		ClimateZoneManager, WaterSystemBuilder, CellSize);

	if (!bDistributed)
	{
		// DistributeResources can return false if no rules are configured -- not an error
		UE_LOG(LogMapGenerator, Display,
			TEXT("Step 5 -- ResourceDistributor::DistributeResources() reported no distributions (treated as success)."));
	}

	return true;
}

bool AMapGenerator::ExecuteStepTribes()
{
	if (!TribeGenerator || !HeightmapGenerator)
	{
		UE_LOG(LogMapGenerator, Warning, TEXT("Step 6 failed: TribeGenerator or HeightmapGenerator is null"));
		return false;
	}

	UE_LOG(LogMapGenerator, Display, TEXT("Step 6 -- TribeGenerator::GenerateTribes()"));
	const bool bGenerated = TribeGenerator->GenerateTribes(GeneratorSettings, HeightmapGenerator, LandscapeBuilder);

	if (!bGenerated)
	{
		// No tribes spawned is not a pipeline error (for example, if TribeManClass
		// is not set in the settings), just like the ResourceDistributor case.
		UE_LOG(LogMapGenerator, Display,
			TEXT("Step 6 -- TribeGenerator::GenerateTribes() spawned no tribes (treated as success)."));
	}

	return true;
}

// ----------------------------------------------------------------------------
// Private helpers
// ----------------------------------------------------------------------------

void AMapGenerator::OnGenerationSuccess()
{
	// Rebuild NavMesh after landscape + water generation so AI can navigate the new terrain.
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		NavSys->Build();
		UE_LOG(LogMapGenerator, Display, TEXT("AMapGenerator -- NavMesh rebuild triggered."));
	}

	UE_LOG(LogMapGenerator, Display, TEXT("AMapGenerator -- GenerateWorld completed successfully."));
	OnGenerationCompleted.Broadcast();
}

void AMapGenerator::OnGenerationError(const FString& Reason)
{
	UE_LOG(LogMapGenerator, Warning,
		TEXT("AMapGenerator -- GenerateWorld failed: %s"), *Reason);
	OnGenerationFailed.Broadcast(Reason);
}
