// Copyright Priordium. All Rights Reserved.
//
// UTribeGenerator.cpp
// Tribe placement implementation.

#include "UTribeGenerator.h"
#include "UHeightmapGenerator.h"
#include "UMapGeneratorSettings.h"
#include "TribeManager.h"
#include "QuestManager.h"
#include "ULandscapeBuilder.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "LandscapeProxy.h"
#include "UObject/UnrealType.h"
#include "EngineUtils.h"
#include "Kismet/KismetSystemLibrary.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

// Tag applied to every actor spawned by the tribe generator.
// ClearTribes() uses this to find and destroy them even if the tracking arrays are stale.
static const FName GMapGenTribeTag(TEXT("MapGen_tribe"));

DEFINE_LOG_CATEGORY_STATIC(LogTribeGenerator, Log, All);

// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------

UTribeGenerator::UTribeGenerator()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UTribeGenerator::GenerateTribes(const UMapGeneratorSettings* Settings, const UHeightmapGenerator* Heightmap, ULandscapeBuilder* LandscapeBuilder)
{
	ClearTribes();

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTribeGenerator, Warning, TEXT("GenerateTribes: World is null."));
		return false;
	}
	if (!Settings)
	{
		UE_LOG(LogTribeGenerator, Warning, TEXT("GenerateTribes: Settings is null."));
		return false;
	}
	if (!Heightmap || !Heightmap->IsInitialized())
	{
		UE_LOG(LogTribeGenerator, Warning, TEXT("GenerateTribes: Heightmap is null or not initialized."));
		return false;
	}
	if (!Settings->TribeManClass || !Settings->StorageClass)
	{
		UE_LOG(LogTribeGenerator, Warning,
			TEXT("GenerateTribes: TribeManClass or StorageClass is not set in settings."));
		return false;
	}

	const int32 Seed = Settings->Seed != 0 ? Settings->Seed + 137 : FMath::Rand();
	FRandomStream Stream(Seed);

	const TArray<FVector2D> LandCandidates = BuildLandCandidates(Settings, Heightmap);

	if (LandCandidates.IsEmpty())
	{
		UE_LOG(LogTribeGenerator, Warning,
			TEXT("GenerateTribes: No land candidates found. Is the entire map below sea level?"));
		return false;
	}

	// Select tribe positions: as far apart as possible
	const int32            ActualCount    = FMath::Min(Settings->TribeCount, LandCandidates.Num());
	const TArray<FVector2D> TribePositions = SelectTribePositions(LandCandidates, ActualCount, Stream);

	int32 SpawnedCount = 0;
	for (const FVector2D& Pos2D : TribePositions)
	{
		float TerrainZ = 0.f;
		if (!Heightmap->GetTerrainHeight(World, Pos2D.X, Pos2D.Y, TerrainZ, Settings))
		{
			UE_LOG(LogTribeGenerator, Warning,
				TEXT("GenerateTribes: No terrain hit at (%.0f, %.0f) -- skipping tribe."),
				Pos2D.X, Pos2D.Y);
			continue;
		}

		ATribeManager* TribeManager = SpawnTribe(World, FVector(Pos2D.X, Pos2D.Y, TerrainZ), Settings, Heightmap, LandscapeBuilder);
		if (TribeManager)
		{
			SpawnedTribeManagers.Add(TribeManager);
			++SpawnedCount;
		}
	}

	// ── Spawn a shared QuestManager and assign it to every TribeManager ─────
	if (SpawnedCount > 0)
	{
		FActorSpawnParameters QMSpawnParams;
		QMSpawnParams.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AQuestManager* QuestManager = World->SpawnActor<AQuestManager>(
			AQuestManager::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, QMSpawnParams);

		if (QuestManager)
		{
			QuestManager->Tags.AddUnique(GMapGenTribeTag);
			SpawnedActors.Add(QuestManager);

			// Populate the quest resource pool from settings.
			QuestManager->PossibleResourceTypes = Settings->QuestPossibleResourceTypes;

			for (ATribeManager* TribeManager : SpawnedTribeManagers)
			{
				if (TribeManager)
				{
					TribeManager->QuestHandler->QuestManager = QuestManager;
				}
			}

			UE_LOG(LogTribeGenerator, Display,
				TEXT("GenerateTribes: QuestManager spawned and assigned to %d tribe(s) with %d possible resource type(s)."),
				SpawnedTribeManagers.Num(), QuestManager->PossibleResourceTypes.Num());
		}
		else
		{
			UE_LOG(LogTribeGenerator, Warning,
				TEXT("GenerateTribes: Failed to spawn QuestManager from class '%s'."),
				*AQuestManager::StaticClass()->GetName());
		}
	}

	UE_LOG(LogTribeGenerator, Display,
		TEXT("GenerateTribes: Spawned %d / %d tribe(s)."), SpawnedCount, Settings->TribeCount);
	return SpawnedCount > 0;
}

// static
TArray<FVector2D> UTribeGenerator::BuildLandCandidates(
	const UMapGeneratorSettings* Settings,
	const UHeightmapGenerator*   Heightmap)
{
	const FIntPoint Res      = Heightmap->GetResolution();
	const float     CellSize = static_cast<float>(Settings->QuadSize);

	// Identical to UResourceDistributor::CalculatePlacementAreas map origin formula
	const FVector2D MapOrigin(
		-(Res.X - 1) * CellSize * 0.5f,
		-(Res.Y - 1) * CellSize * 0.5f);

	// Edge margin: 10 % of the shorter axis, minimum 1 cell
	const int32 MarginCells = FMath::Max(1, FMath::Min(Res.X, Res.Y) / 10);

	// Subsample the heightmap so we get ~64x64 = ~4096 candidates regardless of resolution.
	// This keeps farthest-point sampling fast (O(candidates * tribeCount)).
	const int32 Step = FMath::Max(1, FMath::Min(Res.X, Res.Y) / 64);

	TArray<FVector2D> LandCandidates;
	LandCandidates.Reserve(4096);

	for (int32 Y = MarginCells; Y < Res.Y - MarginCells; Y += Step)
	{
		for (int32 X = MarginCells; X < Res.X - MarginCells; X += Step)
		{
			if (Heightmap->GetHeightAt(X, Y) > Settings->SeaLevel)
			{
				LandCandidates.Add(FVector2D(
					MapOrigin.X + X * CellSize,
					MapOrigin.Y + Y * CellSize));
			}
		}
	}

	return LandCandidates;
}

void UTribeGenerator::ClearTribes()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		SpawnedActors.Empty();
		SpawnedTribeManagers.Empty();
		return;
	}

	// Primary: destroy every actor tagged MapGen_tribe.
	// This works even if the tracking arrays are stale (e.g. after an editor restart).
	TArray<AActor*> TaggedActors;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->Tags.Contains(GMapGenTribeTag))
		{
			TaggedActors.Add(*It);
		}
	}

	auto DestroyActorSafe = [World](AActor* Actor) -> bool
	{
		if (!IsValid(Actor))
		{
			return false;
		}

#if WITH_EDITOR
		if (GEditor && World &&
			(World->WorldType == EWorldType::Editor || World->WorldType == EWorldType::EditorPreview))
		{
			return World->DestroyActor(Actor, /*bNetForce=*/true, /*bShouldModifyLevel=*/true);
		}
#endif

		return Actor->Destroy();
	};

	int32 Destroyed = 0;
	for (AActor* Actor : TaggedActors)
	{
		if (DestroyActorSafe(Actor))
		{
			++Destroyed;
		}
	}

	UE_LOG(LogTribeGenerator, Display,
		TEXT("ClearTribes: destroyed %d actor(s) with tag '%s'."), Destroyed, *GMapGenTribeTag.ToString());

	SpawnedActors.Empty();
	SpawnedTribeManagers.Empty();
}

ATribeManager* UTribeGenerator::SpawnTribe(
	UWorld*                      World,
	const FVector&               SpawnLocation,
	const UMapGeneratorSettings* Settings,
	const UHeightmapGenerator*   Heightmap,
	ULandscapeBuilder*           LandscapeBuilder)
{
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	// Outliner folder: "Generated Tribes/Tribe_0", "Generated Tribes/Tribe_1", ...
	// SpawnedTribeManagers hasn't been updated yet for this tribe, so .Num() == current index.
	const int32 TribeIndex = SpawnedTribeManagers.Num();
	const FName TribeFolderPath(*FString::Printf(TEXT("Tribes/Tribe_%d"), TribeIndex));

	ATribeManager* TribeManager = SpawnTribeManager(World, SpawnLocation, Settings, SpawnParams, TribeFolderPath);
	if (!TribeManager)
	{
		return nullptr;
	}

	// Store terrain references so TribeManager::Build() can use them at runtime.
	TribeManager->TerrainHeightmap        = Heightmap;
	TribeManager->TerrainSettings         = Settings;
	TribeManager->TerrainLandscapeBuilder = LandscapeBuilder;
	TribeManager->TribeFolderPath         = TribeFolderPath;

	// Set the TribeMan class so CreateTribeMan() knows which Blueprint to spawn.
	// Settings->TribeManClass is TSubclassOf<AActor>; TribeManager expects
	// TSubclassOf<ACharacter> — both wrap a UClass*, so the Get() cast is safe
	// here because GenerateTribes() already validated that it is a Character class.
	TribeManager->TribeManClass = Settings->TribeManClass.Get();

	AActor* TribeActor = SpawnTribeActor(World, SpawnLocation, Settings, SpawnParams, TribeIndex, TribeFolderPath);
	if (!TribeActor)
	{
		return nullptr;
	}

	// Store the tribe actor reference so TribeManager::Build() can pass it to SpawnBuilding().
	TribeManager->TribeActor = TribeActor;

	// Set the Manager property on TribeActor to point back to TribeManager.
	{
		static const FName ManagerPropName(TEXT("Manager"));
		FObjectProperty* ManagerProp =
			FindFProperty<FObjectProperty>(TribeActor->GetClass(), ManagerPropName);
		if (ManagerProp)
		{
			ManagerProp->SetObjectPropertyValue_InContainer(TribeActor, TribeManager);
		}
		else
		{
			UE_LOG(LogTribeGenerator, Warning,
				TEXT("SpawnTribe: BP_Tribe '%s' has no object property named 'Manager'. "
				     "Check the exact variable name in the Blueprint."),
				*TribeActor->GetClass()->GetName());
		}
	}

	SpawnStorage(World, TribeManager, SpawnLocation, Settings, SpawnParams, Heightmap, TribeActor, TribeFolderPath, LandscapeBuilder);

	TArray<ACharacter*> NewTribeMen = SpawnTribeMen(
		World, TribeManager, SpawnLocation,
		Settings->TribeManClass,
		Settings->TribeManCountPerTribe,
		Settings->TribeManSpawnRadius,
		TribeActor, TribeFolderPath,
		Heightmap, Settings);

	for (ACharacter* TribeMan : NewTribeMen)
	{
		TribeMan->Tags.AddUnique(GMapGenTribeTag);
		SpawnedActors.Add(TribeMan);
	}

	return TribeManager;
}

ATribeManager* UTribeGenerator::SpawnTribeManager(
	UWorld*                      World,
	const FVector&               Location,
	const UMapGeneratorSettings* Settings,
	const FActorSpawnParameters& SpawnParams,
	const FName&                 TribeFolderPath)
{
	// Use Blueprint subclass if provided, otherwise fall back to the base C++ class
	TSubclassOf<ATribeManager> ManagerClass = ATribeManager::StaticClass();
	if (Settings->TribeManagerClass &&
		Settings->TribeManagerClass->IsChildOf(ATribeManager::StaticClass()))
	{
		ManagerClass = Settings->TribeManagerClass;
	}

	ATribeManager* TribeManager = World->SpawnActor<ATribeManager>(
		ManagerClass,
		Location,
		FRotator::ZeroRotator,
		SpawnParams);

	if (!TribeManager)
	{
		UE_LOG(LogTribeGenerator, Warning,
			TEXT("SpawnTribeManager: Failed to spawn TribeManager at (%.0f, %.0f, %.0f)."),
			Location.X, Location.Y, Location.Z);
		return nullptr;
	}
	
	// Forward the resource base class from settings so the TribeManager can query nearby resources
	TribeManager->resourceBaseClass = Settings->TribeResourceBaseClass;

	// Forward the ItemPrices class so TribeManager::BeginPlay can create an instance
	// of BP_ItemPrices and call calculatePrices() on it.
	if (ItemPricesClass)
	{
		TribeManager->BuyingHandler->ItemPrices = ItemPricesClass;
	}
	else
	{
		UE_LOG(LogTribeGenerator, Warning,
			TEXT("SpawnTribeManager: ItemPricesClass is not set on UTribeGenerator. "
			     "TribeManager will not be able to look up item prices. "
			     "Assign BP_ItemPrices (Content/Tribes) to the ItemPricesClass property."));
	}

	TribeManager->Tags.AddUnique(GMapGenTribeTag);
#if WITH_EDITOR
	if (!TribeFolderPath.IsNone())
	{
		TribeManager->SetFolderPath(TribeFolderPath);
	}
#endif

	return TribeManager;
}

// Sets the Blueprint object-reference variable named "Tribe" on Actor to point at TribeActor
// (a BP_Tribe instance). BP_TribeMan and BP_Storage both expose a "Tribe" variable so they
// can reference their owning tribe.
// Sets the Blueprint FLinearColor variable named "TribeColor" on TribeActor.
// Hue is evenly distributed across the colour wheel based on TribeIndex / TribeCount.
// Logs a Warning if the property is not found so the name can be corrected easily.
void UTribeGenerator::SetTribeColor(AActor* TribeActor, int32 TribeIndex, int32 TribeCount)
{
	if (!TribeActor)
	{
		return;
	}

	// Evenly-spaced hues: tribe 0 = 0°, tribe 1 = 120° (for 3), etc.
	const float Hue        = (static_cast<float>(TribeIndex) / static_cast<float>(FMath::Max(1, TribeCount))) * 360.f;
	const FLinearColor HSV(Hue, 0.85f, 0.9f, 1.f);
	const FLinearColor RGB = HSV.HSVToLinearRGB();

	static const FName PropName(TEXT("TribeColor"));

	FStructProperty* ColorProp = FindFProperty<FStructProperty>(TribeActor->GetClass(), PropName);
	if (ColorProp && ColorProp->Struct == TBaseStructure<FLinearColor>::Get())
	{
		*ColorProp->ContainerPtrToValuePtr<FLinearColor>(TribeActor) = RGB;
		UE_LOG(LogTribeGenerator, Verbose,
			TEXT("SetTribeColor: Tribe_%d -> H=%.0f RGB=(%.2f, %.2f, %.2f)"),
			TribeIndex, Hue, RGB.R, RGB.G, RGB.B);
	}
	else
	{
		UE_LOG(LogTribeGenerator, Warning,
			TEXT("SetTribeColor: '%s' has no FLinearColor property named '%s'. Check the exact variable name in BP_Tribe."),
			*TribeActor->GetClass()->GetName(), *PropName.ToString());
	}
}

void UTribeGenerator::SetTribeOwner(AActor* Actor, AActor* TribeActor)
{
	if (!Actor || !TribeActor)
	{
		return;
	}

	FObjectProperty* TribeProp = FindFProperty<FObjectProperty>(Actor->GetClass(), TEXT("Tribe"));
	if (TribeProp)
	{
		TribeProp->SetObjectPropertyValue_InContainer(Actor, TribeActor);
	}
	else
	{
		UE_LOG(LogTribeGenerator, Verbose,
			TEXT("SetTribeOwner: Actor '%s' has no 'Tribe' object property -- skipping."),
			*Actor->GetName());
	}
}

AActor* UTribeGenerator::SpawnTribeActor(
	UWorld*                      World,
	const FVector&               SpawnLocation,
	const UMapGeneratorSettings* Settings,
	const FActorSpawnParameters& SpawnParams,
	int32                        TribeIndex,
	const FName&                 TribeFolderPath)
{
	if (!Settings || !Settings->TribeClass || !World)
	{
		if (Settings && !Settings->TribeClass)
		{
			UE_LOG(LogTribeGenerator, Warning,
				TEXT("SpawnTribeActor: TribeClass is not set in settings -- 'Tribe' property will remain null on TribeMan/Storage."));
		}
		return nullptr;
	}

	AActor* TribeActor = World->SpawnActor<AActor>(Settings->TribeClass, SpawnLocation,
		FRotator::ZeroRotator, SpawnParams);
	
	if (TribeActor)
	{
		TribeActor->Tags.AddUnique(GMapGenTribeTag);
#if WITH_EDITOR
		if (!TribeFolderPath.IsNone())
		{
			TribeActor->SetFolderPath(TribeFolderPath);
		}
#endif
		SetTribeColor(TribeActor, TribeIndex, Settings->TribeCount);
		SpawnedActors.Add(TribeActor);

		UFunction* InitFunc = TribeActor->FindFunction(TEXT("Initialize"));
		if (InitFunc)
		{
			TribeActor->ProcessEvent(InitFunc, nullptr);
		}
		else
		{
			UE_LOG(LogTribeGenerator, Warning,
				TEXT("SpawnTribeActor: 'Initialize' function not found on '%s'."),
				*TribeActor->GetClass()->GetName());
		}
	}
	else
	{
		UE_LOG(LogTribeGenerator, Warning,
			TEXT("SpawnTribeActor: Failed to spawn TribeActor (BP_Tribe) at (%.0f, %.0f, %.0f)."),
			SpawnLocation.X, SpawnLocation.Y, SpawnLocation.Z);
	}

	return TribeActor;
}

// Returns the lowest terrain Z found by sampling a 3x3 grid across the footprint.
// Every sample is a downward line trace (ALandscapeProxy-preferred, same logic as
// GetTerrainHeight). Falls back to CenterZ for samples that produce no hit.
static float FindLowestFootprintZ(
	UWorld*                      World,
	float                        CenterX,
	float                        CenterY,
	float                        CenterZ,
	float                        HalfExtent,
	const UHeightmapGenerator*   Heightmap,
	const UMapGeneratorSettings* Settings)
{
	static constexpr int32 GridSize = 3; // 3x3 = 9 samples

	float MinZ = CenterZ;

	for (int32 Row = 0; Row < GridSize; ++Row)
	{
		for (int32 Col = 0; Col < GridSize; ++Col)
		{
			// Map grid indices to [-HalfExtent, +HalfExtent] range
			const float T  = (GridSize > 1) ? static_cast<float>(Col) / (GridSize - 1) : 0.5f;
			const float U  = (GridSize > 1) ? static_cast<float>(Row) / (GridSize - 1) : 0.5f;
			const float SX = CenterX + FMath::Lerp(-HalfExtent, HalfExtent, T);
			const float SY = CenterY + FMath::Lerp(-HalfExtent, HalfExtent, U);

			float SampleZ = CenterZ;
			Heightmap->GetTerrainHeight(World, SX, SY, SampleZ, Settings);
			MinZ = FMath::Min(MinZ, SampleZ);
		}
	}

	return MinZ;
}

// static
AActor* UTribeGenerator::SpawnBuilding(
	UWorld*                      World,
	ATribeManager*               TribeManager,
	TSubclassOf<AActor>          BuildingClass,
	const FVector&               Location,
	const UHeightmapGenerator*   Heightmap,
	const UMapGeneratorSettings* Settings,
	ULandscapeBuilder*           LandscapeBuilder,
	AActor*                      TribeActor,
	const FName&                 FolderPath)
{
	if (!World || !BuildingClass)
	{
		UE_LOG(LogTribeGenerator, Warning, TEXT("SpawnBuilding: World or BuildingClass is null."));
		return nullptr;
	}

	// 1. Deferred spawn: the actor is created but BeginPlay has NOT fired yet.
	//    This lets us set the "Tribe" property before BeginPlay runs so that
	//    any Blueprint logic in BeginPlay (e.g. ApplyColorToIndicator) can
	//    safely read Tribe without getting None.
	const FTransform SpawnTransform(FRotator::ZeroRotator, Location);
	AActor* Building = World->SpawnActorDeferred<AActor>(
		BuildingClass,
		SpawnTransform,
		/*Owner=*/nullptr,
		/*Instigator=*/nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!Building)
	{
		UE_LOG(LogTribeGenerator, Warning,
			TEXT("SpawnBuilding: Failed to spawn '%s' at (%.0f, %.0f, %.0f)."),
			*BuildingClass->GetName(), Location.X, Location.Y, Location.Z);
		return nullptr;
	}

	// 2. Set the "Tribe" back-reference BEFORE BeginPlay fires.
	if (TribeActor)
	{
		SetTribeOwner(Building, TribeActor);
	}

	// 3. FinishSpawning triggers BeginPlay — Tribe is already valid at this point.
	Building->FinishSpawning(SpawnTransform);

	// 4. Terrain snapping: sample 3x3 footprint grid for the lowest Z.
	if (Heightmap && Settings)
	{
		FVector Origin, Extent;
		Building->GetActorBounds(/*bOnlyCollidingComponents=*/false, Origin, Extent);
		const float HalfExtent = FMath::Max(Extent.X, Extent.Y) * 1.2f;

		const float GroundZ = FindLowestFootprintZ(
			World, Location.X, Location.Y, Location.Z, HalfExtent, Heightmap, Settings);

		// 5. Flatten the landscape under the footprint.
		if (LandscapeBuilder)
		{
			LandscapeBuilder->FlattenArea(Location.X, Location.Y, HalfExtent, Settings);
		}

		// 6. Move to the correct ground Z.
		Building->SetActorLocation(FVector(Location.X, Location.Y, GroundZ));
	}

	// 7. Destroy every actor whose bounds overlap the building footprint,
	//    skipping the building itself and any ALandscapeProxy actors.
	{
		FVector Origin, Extent;
		Building->GetActorBounds(/*bOnlyCollidingComponents=*/false, Origin, Extent);

		// Tiny expansion so actors whose pivot sits exactly on the edge are caught.
		const FVector QueryExtent = Extent + FVector(5.f, 5.f, 5.f);

		const TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes = {
			UEngineTypes::ConvertToObjectType(ECC_WorldStatic),
			UEngineTypes::ConvertToObjectType(ECC_WorldDynamic),
		};
		const TArray<AActor*> IgnoreActors = { Building };

		TArray<AActor*> Overlapping;
		UKismetSystemLibrary::BoxOverlapActors(
			World, Origin, QueryExtent,
			ObjectTypes, /*FilterClass=*/nullptr, IgnoreActors, Overlapping);

		int32 DestroyedCount = 0;
		for (AActor* Actor : Overlapping)
		{
			if (!IsValid(Actor)) continue;
			if (Actor->IsA<ALandscapeProxy>()) continue;

			Actor->Destroy();
			++DestroyedCount;
		}

		if (DestroyedCount > 0)
		{
			UE_LOG(LogTribeGenerator, Verbose,
				TEXT("SpawnBuilding: Destroyed %d overlapping actor(s) under '%s'."),
				DestroyedCount, *Building->GetName());
		}
	}

	// 8. Register with the TribeManager.
	if (TribeManager)
	{
		TribeManager->storages.AddUnique(Building);
	}

#if WITH_EDITOR
	if (!FolderPath.IsNone())
	{
		Building->SetFolderPath(FolderPath);
	}
#endif

	return Building;
}

void UTribeGenerator::SpawnStorage(
	UWorld*                      World,
	ATribeManager*               TribeManager,
	const FVector&               CenterLocation,
	const UMapGeneratorSettings* Settings,
	const FActorSpawnParameters& SpawnParams,
	const UHeightmapGenerator*   Heightmap,
	AActor*                      TribeActor,
	const FName&                 TribeFolderPath,
	ULandscapeBuilder*           LandscapeBuilder)
{
	AActor* Storage = SpawnBuilding(
		World, TribeManager, Settings->StorageClass, CenterLocation,
		Heightmap, Settings, LandscapeBuilder, TribeActor, TribeFolderPath);

	if (Storage)
	{
		// Tag so ClearTribes() can find and destroy it.
		Storage->Tags.AddUnique(GMapGenTribeTag);
		SpawnedActors.Add(Storage);
	}
	else
	{
		UE_LOG(LogTribeGenerator, Warning,
			TEXT("SpawnStorage: SpawnBuilding failed at (%.0f, %.0f, %.0f)."),
			CenterLocation.X, CenterLocation.Y, CenterLocation.Z);
	}
}

// static
TArray<ACharacter*> UTribeGenerator::SpawnTribeMen(
	UWorld*                      World,
	ATribeManager*               TribeManager,
	const FVector&               CenterLocation,
	TSubclassOf<AActor>          TribeManClass,
	int32                        Count,
	float                        SpawnRadius,
	AActor*                      TribeActor,
	const FName&                 FolderPath,
	const UHeightmapGenerator*   Heightmap,
	const UMapGeneratorSettings* Settings)
{
	TArray<ACharacter*> Result;

	if (!World || !TribeManClass || Count <= 0)
	{
		return Result;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	for (int32 i = 0; i < Count; ++i)
	{
		// Evenly distribute around the circle; with Count=1 / SpawnRadius=0 this
		// collapses to exactly CenterLocation.
		const float Angle     = (Count > 1) ? (2.f * PI * i / Count) : 0.f;
		const float TribeManX = CenterLocation.X + FMath::Cos(Angle) * SpawnRadius;
		const float TribeManY = CenterLocation.Y + FMath::Sin(Angle) * SpawnRadius;

		// Terrain-snap Z when a heightmap is provided; otherwise use CenterLocation.Z.
		float TribeManZ = CenterLocation.Z;
		if (Heightmap && Settings)
		{
			Heightmap->GetTerrainHeight(World, TribeManX, TribeManY, TribeManZ, Settings);
		}

		const FVector SpawnLocation(TribeManX, TribeManY, TribeManZ + 100.f);

		AActor* TribeManActor = World->SpawnActor<AActor>(
			TribeManClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);

		if (!TribeManActor)
		{
			UE_LOG(LogTribeGenerator, Warning,
				TEXT("SpawnTribeMen: Failed to spawn TribeMan %d/%d."), i + 1, Count);
			continue;
		}

		ACharacter* TribeMan = Cast<ACharacter>(TribeManActor);
		if (!TribeMan)
		{
			UE_LOG(LogTribeGenerator, Warning,
				TEXT("SpawnTribeMen: Spawned actor %d/%d is not a Character -- skipped."),
				i + 1, Count);
			TribeManActor->Destroy();
			continue;
		}

		if (TribeManager)
		{
			TribeManager->tribeMen.AddUnique(TribeMan);
		}

		if (TribeActor)
		{
			SetTribeOwner(TribeMan, TribeActor);
		}

#if WITH_EDITOR
		if (!FolderPath.IsNone())
		{
			TribeMan->SetFolderPath(FolderPath);
		}
#endif

		UFunction* InitFunc = TribeManActor->FindFunction(TEXT("Initialize"));
		if (InitFunc)
		{
			TribeManActor->ProcessEvent(InitFunc, nullptr);
		}
		else
		{
			UE_LOG(LogTribeGenerator, Warning,
				TEXT("SpawnTribeActor: 'Initialize' function not found on '%s'."),
				*TribeManActor->GetClass()->GetName());
		}

		Result.Add(TribeMan);
	}

	return Result;
}

// static
TArray<FVector2D> UTribeGenerator::SelectTribePositions(
	const TArray<FVector2D>& Candidates,
	int32                    Count,
	FRandomStream&           Stream)
{
	TArray<FVector2D> Selected;
	if (Candidates.IsEmpty() || Count <= 0)
	{
		return Selected;
	}

	Count = FMath::Min(Count, Candidates.Num());
	Selected.Reserve(Count);

	// Seed the selection with a random candidate
	Selected.Add(Candidates[Stream.RandRange(0, Candidates.Num() - 1)]);

	// MinDistances[i] = squared distance from candidate i to the nearest already-selected point.
	// Updated incrementally so the algorithm runs in O(Candidates * Count)
	// instead of O(Candidates^2 * Count).
	TArray<float> MinDistances;
	MinDistances.SetNumUninitialized(Candidates.Num());
	for (int32 i = 0; i < Candidates.Num(); ++i)
	{
		MinDistances[i] = FVector2D::DistSquared(Candidates[i], Selected[0]);
	}

	while (Selected.Num() < Count)
	{
		// The candidate with the largest minimum distance becomes the next tribe location
		int32 BestIdx  = 0;
		float BestDist = MinDistances[0];
		for (int32 i = 1; i < Candidates.Num(); ++i)
		{
			if (MinDistances[i] > BestDist)
			{
				BestDist = MinDistances[i];
				BestIdx  = i;
			}
		}

		Selected.Add(Candidates[BestIdx]);

		// Update MinDistances based on the newly added point
		const FVector2D& NewPoint = Selected.Last();
		for (int32 i = 0; i < Candidates.Num(); ++i)
		{
			const float DistToNew = FVector2D::DistSquared(Candidates[i], NewPoint);
			if (DistToNew < MinDistances[i])
			{
				MinDistances[i] = DistToNew;
			}
		}
	}

	return Selected;
}
