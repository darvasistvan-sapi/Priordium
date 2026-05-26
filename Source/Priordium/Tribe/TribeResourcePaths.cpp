// Copyright Priordium. All Rights Reserved.

#include "TribeResourcePaths.h"

#include "TribeManager.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "AI/Navigation/NavQueryFilter.h"
#include "AIController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TribeTask.h"
#include "ResourceGatheringTask.h"

// ─────────────────────────────────────────────────────────────────────────────

UTribeResourcePaths::UTribeResourcePaths()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal: owner access
// ─────────────────────────────────────────────────────────────────────────────

ATribeManager* UTribeResourcePaths::GetOwnerManager() const
{
	return Cast<ATribeManager>(GetOwner());
}

// ─────────────────────────────────────────────────────────────────────────────
// Public entry points
// ─────────────────────────────────────────────────────────────────────────────

void UTribeResourcePaths::RefreshPaths()
{
	CalculateNearbyResources();

	UWorld* WorldContext = GetWorld();
	if (!WorldContext)
	{
		UE_LOG(LogTemp, Warning, TEXT("UTribeResourcePaths::RefreshPaths: WorldContext is null."));
		return;
	}

	ATribeManager* Owner = GetOwnerManager();
	if (!Owner || !Owner->resourceBaseClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UTribeResourcePaths::RefreshPaths: Owner or ResourceBaseClass is null."));
		return;
	}

	UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(WorldContext);
	if (!NavigationSystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("UTribeResourcePaths::RefreshPaths: NavigationSystem is null."));
		return;
	}

	const ANavigationData* NavigationData = NavigationSystem->GetDefaultNavDataInstance();
	if (!NavigationData)
	{
		UE_LOG(LogTemp, Warning, TEXT("UTribeResourcePaths::RefreshPaths: NavigationData is null."));
		return;
	}

	StorageResourcePathLengths.Empty();
	PendingPathQueries = 0;

	for (AActor* Storage : Owner->storages)
	{
		if (!Storage) continue;
		CalculateStorageResourcePathLengthsForStorage(
			TWeakObjectPtr<AActor>(Storage), WorldContext, NavigationSystem, NavigationData);
	}

	// If no async queries were launched (no reachable resources, projection
	// failures, etc.), invoke the completion step immediately.
	if (PendingPathQueries == 0)
	{
		CalculateStorageResourcePaths();
	}
}

void UTribeResourcePaths::CheckNullReferences()
{
	const bool bAnyRemoved =
		CheckNearbyResourcesInternal() ||
		CheckOccupiedResourcesInternal() ||
		CheckStorageResourcePathLengthsInternal();

	(void)bAnyRemoved; // reserved for future conditional logic
}

void UTribeResourcePaths::OrderTribeMenToCollect()
{
	ATribeManager* Owner = GetOwnerManager();
	if (!Owner) return;

	// Normal mode: iterate StorageResourcePaths in order (shortest path first).
	// Each entry is consumed at most once per tick via OccupiedResources.
	int32 PathIndex = 0;
	TObjectPtr<AActor> NearbyRaspBerry = GetFirstNearbyRaspBerry();

	for (ACharacter* TribeMan : Owner->tribeMen)
	{
		if (!TribeMan) continue;
		if (Owner->HasTribeManTask(TribeMan)) continue;

		if (NearbyResources.Num() > OccupiedResources.Num())
		{
			if (NearbyRaspBerry != nullptr)
			{
				CollectResource(TribeMan, NearbyRaspBerry.Get());
				NearbyRaspBerry = GetFirstNearbyRaspBerry();
			}
			else
			{
				// Find the next free resource (normal shortest-path order).
				while (PathIndex < StorageResourcePaths.Num())
				{
					const FStorageResourcePath& Candidate = StorageResourcePaths[PathIndex];
					++PathIndex;

					if (!Candidate.Resource.IsValid()) continue;
					if (OccupiedResources.Contains(Candidate.Resource)) continue;

					CollectResource(TribeMan, Candidate.Resource.Get());
					break;
				}

				// No more free resources — stop assigning.
				if (PathIndex >= StorageResourcePaths.Num()) break;
			}
		}
	}
}

void UTribeResourcePaths::CollectResource(AActor* TribeMan, AActor* Resource)
{
	if (!Resource)
	{
		UE_LOG(LogTemp, Warning, TEXT("UTribeResourcePaths::CollectResource: Resource is null."));
		return;
	}

	if (!TribeMan)
	{
		UE_LOG(LogTemp, Warning, TEXT("UTribeResourcePaths::CollectResource: TribeMan is null."));
		return;
	}

	ATribeManager* Owner = GetOwnerManager();
	if (!Owner) return;

	ACharacter* TribeManChar = Cast<ACharacter>(TribeMan);
	if (!TribeManChar) return;

	if (Owner->HasTribeManTask(TribeManChar))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UTribeResourcePaths::CollectResource: Attempted to assign a resource to a busy TribeMan '%s'."),
			*TribeMan->GetName());
		return;
	}

	if (OccupiedResources.Contains(TWeakObjectPtr<AActor>(Resource)))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UTribeResourcePaths::CollectResource: Attempted to assign an already occupied resource '%s'."),
			*Resource->GetName());
		return;
	}

	// If the TribeMan was pushed off the navmesh (e.g. by another TribeMan's capsule),
	// snap them back to the nearest navigable point before starting the task.
	// This prevents AIMoveTo from failing immediately due to an off-navmesh start position.
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation NavLoc;
		const FVector SearchExtent(500.f, 500.f, 250.f);
		if (NavSys->ProjectPointToNavigation(TribeMan->GetActorLocation(), NavLoc, SearchExtent))
		{
			const float OffNavmeshDistSq = FVector::DistSquared2D(TribeMan->GetActorLocation(), NavLoc.Location);
			// Only teleport if meaningfully off the navmesh (> 10 cm away horizontally)
			if (OffNavmeshDistSq > 100.f)
			{
				UE_LOG(LogTemp, Log,
					TEXT("UTribeResourcePaths::CollectResource: Snapping '%s' to navmesh (was %.1f cm off)."),
					*TribeMan->GetName(), FMath::Sqrt(OffNavmeshDistSq));
				TribeMan->SetActorLocation(NavLoc.Location, false, nullptr, ETeleportType::TeleportPhysics);
			}
		}
	}

	TSharedPtr<ResourceGatheringTask> ResourceTask =
		MakeShared<ResourceGatheringTask>(TribeManChar, Resource);
	ResourceTask->Execute();
	Owner->AssignTribeManTask(TribeManChar, ResourceTask);
	OccupiedResources.Add(TWeakObjectPtr<AActor>(Resource));
}

TObjectPtr<AActor> UTribeResourcePaths::GetFirstNearbyRaspBerry() const
{
	const ATribeManager* Owner = GetOwnerManager();
	if (!Owner) return nullptr;

	for (const TObjectPtr<AActor>& NearbyResource : NearbyResources)
	{
		if (!NearbyResource) continue;
		if (OccupiedResources.Contains(NearbyResource)) continue;

		const FByteProperty* TypeProp =
			FindFProperty<FByteProperty>(NearbyResource->GetClass(), Owner->ResourceTypePropertyName);
		if (!TypeProp || !TypeProp->Enum) continue;

		const uint8 EnumVal = TypeProp->GetPropertyValue_InContainer(NearbyResource.Get());
		const FName TypeName(TypeProp->Enum->GetAuthoredNameStringByValue(
			static_cast<int64>(EnumVal)));

		if (TypeName == Owner->RaspBerryResourceTypeName)
		{
			return NearbyResource;
		}
	}
	return nullptr;
}

void UTribeResourcePaths::RemoveOccupied(AActor* Resource)
{
	if (Resource)
	{
		OccupiedResources.Remove(TWeakObjectPtr<AActor>(Resource));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal: nearby resource census
// ─────────────────────────────────────────────────────────────────────────────

void UTribeResourcePaths::CalculateNearbyResources()
{
	NearbyResources.Empty();

	UWorld* WorldContext = GetWorld();
	if (!WorldContext) return;

	ATribeManager* Owner = GetOwnerManager();
	if (!Owner || !Owner->resourceBaseClass) return;

	static constexpr float SearchRadius = 5000.f;

	for (AActor* Storage : Owner->storages)
	{
		if (!Storage) continue;

		TArray<AActor*> OverlappingResources;
		TArray<AActor*> ActorsToIgnore;
		UKismetSystemLibrary::SphereOverlapActors(
			WorldContext,
			Storage->GetActorLocation(),
			SearchRadius,
			TArray<TEnumAsByte<EObjectTypeQuery>>(),
			Owner->resourceBaseClass,
			ActorsToIgnore,
			OverlappingResources);

		for (AActor* Resource : OverlappingResources)
		{
			NearbyResources.AddUnique(Resource);
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal: path-length calculation
// ─────────────────────────────────────────────────────────────────────────────

void UTribeResourcePaths::CalculateStorageResourcePaths()
{
	// Note: RefreshPaths() is NOT called here.
	// Path lengths are populated asynchronously; this function only reads
	// the results that the async callbacks have already written.
	StorageResourcePaths.Empty();

	for (const auto& StoragePair : StorageResourcePathLengths)
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
			StorageResourcePaths.Add(Entry);
		}
	}

	StorageResourcePaths.Sort([](const FStorageResourcePath& A, const FStorageResourcePath& B)
	{
		return A.PathLength < B.PathLength;
	});
}

void UTribeResourcePaths::CalculateStorageResourcePathLengthsForStorage(
	TWeakObjectPtr<AActor> Storage,
	UWorld* WorldContext,
	UNavigationSystemV1* NavigationSystem,
	const ANavigationData* NavigationData)
{
	if (!Storage.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UTribeResourcePaths: Encountered null storage actor in storages array."));
		return;
	}

	StorageResourcePathLengths.Add(Storage, TMap<TWeakObjectPtr<AActor>, float>());

	for (const TWeakObjectPtr<AActor>& Resource : NearbyResources)
	{
		CalculateStorageResourcePathLength(Storage, Resource, NavigationSystem, NavigationData);
	}
}

void UTribeResourcePaths::CalculateStorageResourcePathLength(
	TWeakObjectPtr<AActor> Storage,
	TWeakObjectPtr<AActor> Resource,
	UNavigationSystemV1* NavigationSystem,
	const ANavigationData* NavigationData)
{
	if (!Resource.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UTribeResourcePaths: Encountered null resource actor in NearbyResources."));
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
		UE_LOG(LogTemp, Warning,
			TEXT("UTribeResourcePaths: Storage '%s' @ (%.0f,%.0f,%.0f) could not be projected onto navmesh — skipping."),
			*Storage->GetName(),
			Storage->GetActorLocation().X, Storage->GetActorLocation().Y, Storage->GetActorLocation().Z);
		return;
	}

	FNavLocation ResourceNavLoc;
	if (!NavigationSystem->ProjectPointToNavigation(
		Resource->GetActorLocation(), ResourceNavLoc, QueryExtent))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UTribeResourcePaths: Resource '%s' @ (%.0f,%.0f,%.0f) could not be projected onto navmesh — skipping."),
			*Resource->GetName(),
			Resource->GetActorLocation().X, Resource->GetActorLocation().Y, Resource->GetActorLocation().Z);
		return;
	}

	FPathFindingQuery PathFindingQuery(
		this,
		*NavigationData,
		StorageNavLoc.Location,
		ResourceNavLoc.Location);

	// UE 5.6: FNavPathQueryDelegate passes the computed path as a third parameter.
	FNavPathQueryDelegate PathQueryDelegate = FNavPathQueryDelegate::CreateLambda(
		[this, Storage, Resource](uint32 QueryID, ENavigationQueryResult::Type Result, FNavPathSharedPtr Path)
		{
			// Always decrement first so the counter is accurate even on early-return paths.
			--PendingPathQueries;

			if (Result != ENavigationQueryResult::Success)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("UTribeResourcePaths: Path query failed. QueryID=%u Result=%s"),
					QueryID, *UEnum::GetValueAsString(Result));
			}
			else if (!Storage.IsValid())
			{
				UE_LOG(LogTemp, Warning,
					TEXT("UTribeResourcePaths: Storage is no longer valid. QueryID=%u"), QueryID);
			}
			else if (!Resource.IsValid())
			{
				UE_LOG(LogTemp, Warning,
					TEXT("UTribeResourcePaths: Resource is no longer valid. QueryID=%u"), QueryID);
			}
			else if (!Path.IsValid())
			{
				UE_LOG(LogTemp, Warning,
					TEXT("UTribeResourcePaths: Path is invalid in query callback. QueryID=%u"), QueryID);
			}
			else
			{
				TMap<TWeakObjectPtr<AActor>, float>* StorageResourcePathMap =
					StorageResourcePathLengths.Find(Storage);
				if (StorageResourcePathMap)
				{
					StorageResourcePathMap->Add(Resource, Path->GetLength());
				}
				else
				{
					UE_LOG(LogTemp, Warning,
						TEXT("UTribeResourcePaths: Could not find storage map entry for callback result. QueryID=%u"),
						QueryID);
				}
			}

			// All in-flight queries for this cycle have completed —
			// rebuild the sorted free-path list.
			if (PendingPathQueries == 0)
			{
				CalculateStorageResourcePaths();
			}
		});

	++PendingPathQueries;
	NavigationSystem->FindPathAsync(
		FNavAgentProperties::DefaultProperties, PathFindingQuery, PathQueryDelegate);
}

// ─────────────────────────────────────────────────────────────────────────────
// Stale-reference checks
// ─────────────────────────────────────────────────────────────────────────────

bool UTribeResourcePaths::CheckNearbyResourcesInternal()
{
	bool bRemoved = false;
	for (int32 i = NearbyResources.Num() - 1; i >= 0; --i)
	{
		if (!NearbyResources[i] || !IsValid(NearbyResources[i]))
		{
			NearbyResources.RemoveAt(i);
			bRemoved = true;
		}
	}
	return bRemoved;
}

bool UTribeResourcePaths::CheckOccupiedResourcesInternal()
{
	bool bRemoved = false;
	for (auto It = OccupiedResources.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
			bRemoved = true;
		}
	}
	return bRemoved;
}

bool UTribeResourcePaths::CheckStorageResourcePathLengthsInternal()
{
	bool bRemoved = false;
	for (auto StorageIt = StorageResourcePathLengths.CreateIterator(); StorageIt; ++StorageIt)
	{
		if (!StorageIt->Key.IsValid())
		{
			StorageIt.RemoveCurrent();
			bRemoved = true;
			continue;
		}
		for (auto PathIt = StorageIt->Value.CreateIterator(); PathIt; ++PathIt)
		{
			if (!PathIt->Key.IsValid())
			{
				PathIt.RemoveCurrent();
				bRemoved = true;
			}
		}
		if (StorageIt->Value.Num() == 0)
		{
			StorageIt.RemoveCurrent();
			bRemoved = true;
		}
	}
	return bRemoved;
}
