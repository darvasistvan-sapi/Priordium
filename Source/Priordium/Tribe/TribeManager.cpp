// Copyright Priordium. All Rights Reserved.

#include "TribeManager.h"

#include "FStorageMapAccessor.h"
#include "TribeQuestManager.h"
#include "TribeBuyingManager.h"
#include "TribeResourcePaths.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "AIController.h"
#include "TribeTask.h"
#include "ResourceGatheringTask.h"


// Static member definition — one list shared across all instances.
TArray<TWeakObjectPtr<ATribeManager>> ATribeManager::AllTribeManagers;

// ─────────────────────────────────────────────────────────────────────────────

ATribeManager::ATribeManager()
{
	// Ignore updating on every frame, this actor only works when ResourcePaths->RefreshPaths() is called.
	PrimaryActorTick.bCanEverTick = false;

	QuestHandler   = CreateDefaultSubobject<UTribeQuestManager>(TEXT("QuestHandler"));
	BuyingHandler  = CreateDefaultSubobject<UTribeBuyingManager>(TEXT("BuyingHandler"));
	ResourcePaths  = CreateDefaultSubobject<UTribeResourcePaths>(TEXT("ResourcePaths"));
}

void ATribeManager::BeginPlay()
{
    Super::BeginPlay();

    // Register this instance so other systems can reach every active tribe.
    AllTribeManagers.Add(this);

    // Kick off the initial async path queries.
    // CalculateStorageResourcePaths() will be called automatically once
    // all callbacks have fired (via the PendingPathQueries counter).
    if (ResourcePaths)
    {
        ResourcePaths->RefreshPaths();
    }

    GetWorldTimerManager().SetTimer(
        ManageTimerHandle,
        this,
        &ATribeManager::Manage,
        3.f,
        /*bLoop=*/true
    );
}

void ATribeManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Remove this instance and any stale (already-destroyed) entries in one pass.
	AllTribeManagers.RemoveAll([this](const TWeakObjectPtr<ATribeManager>& Ptr)
	{
		return Ptr == this || !Ptr.IsValid();
	});

	Super::EndPlay(EndPlayReason);
}

void ATribeManager::Manage()
{
	CheckNullReferences();

	// Resume stale/interrupted tasks BEFORE assigning new ones.
	// This prevents double-Execute(): OrderTribeMenToCollect calls Execute() when
	// assigning a task, and if resumeIdleTribeManTasks ran afterwards it would see
	// velocity=0 (physics not yet updated) and call Execute() a second time on the
	// same tick, breaking the Blueprint state machine.
	resumeIdleTribeManTasks();

	// Assign tasks to TribeMen that have no task yet.
	if (ResourcePaths)
	{
		ResourcePaths->OrderTribeMenToCollect();
	}

	if (BuyingHandler && ResourcePaths &&
		0.9 * ResourcePaths->NearbyResourceCount() <= ResourcePaths->OccupiedResourceCount())
	{
		BuyingHandler->TryBuildStorage();
	}

	if (BuyingHandler && ResourcePaths &&
		tribeMen.Num() < 50 &&
		ResourcePaths->OccupiedResourceCount() < ResourcePaths->NearbyResourceCount())
	{
		BuyingHandler->CreateTribeMan();
	}

	if (QuestHandler)
	{
		QuestHandler->CheckQuests();
	}
}

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

// ─────────────────────────────────────────────────────────────────────────────
// Resource census near an arbitrary world location
// ─────────────────────────────────────────────────────────────────────────────

TMap<FName, int32> ATribeManager::CountResourcesNearLocation(FVector Location) const
{
	TMap<FName, int32> Result;

	UWorld* World = GetWorld();
	if (!World || !resourceBaseClass)
	{
		return Result;
	}

	// 100 m in Unreal units (1 UU = 1 cm)
	static constexpr float SearchRadiusSq          = 5000.f * 5000.f;
	static constexpr float StorageExclusionRadiusSq = 10000.f * 10000.f;

	TArray<FVector> WorldStorageLocations = GetAllWorldStorageLocations();

	// Snapshot storage positions once so the inner loop stays cache-friendly.
	TArray<FVector> StorageLocations;
	StorageLocations.Reserve(storages.Num());
	for (const TObjectPtr<AActor>& StoragePtr : storages)
	{
		if (const AActor* Storage = StoragePtr.Get())
		{
			StorageLocations.Add(Storage->GetActorLocation());
		}
	}

	// Collect all BP_Resource instances in the world.
	TArray<AActor*> AllResources;
	UGameplayStatics::GetAllActorsOfClass(World, resourceBaseClass, AllResources);

	for (const AActor* Resource : AllResources)
	{
		if (!Resource) continue;

		const FVector ResourceLoc = Resource->GetActorLocation();
		const float DistSq = FVector::DistSquared(ResourceLoc, Location);

		// ── 1. Must be within search radius of Location ───────────────────────
		if (DistSq > SearchRadiusSq)
		{
			continue;
		}

		// ── 2. Must NOT be within 100 m of any storage ────────────────────────
		bool bNearStorage = false;
		for (const FVector& StorageLoc : WorldStorageLocations)
		{
			if (FVector::DistSquared(ResourceLoc, StorageLoc) < StorageExclusionRadiusSq)
			{
				bNearStorage = true;
				break;
			}
		}
		if (bNearStorage) continue;

		// ── 3. Read the E_ResourceType enum property via reflection ───────────
		const FByteProperty* TypeProp =
			FindFProperty<FByteProperty>(Resource->GetClass(), ResourceTypePropertyName);
		if (!TypeProp || !TypeProp->Enum)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("ATribeManager::CountResourcesNearLocation: "
				     "  -> skipped: property '%s' not found or not an enum on '%s'."),
				*ResourceTypePropertyName.ToString(),
				*Resource->GetClass()->GetName());
			continue;
		}

		const uint8 EnumVal  = TypeProp->GetPropertyValue_InContainer(Resource);
		const FName TypeName(TypeProp->Enum->GetAuthoredNameStringByValue(
		                         static_cast<int64>(EnumVal)));

		// ── 4. Read the ResourceAmount int32 property and accumulate ──────────
		const FIntProperty* AmountProp =
			FindFProperty<FIntProperty>(Resource->GetClass(), ResourceAmountPropertyName);
		if (!AmountProp)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("ATribeManager::CountResourcesNearLocation: "
				     "  -> skipped: property '%s' not found on '%s'."),
				*ResourceAmountPropertyName.ToString(),
				*Resource->GetClass()->GetName());
			continue;
		}

		int32 Amount = AmountProp->GetPropertyValue_InContainer(Resource);

		// RaspBerry resources are scaled by the NearbyResources / OccupiedResources
		// ratio to reflect their relative abundance.  Guard against division by
		// zero: when nothing is occupied the denominator is treated as 1.
		if (TypeName == RaspBerryResourceTypeName && ResourcePaths &&
			ResourcePaths->NearbyResourceCount() > 0)
		{
			const int32 Denominator = FMath::Max(1, ResourcePaths->OccupiedResourceCount());
			const float Multiplier  = static_cast<float>(ResourcePaths->NearbyResourceCount())
			                        / static_cast<float>(Denominator);
			Amount = FMath::RoundToInt(static_cast<float>(Amount) * Multiplier);
		}

		if (Amount > 0)
		{
			Result.FindOrAdd(TypeName) += Amount;
		}
	}

	return Result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Best resource location search
// ─────────────────────────────────────────────────────────────────────────────

TArray<FResourceLocationCandidate> ATribeManager::FindLocationsWithResources() const
{
	// All distances in Unreal units (1 UU = 1 cm).
	static constexpr float GridStep         =  1000.f;            // 10 m
	static constexpr float MinStorageDistSq = 10000.f * 10000.f;  // 100 m – must be outside
	static constexpr float MaxStorageDistSq = 20000.f * 20000.f;  // 200 m – must be inside
	static constexpr int32 Steps            = 30;                  // ±30 steps × 10 m = ±300 m

	TArray<FResourceLocationCandidate> Result;

	// ── Snapshot storage world positions ──────────────────────────────────────
	TArray<FVector> StorageLocations;
	StorageLocations.Reserve(storages.Num());
	for (const TObjectPtr<AActor>& StoragePtr : storages)
	{
		if (const AActor* Storage = StoragePtr.Get())
		{
			StorageLocations.Add(Storage->GetActorLocation());
		}
	}

	if (StorageLocations.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("ATribeManager::FindLocationsWithResources: No storages registered."));
		return Result;
	}

	// ── Generate and evaluate grid candidates ─────────────────────────────────
	// Each storage contributes a local ±200 m grid. A TSet of quantized grid
	// indices prevents evaluating the same world point more than once when
	// storage grids overlap.
	TSet<FIntVector> Visited;
	Visited.Reserve(StorageLocations.Num() * (2 * Steps + 1) * (2 * Steps + 1));

	for (const FVector& StorageLoc : StorageLocations)
	{
		for (int32 DY = -Steps; DY <= Steps; ++DY)
		{
			for (int32 DX = -Steps; DX <= Steps; ++DX)
			{
				const FVector Candidate(
					StorageLoc.X + DX * GridStep,
					StorageLoc.Y + DY * GridStep,
					StorageLoc.Z);  // Z approximated to the generating storage's height

				// Deduplicate: quantise to 20 m grid indices.
				const FIntVector GridIdx(
					FMath::RoundToInt(Candidate.X / GridStep),
					FMath::RoundToInt(Candidate.Y / GridStep),
					0);

				bool bAlreadyVisited;
				Visited.Add(GridIdx, &bAlreadyVisited);
				if (bAlreadyVisited) continue;

				// ── Filter: ≥100 m from ALL storages, ≤200 m from AT LEAST ONE ──
				bool bTooClose    = false;
				bool bWithinRange = false;

				for (const FVector& SLoc : StorageLocations)
				{
					const float DistSq = FVector::DistSquared(Candidate, SLoc);
					if (DistSq < MinStorageDistSq)
					{
						bTooClose = true;
						break;
					}
					if (DistSq <= MaxStorageDistSq)
					{
						bWithinRange = true;
					}
				}

				if (bTooClose || !bWithinRange) continue;

				// ── Count resources and build the entry ────────────────────────
				FResourceLocationCandidate Entry;
				Entry.Location  = Candidate;
				Entry.Resources = CountResourcesNearLocation(Candidate);

				for (const TPair<FName, int32>& Pair : Entry.Resources)
				{
					Entry.TotalCount += Pair.Value;
				}

				Result.Add(MoveTemp(Entry));
			}
		}
	}

	// ── Sort descending by TotalCount (most resources first) ──────────────────
	Result.Sort([](const FResourceLocationCandidate& A, const FResourceLocationCandidate& B)
	{
		return A.TotalCount > B.TotalCount;
	});

	return Result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Resource credit / deduction helpers
// ─────────────────────────────────────────────────────────────────────────────

bool ATribeManager::AddResourceAmount(FName ResourceType, int32 Amount)
{
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
				void* ValuePtr = MapHelper.GetValuePtr(Iter.GetInternalIndex());
				const int32 Current = Acc.ValInt->GetPropertyValue(ValuePtr);
				Acc.ValInt->SetPropertyValue(ValuePtr, Current + Amount);
				return true;
			}
		}
	}

	UE_LOG(LogTemp, Warning,
		TEXT("ATribeManager::AddResourceAmount: '%s' not found in any storage map."),
		*ResourceType.ToString());
	return false;
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

// ─────────────────────────────────────────────────────────────────────────────
// Task registry accessors
// ─────────────────────────────────────────────────────────────────────────────

bool ATribeManager::HasTribeManTask(ACharacter* TribeMan) const
{
	return tribeManTasks.Contains(TribeMan);
}

void ATribeManager::AssignTribeManTask(ACharacter* TribeMan, TSharedPtr<TribeTask> Task)
{
	tribeManTasks.Add(TribeMan, MoveTemp(Task));
}

// ─────────────────────────────────────────────────────────────────────────────
// Null-reference cleanup
// ─────────────────────────────────────────────────────────────────────────────

void ATribeManager::CheckNullReferences()
{
	// Resource-path state (NearbyResources, OccupiedResources, path lengths).
	if (ResourcePaths)
	{
		ResourcePaths->CheckNullReferences();
	}

	// TribeMan / storage / task state.
	const bool bAnyRemoved = checkTribeMen() || checkStorages();
	if (bAnyRemoved)
	{
		checkTribeManTasks();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// TribeMan idle-resume logic
// ─────────────────────────────────────────────────────────────────────────────

void ATribeManager::resumeIdleTribeManTasks()
{
	// Number of consecutive idle-after-Execute() detections before we give up
	// on the current task and free the TribeMan for re-assignment.
	static constexpr int32 MaxResumeFailures = 6;

	for (ACharacter* TribeMan : tribeMen)
	{
		if (!TribeMan) continue;

		TSharedPtr<TribeTask>* TaskPtr = tribeManTasks.Find(TribeMan);
		if (!TaskPtr)
		{
			continue;
		}
		if (!TaskPtr->IsValid())
		{
			tribeManTasks.Remove(TribeMan);
			continue;
		}

		const UCharacterMovementComponent* Movement = TribeMan->GetCharacterMovement();
		if (!Movement) continue;

		// Read the Blueprint bool variable "isWorking" via reflection.
		bool bIsWorking = false;
		if (const FBoolProperty* IsWorkingProp =
			FindFProperty<FBoolProperty>(TribeMan->GetClass(), FName(TEXT("isWorking"))))
		{
			bIsWorking = IsWorkingProp->GetPropertyValue_InContainer(TribeMan);
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("ATribeManager::resumeIdleTribeManTasks: 'isWorking' property not found on '%s'."),
				*TribeMan->GetActorNameOrLabel());
		}

		const bool bIsIdle = !bIsWorking && Movement->Velocity.IsNearlyZero();

		if (!bIsIdle)
		{
			// TribeMan is actively working or moving — reset failure counter.
			TribeManResumeFailures.Remove(TribeMan);
			continue;
		}

		// TribeMan is idle with an assigned task — try to resume.
		// Copy the SharedPtr before any potential map mutation below.
		TSharedPtr<TribeTask> Task = *TaskPtr;

		const bool bExecuteSucceeded = Task->Execute();

		if (!bExecuteSucceeded)
		{
			if (AActor* TargetRes = Task->GetTargetResource(); IsValid(TargetRes))
			{
				if (ResourcePaths) ResourcePaths->RemoveOccupied(TargetRes);
			}
			tribeManTasks.Remove(TribeMan);
			TribeManResumeFailures.Remove(TribeMan);
			RescueTribeManToNavmesh(TribeMan);
			continue;
		}

		// Execute() returned true but the TribeMan might still not be moving
		// (e.g. resource depleted at Blueprint level, navmesh failure, etc.).
		// Count consecutive idle resumes and abandon the task after the threshold.
		int32& Failures = TribeManResumeFailures.FindOrAdd(TribeMan, 0);
		++Failures;

		if (Failures >= MaxResumeFailures)
		{
			if (AActor* TargetRes = Task->GetTargetResource(); IsValid(TargetRes))
			{
				if (ResourcePaths) ResourcePaths->RemoveOccupied(TargetRes);
			}
			tribeManTasks.Remove(TribeMan);
			TribeManResumeFailures.Remove(TribeMan);
			RescueTribeManToNavmesh(TribeMan);
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// World-wide storage query
// ─────────────────────────────────────────────────────────────────────────────

TArray<FVector> ATribeManager::GetAllWorldStorageLocations() const
{
	TArray<FVector> Result;

	// Resolve which class to query: prefer the explicitly assigned StorageClass,
	// fall back to the runtime class of the first valid entry in this->storages.
	UClass* ClassToQuery = StorageClass.Get();
	if (!ClassToQuery)
	{
		for (const TObjectPtr<AActor>& StoragePtr : storages)
		{
			if (const AActor* S = StoragePtr.Get())
			{
				ClassToQuery = S->GetClass();
				break;
			}
		}
	}

	if (!ClassToQuery)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("ATribeManager::GetAllWorldStorages: StorageClass is not set and "
			     "this->storages is empty – cannot query world storages."));
		return Result;
	}

	TArray<AActor*> WorldStorages;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ClassToQuery, WorldStorages);

	Result.Reserve(WorldStorages.Num());
	for (const AActor* S : WorldStorages)
	{
		if (IsValid(S))
		{
			Result.Add(S->GetActorLocation());
		}
	}

	return Result;
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

void ATribeManager::RescueTribeManToNavmesh(ACharacter* TribeMan)
{
	if (!TribeMan) return;

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys) return;

	AAIController* AIC = Cast<AAIController>(TribeMan->GetController());
	if (!AIC) return;

	const FVector CurrentLoc = TribeMan->GetActorLocation();
	constexpr float RescueDistance = 5000.f; // 50 metres in UE units (1 UU = 1 cm)
	constexpr int32 MaxAttempts   = 20;
	constexpr float ProjectExtent = 2000.f;  // snap tolerance around the target point

	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		// Pick a random horizontal direction and step 50 m in that direction.
		const float AngleRad = FMath::FRandRange(0.f, 2.f * PI);
		const FVector Target = CurrentLoc + FVector(
			FMath::Cos(AngleRad) * RescueDistance,
			FMath::Sin(AngleRad) * RescueDistance,
			0.f);
		FNavLocation NavLocation;
		if (NavSys->ProjectPointToNavigation(Target, NavLocation, FVector(ProjectExtent)))
		{
			AIC->MoveToLocation(NavLocation.Location, /*AcceptanceRadius=*/200.f);
			return;
		}
	}

	UE_LOG(LogTemp, Warning,
		TEXT("ATribeManager: Could not rescue '%s' — no navmesh found in %d attempts at 50 m distance."),
		*TribeMan->GetName(), MaxAttempts);
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
			TribeManResumeFailures.Remove(tribeManTask->Key);
			tribeManTask.RemoveCurrent();
			tasksRemoved = true;
		}
	}
	return tasksRemoved;
}
