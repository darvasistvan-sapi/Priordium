// Copyright Priordium. All Rights Reserved.

#include "TribeManager.h"

#include "UTribeGenerator.h"

#include "UObject/UnrealType.h"

#include "GameFramework/Character.h"
#include "Engine/EngineTypes.h"
#include "LandscapeProxy.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "NavigationSystem.h"
#include "AI/Navigation/NavQueryFilter.h"
#include "TribeTask.h"
#include "ResourceGatheringTask.h"
#include "UMapGeneratorSettings.h"

ATribeManager::ATribeManager()
{
    // Ignore updating on every frame, this actor only works when calculateStorageResourcePathLengths() is called.
    PrimaryActorTick.bCanEverTick = false;
}

void ATribeManager::BeginPlay()
{
    Super::BeginPlay();

    // Kick off the initial async path queries.
    // calculateStorageResourcePaths() will be called automatically once
    // all callbacks have fired (via the PendingPathQueries counter).
    calculateStorageResourcePathLengths();

    GetWorldTimerManager().SetTimer(
        ManageTimerHandle,
        this,
        &ATribeManager::Manage,
        2.f,
        /*bLoop=*/true
    );
}

// ─────────────────────────────────────────────────────────────────────────────
// Storage map accessor
// Resolves the FMapProperty (key = FByteProperty+UEnum, value = FIntProperty)
// named StorageInventoryPropertyName on a storage actor.
// ─────────────────────────────────────────────────────────────────────────────
namespace
{
	struct FStorageMapAccessor
	{
		FMapProperty*        MapProp  = nullptr;
		const FByteProperty* KeyByte  = nullptr;
		const UEnum*         KeyEnum  = nullptr;
		const FIntProperty*  ValInt   = nullptr;

		bool IsValid() const { return MapProp && KeyByte && KeyEnum && ValInt; }

		static FStorageMapAccessor Resolve(AActor* Actor, FName PropName)
		{
			FStorageMapAccessor Out;
			Out.MapProp = FindFProperty<FMapProperty>(Actor->GetClass(), PropName);
			if (!Out.MapProp) return Out;
			Out.KeyByte = CastField<FByteProperty>(Out.MapProp->KeyProp);
			Out.ValInt  = CastField<FIntProperty>(Out.MapProp->ValueProp);
			if (Out.KeyByte) Out.KeyEnum = Out.KeyByte->Enum;
			return Out;
		}
	};
} // namespace

// ─────────────────────────────────────────────────────────────────────────────

int32 ATribeManager::GetResourceAmount(FName ResourceType) const
{
	int32 Total = 0;

	for (const TObjectPtr<AActor>& StoragePtr : storages)
	{
		AActor* Storage = StoragePtr.Get();
		if (!Storage) continue;

		const FStorageMapAccessor Acc = FStorageMapAccessor::Resolve(Storage, StorageInventoryPropertyName);
		if (!Acc.IsValid()) continue;

		FScriptMapHelper MapHelper(Acc.MapProp, Acc.MapProp->ContainerPtrToValuePtr<void>(Storage));
		for (FScriptMapHelper::FIterator Iter(MapHelper); Iter; ++Iter)
		{
			const uint8 EnumVal = Acc.KeyByte->GetPropertyValue(MapHelper.GetKeyPtr(Iter.GetInternalIndex()));
			const FName Key(Acc.KeyEnum->GetAuthoredNameStringByValue(static_cast<int64>(EnumVal)));
			if (Key == ResourceType)
			{
				Total += Acc.ValInt->GetPropertyValue(MapHelper.GetValuePtr(Iter.GetInternalIndex()));
			}
		}
	}

	return Total;
}

bool ATribeManager::DeductResourceAmount(FName ResourceType, int32 Amount)
{
	int32 Remaining = Amount;

	for (const TObjectPtr<AActor>& StoragePtr : storages)
	{
		if (Remaining <= 0) break;

		AActor* Storage = StoragePtr.Get();
		if (!Storage) continue;

		const FStorageMapAccessor Acc = FStorageMapAccessor::Resolve(Storage, StorageInventoryPropertyName);
		if (!Acc.IsValid()) continue;

		FScriptMapHelper MapHelper(Acc.MapProp, Acc.MapProp->ContainerPtrToValuePtr<void>(Storage));
		for (FScriptMapHelper::FIterator Iter(MapHelper); Iter; ++Iter)
		{
			const uint8 EnumVal = Acc.KeyByte->GetPropertyValue(MapHelper.GetKeyPtr(Iter.GetInternalIndex()));
			const FName Key(Acc.KeyEnum->GetAuthoredNameStringByValue(static_cast<int64>(EnumVal)));
			if (Key != ResourceType) continue;

			void* ValuePtr = MapHelper.GetValuePtr(Iter.GetInternalIndex());
			const int32 Current = Acc.ValInt->GetPropertyValue(ValuePtr);
			const int32 Deduct  = FMath::Min(Current, Remaining);
			Acc.ValInt->SetPropertyValue(ValuePtr, Current - Deduct);
			Remaining -= Deduct;
			break;
		}
	}

	return Remaining == 0;
}

AActor* ATribeManager::Build(TSubclassOf<AActor> BuildingClass, FVector Location)
{
	static constexpr int32 WoodCost = 50;

	const int32 Available = GetResourceAmount(WoodResourceTypeName);
	if (Available < WoodCost)
	{
		UE_LOG(LogTemp, Error,
			TEXT("ATribeManager::Build: Not enough '%s' to construct. Required: %d, Available: %d."),
			*WoodResourceTypeName.ToString(), WoodCost, Available);
		return nullptr;
	}

	DeductResourceAmount(WoodResourceTypeName, WoodCost);

	return UTribeGenerator::SpawnBuilding(
		GetWorld(),
		this,
		BuildingClass,
		Location,
		TerrainHeightmap.Get(),
		TerrainSettings.Get(),
		TerrainLandscapeBuilder.Get(),
		TribeActor.Get(),
		TribeFolderPath);
}

void ATribeManager::Manage()
{
	CheckNullReferences();

	// Assign tasks to TribeMen that have no task yet.
	orderTribeMenToCollect();

	// Re-execute tasks for TribeMen that have a task but stopped moving.
	resumeIdleTribeManTasks();

	// Build a new storage if we have enough wood and a free spot nearby.
	if (GetResourceAmount(WoodResourceTypeName) >= 50)
	{
		TryBuildStorage();
	}
}

void ATribeManager::TryBuildStorage()
{
	// Determine the class to build from an existing storage.
	AActor* ReferenceStorage = nullptr;
	for (const TObjectPtr<AActor>& S : storages)
	{
		if (S.Get()) { ReferenceStorage = S.Get(); break; }
	}
	if (!ReferenceStorage) return;

	// The Build() call itself checks and deducts the 300-wood cost.
	FVector BuildLocation;
	if (!FindFreeBuildLocation(BuildLocation)) return;

	Build(ReferenceStorage->GetClass(), BuildLocation);
}

bool ATribeManager::FindFreeBuildLocation(FVector& OutLocation) const
{
	UWorld* World = GetWorld();
	if (!World) return false;

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys) return false;

	// Collect existing storage positions as search centres.
	TArray<FVector> Centers;
	for (const TObjectPtr<AActor>& S : storages)
	{
		if (S) Centers.Add(S->GetActorLocation());
	}
	if (Centers.IsEmpty()) return false;

	static constexpr int32 MaxAttempts = 30;
	static constexpr float MaxRadius   =  10000.f;  // 1 km in cm
	static constexpr float MinRadius   =   2000.f;  // min 20 m from the centre storage
	static constexpr float ClearRadius =    500.f;  // must be free within 5 m

	// Object types to overlap-test against: static scenery, dynamic actors, pawns.
	const TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes =
	{
		UEngineTypes::ConvertToObjectType(ECC_WorldStatic),
		UEngineTypes::ConvertToObjectType(ECC_WorldDynamic),
		UEngineTypes::ConvertToObjectType(ECC_Pawn),
	};

	// Ignore this manager actor in overlap queries.
	const TArray<AActor*> IgnoreActors = { const_cast<ATribeManager*>(this) };

	const FVector NavExtent(500.f, 500.f, 5000.f);

	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		// Pick a random storage as the centre.
		const FVector Center = Centers[FMath::RandRange(0, Centers.Num() - 1)];

		// Random point in an annulus [MinRadius, MaxRadius] around the centre.
		const float Angle = FMath::FRandRange(0.f, 2.f * PI);
		const float Dist  = FMath::FRandRange(MinRadius, MaxRadius);
		const FVector Candidate(
			Center.X + FMath::Cos(Angle) * Dist,
			Center.Y + FMath::Sin(Angle) * Dist,
			Center.Z + 5000.f);   // start high so the navmesh snap goes downward

		// Must land on the navigation mesh.
		FNavLocation NavLoc;
		if (!NavSys->ProjectPointToNavigation(Candidate, NavLoc, NavExtent)) {
			continue;
		}

		// Raise the overlap sphere 150 cm above the surface to reduce false hits,
		// then explicitly strip the landscape before evaluating the result.
		TArray<AActor*> HitActors;
		const FVector CheckPos = NavLoc.Location + FVector(0.f, 0.f, 150.f);
		UKismetSystemLibrary::SphereOverlapActors(
			World, CheckPos, ClearRadius,
			ObjectTypes, nullptr, IgnoreActors, HitActors);

		// The landscape is always present under every valid navmesh point — ignore it.
		HitActors.RemoveAll([](const AActor* A)
		{
			return !A || A->IsA<ALandscapeProxy>();
		});

		if (!HitActors.IsEmpty()) continue;

		OutLocation = NavLoc.Location;
		return true;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("ATribeManager::FindFreeBuildLocation: No free build spot found after %d attempts."),
		MaxAttempts);
	return false;
}

void ATribeManager::orderTribeMenToCollect()
{
	// Iterate storageResourcePaths in order (shortest path first).
	// Each entry is consumed at most once per tick: once a resource is
	// assigned it is added to OccupiedResources and skipped by subsequent
	// TribeMen in the same loop.
	int32 PathIndex = 0;

	for (ACharacter* TribeMan : tribeMen)
	{
		if (!TribeMan) continue;
		if (tribeManTasks.Contains(TribeMan)) continue;

		// Find the next free (not yet occupied this tick) resource.
		while (PathIndex < storageResourcePaths.Num())
		{
			const FStorageResourcePath& Candidate = storageResourcePaths[PathIndex];
			++PathIndex;

			if (!Candidate.Resource.IsValid()) continue;
			if (OccupiedResources.Contains(Candidate.Resource)) continue;

			collectResource(TribeMan, Candidate.Resource.Get());
			break;
		}

		// No more free resources — stop assigning.
		if (PathIndex >= storageResourcePaths.Num()) break;
	}
}

void ATribeManager::resumeIdleTribeManTasks()
{
	for (ACharacter* TribeMan : tribeMen)
	{
		if (!TribeMan) continue;

		TSharedPtr<TribeTask>* Task = tribeManTasks.Find(TribeMan);
		if (!Task || !Task->IsValid()) continue;

		// Read the Blueprint bool variable "IsWorking" via reflection.
		// If the property doesn't exist on this class, default to false so
		// the velocity check alone decides whether to resume.
		bool bIsWorking = false;
		if (const FBoolProperty* IsWorkingProp =
			FindFProperty<FBoolProperty>(TribeMan->GetClass(), TEXT("IsWorking")))
		{
			bIsWorking = IsWorkingProp->GetPropertyValue_InContainer(TribeMan);
		}

		// Consider the TribeMan idle if they are not working and their
		// movement component reports a velocity of (nearly) zero.
		const UCharacterMovementComponent* Movement = TribeMan->GetCharacterMovement();
		if (!Movement) continue;

		if (!bIsWorking && Movement->Velocity.IsNearlyZero())
		{
			(*Task)->Execute();
		}
	}
}

void ATribeManager::calculateStorageResourcePaths()
{
	// Note: calculateStorageResourcePathLengths() is NOT called here.
	// Path lengths are populated asynchronously; this function only reads
	// the results that the async callbacks have already written.
	storageResourcePaths.Empty();

	for (const auto& StoragePair : storageResourcePathLengths)
	{
		if (!StoragePair.Key.IsValid()) continue;

		for (const auto& ResourcePair : StoragePair.Value)
		{
			if (!ResourcePair.Key.IsValid()) continue;
			if (OccupiedResources.Contains(ResourcePair.Key)) continue;

			FStorageResourcePath Entry;
			Entry.Storage    = StoragePair.Key;
			Entry.Resource   = ResourcePair.Key;
			Entry.PathLength = ResourcePair.Value;
			storageResourcePaths.Add(Entry);
		}
	}

	storageResourcePaths.Sort([](const FStorageResourcePath& A, const FStorageResourcePath& B)
	{
		return A.PathLength < B.PathLength;
	});
}

void ATribeManager::calculateStorageResourcePathLengths()
{
	calculateNearbyResources();

    UWorld* WorldContext = GetWorld();
    if (!WorldContext)
    {
        UE_LOG(LogTemp, Warning, TEXT("TribeManager: WorldContext is null."));
        return;
    }

    if (!resourceBaseClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("TribeManager: ResourceBaseClass is null."));
        return;
    }

    UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(WorldContext);
    if (!NavigationSystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("TribeManager: NavigationSystem is null."));
        return;
    }

    const ANavigationData* NavigationData = NavigationSystem->GetDefaultNavDataInstance();
    if (!NavigationData)
    {
        UE_LOG(LogTemp, Warning, TEXT("TribeManager: NavigationData is null."));
        return;
    }

    storageResourcePathLengths.Empty();
    PendingPathQueries = 0;

    for (AActor* Storage : storages)
    {
		if (!Storage) continue;
		CalculateStorageResourcePathLengths(TWeakObjectPtr<AActor>(Storage), WorldContext, NavigationSystem, NavigationData);
    }

    // If no async queries were launched (no reachable resources, projection
    // failures, etc.), invoke the completion step immediately.
    if (PendingPathQueries == 0)
    {
        calculateStorageResourcePaths();
    }
}

void ATribeManager::CalculateStorageResourcePathLengths(TWeakObjectPtr<AActor> Storage, UWorld* WorldContext, UNavigationSystemV1* NavigationSystem, const ANavigationData* NavigationData)
{
	if (!Storage.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("TribeManager: Encountered null storage actor in storages array."));
		return;
	}

    // 2 km - in UE, 1 unit = 1 cm, so 2 km = 200,000 cm
	static constexpr float SearchRadius = 200000.f;

	// Collect resources that fall within 2 km of the current storage

	storageResourcePathLengths.Add(Storage, TMap<TWeakObjectPtr<AActor>, float>());

	for (const TWeakObjectPtr<AActor>& Resource : nearbyResources)
	{
		CalculateStorageResourcePathLength(Storage, Resource, NavigationSystem, NavigationData);
	}
}

void ATribeManager::CalculateStorageResourcePathLength(TWeakObjectPtr<AActor> Storage, TWeakObjectPtr<AActor> Resource, UNavigationSystemV1* NavigationSystem, const ANavigationData* NavigationData)
{
	if (!Resource.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("TribeManager: Encountered null resource actor in NearbyResources."));
		return;
	}

	// Project both endpoints onto the navmesh before querying.
	// Without this, actors whose pivot isn't exactly on the navmesh surface
	// (buildings, resources on slopes, etc.) will always produce a Fail result.
	// Z extent is large to handle steep terrain where the navmesh surface can be
	// many meters away from the actor pivot vertically.
	const FVector QueryExtent(500.f, 500.f, 5000.f);

	FNavLocation StorageNavLoc;
	if (!NavigationSystem->ProjectPointToNavigation(
		Storage->GetActorLocation(), StorageNavLoc, QueryExtent))
	{
		UE_LOG(LogTemp, Warning, TEXT("TribeManager: Storage '%s' @ (%.0f,%.0f,%.0f) could not be projected onto navmesh -- skipping."),
			*Storage->GetName(),
			Storage->GetActorLocation().X, Storage->GetActorLocation().Y, Storage->GetActorLocation().Z);
		return;
	}

	FNavLocation ResourceNavLoc;
	if (!NavigationSystem->ProjectPointToNavigation(
		Resource->GetActorLocation(), ResourceNavLoc, QueryExtent))
	{
		UE_LOG(LogTemp, Warning, TEXT("TribeManager: Resource '%s' @ (%.0f,%.0f,%.0f) could not be projected onto navmesh -- skipping."),
			*Resource->GetName(),
			Resource->GetActorLocation().X, Resource->GetActorLocation().Y, Resource->GetActorLocation().Z);
		return;
	}

	FPathFindingQuery PathFindingQuery(
		this,
		*NavigationData,
		StorageNavLoc.Location,
		ResourceNavLoc.Location
	);

	// UE 5.6: FNavPathQueryDelegate passes the computed path as a third parameter
	FNavPathQueryDelegate PathQueryDelegate = FNavPathQueryDelegate::CreateLambda(
		[this, Storage, Resource](uint32 QueryID, ENavigationQueryResult::Type Result, FNavPathSharedPtr Path)
		{
			// Always decrement first so the counter is accurate even on early-return paths.
			--PendingPathQueries;

			if (Result != ENavigationQueryResult::Success)
			{
				UE_LOG(LogTemp, Warning, TEXT("TribeManager: Path query failed. QueryID=%u Result=%s"), QueryID, *UEnum::GetValueAsString(Result));
			}
			else if (!Storage.IsValid())
			{
				UE_LOG(LogTemp, Warning, TEXT("TribeManager: Storage is no longer valid. QueryID=%u"), QueryID);
			}
			else if (!Resource.IsValid())
			{
				UE_LOG(LogTemp, Warning, TEXT("TribeManager: Resource is no longer valid. QueryID=%u"), QueryID);
			}
			else if (!Path.IsValid())
			{
				UE_LOG(LogTemp, Warning, TEXT("TribeManager: Path is invalid in query callback. QueryID=%u"), QueryID);
			}
			else
			{
				TMap<TWeakObjectPtr<AActor>, float>* StorageResourcePathMap = storageResourcePathLengths.Find(Storage);
				if (StorageResourcePathMap)
				{
					StorageResourcePathMap->Add(Resource, Path->GetLength());
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("TribeManager: Could not find storage map entry for callback result. QueryID=%u"), QueryID);
				}
			}

			// All in-flight queries for this cycle have completed —
			// rebuild the sorted free-path list.
			if (PendingPathQueries == 0)
			{
				calculateStorageResourcePaths();
				Manage();
			}
		}
	);

	++PendingPathQueries;
	NavigationSystem->FindPathAsync(FNavAgentProperties::DefaultProperties, PathFindingQuery, PathQueryDelegate);
}

void ATribeManager::CheckNullReferences()
{
	bool anyItemDeleted = checkNearbyResources() || checkTribeMen() || checkStorages();

	if (anyItemDeleted)
	{
		checkOccupiedResources();
		checkTribeManTasks();
		checkStorageResourcePathLengths();
	}
}

bool ATribeManager::checkNearbyResources()
{
	bool resourceRemoved = false;
	for (int32 i = nearbyResources.Num() - 1; i >= 0; --i)
	{
		if (!nearbyResources[i] || !IsValid(nearbyResources[i]))
		{
			nearbyResources.RemoveAt(i);
			resourceRemoved = true;
		}
	}
	return resourceRemoved;
}

bool ATribeManager::checkOccupiedResources()
{
	bool resourceRemoved = false;
	for (auto occupiedResource = OccupiedResources.CreateIterator(); occupiedResource; ++occupiedResource)
	{
		if (!occupiedResource->IsValid())
		{
			occupiedResource.RemoveCurrent();
			resourceRemoved = true;
		}
	}
	return resourceRemoved;
}

bool ATribeManager::checkTribeMen()
{
	bool tribeMenRemoved = false;
	for (int32 i = tribeMen.Num() - 1; i >= 0; --i)
	{
		if (!tribeMen[i] || !IsValid(tribeMen[i]))
		{
			tribeMen.RemoveAt(i);
			tribeMenRemoved = true;
		}
	}
	return tribeMenRemoved;
}

bool ATribeManager::checkStorages()
{
	bool storagesRemoved = false;
	for (int32 i = storages.Num() - 1; i >= 0; --i)
	{
		if (!storages[i] || !IsValid(storages[i]))
		{
			storages.RemoveAt(i);
			storagesRemoved = true;
		}
	}
	return storagesRemoved;
}

bool ATribeManager::checkTribeManTasks()
{
	bool tasksRemoved = false;
	for (auto tribeManTask = tribeManTasks.CreateIterator(); tribeManTask; ++tribeManTask)
	{
		if (!tribeManTask->Key || !IsValid(tribeManTask->Key.Get()) ||
			!tribeManTask->Value.IsValid())
		{
			tribeManTask.RemoveCurrent();
			tasksRemoved = true;
		}
	}
	return tasksRemoved;
}

bool ATribeManager::checkStorageResourcePathLengths()
{
	bool pathLengthsRemoved = false;
	for (auto storageIt = storageResourcePathLengths.CreateIterator(); storageIt; ++storageIt)
	{
		if (!storageIt->Key.IsValid())
		{
			storageIt.RemoveCurrent();
			pathLengthsRemoved = true;
			continue;
		}
		for (auto storagePath = storageIt->Value.CreateIterator(); storagePath; ++storagePath)
		{
			if (!storagePath->Key.IsValid())
			{
				storagePath.RemoveCurrent();
				pathLengthsRemoved = true;
			}
		}
		if (storageIt->Value.Num() == 0)
		{
			storageIt.RemoveCurrent();
			pathLengthsRemoved = true;
		}
	}
	return pathLengthsRemoved;
}

void ATribeManager::calculateNearbyResources()
{
	nearbyResources.Empty();

	UWorld* WorldContext = GetWorld();
	if (!WorldContext) return;

	static constexpr float SearchRadius = 10000.f;

    for (AActor* Storage : storages)
    {
		if (!Storage) continue;

		TArray<AActor*> OverlappingResources;
		TArray<AActor*> ActorsToIgnore;
		UKismetSystemLibrary::SphereOverlapActors(
			WorldContext,
			Storage->GetActorLocation(),
			SearchRadius,
			TArray<TEnumAsByte<EObjectTypeQuery>>(),
			resourceBaseClass,
			ActorsToIgnore,
			OverlappingResources
		);
		for (AActor* Resource : OverlappingResources)
		{
			nearbyResources.AddUnique(Resource);
		}
	}
}

ACharacter* ATribeManager::getFreeTribeMan() const
{
	for (ACharacter* TribeMan : tribeMen)
	{
		if (!TribeMan) continue;

		if (!tribeManTasks.Contains(TribeMan))
		{
			return TribeMan;
		}
	}

	return nullptr;
}

void ATribeManager::collectResource(AActor* TribeMan, AActor* Resource)
{
	if (!Resource)
	{
		UE_LOG(LogTemp, Warning, TEXT("TribeManager: Attempted to collect a null resource."));
		return;
	}

	if (!TribeMan)
	{
		UE_LOG(LogTemp, Warning, TEXT("TribeManager: No free tribeman available for resource collection."));
		return;
	}

	TSharedPtr<ResourceGatheringTask> resourceTask = MakeShared<ResourceGatheringTask>(Cast<ACharacter>(TribeMan), Resource);
	resourceTask->Execute();
	tribeManTasks.Add(Cast<ACharacter>(TribeMan), resourceTask);
	OccupiedResources.Add(TWeakObjectPtr<AActor>(Resource));
}
