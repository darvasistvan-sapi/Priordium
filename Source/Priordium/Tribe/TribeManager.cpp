// Copyright Priordium. All Rights Reserved.

#include "TribeManager.h"

#include "UTribeGenerator.h"

#include "UObject/UnrealType.h"

#include "GameFramework/Character.h"
#include "Engine/EngineTypes.h"
#include "LandscapeProxy.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
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

    // Create a live instance of the ItemPrices class and call calculatePrices()
    // so the Blueprint function populates the Prices map before any lookup.
    if (ItemPrices)
    {
        ItemPricesInstance = NewObject<UObject>(this, ItemPrices);
        if (ItemPricesInstance)
        {
            // Defer calculatePrices() by one tick so all world actors are fully
            // initialized before the Blueprint function body executes.
            GetWorldTimerManager().SetTimerForNextTick(this, &ATribeManager::CallCalculatePrices);
        }
        else
        {
            UE_LOG(LogTemp, Warning,
                TEXT("ATribeManager::BeginPlay: Failed to create ItemPrices instance from class '%s'."),
                *ItemPrices->GetName());
        }
    }

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

void ATribeManager::CallCalculatePrices()
{
    if (!ItemPricesInstance) return;

    UFunction* CalcFunc = ItemPricesInstance->FindFunction(TEXT("calculatePrices"));
    if (CalcFunc)
    {
        void* Params = FMemory_Alloca(FMath::Max<int32>(CalcFunc->ParmsSize, 1));
        FMemory::Memzero(Params, CalcFunc->ParmsSize);
        ItemPricesInstance->ProcessEvent(CalcFunc, Params);
    }
    else
    {
        UE_LOG(LogTemp, Warning,
            TEXT("ATribeManager::CallCalculatePrices: calculatePrices() not found on '%s'."),
            *ItemPricesInstance->GetClass()->GetName());
    }
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
	static constexpr float SearchRadiusSq         = 5000.f * 5000.f;
	static constexpr float StorageExclusionRadiusSq = 10000.f * 10000.f;

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
		for (const FVector& StorageLoc : StorageLocations)
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

		const int32 Amount = AmountProp->GetPropertyValue_InContainer(Resource);
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
	static constexpr int32 Steps            = 10;                  // ±10 steps × 10 m = ±100 m

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

	UE_LOG(LogTemp, Log,
		TEXT("ATribeManager::FindLocationsWithResources: %d candidates evaluated, "
		     "%d valid points returned (best: %d resources)."),
		Visited.Num(),
		Result.Num(),
		Result.IsEmpty() ? 0 : Result[0].TotalCount);

	return Result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Item price lookup
// Resolves the FMapProperty (key = FClassProperty, value = FObjectProperty)
// named ItemPricesPricesPropertyName on the ItemPrices object and returns the
// BP_ItemPrice_C instance whose key matches ItemClass.
// ─────────────────────────────────────────────────────────────────────────────

UObject* ATribeManager::GetItemPrice(TSubclassOf<AActor> ItemClass) const
{
	if (!ItemPricesInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("ATribeManager::GetItemPrice: ItemPricesInstance is null (was ItemPrices set before BeginPlay?)."));
		return nullptr;
	}
	if (!ItemClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("ATribeManager::GetItemPrice: ItemClass is null."));
		return nullptr;
	}

	FMapProperty* MapProp = FindFProperty<FMapProperty>(
		ItemPricesInstance->GetClass(), ItemPricesPricesPropertyName);
	if (!MapProp)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("ATribeManager::GetItemPrice: Could not find map property '%s' on '%s'."),
			*ItemPricesPricesPropertyName.ToString(),
			*ItemPricesInstance->GetClass()->GetName());
		return nullptr;
	}

	FClassProperty* KeyProp = CastField<FClassProperty>(MapProp->KeyProp);
	FObjectProperty* ValProp = CastField<FObjectProperty>(MapProp->ValueProp);
	if (!KeyProp || !ValProp)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("ATribeManager::GetItemPrice: Map property '%s' has unexpected key/value types (key=%s, value=%s)."),
			*ItemPricesPricesPropertyName.ToString(),
			*MapProp->KeyProp->GetClass()->GetName(),
			*MapProp->ValueProp->GetClass()->GetName());
		return nullptr;
	}

	FScriptMapHelper MapHelper(MapProp, MapProp->ContainerPtrToValuePtr<void>(ItemPricesInstance.Get()));
	for (FScriptMapHelper::FIterator Iter(MapHelper); Iter; ++Iter)
	{
		UE_LOG(LogTemp, Log,
			TEXT("ATribeManager::GetItemPrice: Checking price entry %d..."), MapHelper.GetMaxIndex());
		const UClass* StoredClass = Cast<UClass>(
			KeyProp->GetPropertyValue(MapHelper.GetKeyPtr(Iter.GetInternalIndex())));
		if (StoredClass == ItemClass)
		{
			return ValProp->GetPropertyValue(
				MapHelper.GetValuePtr(Iter.GetInternalIndex()));
		}
	}

	UE_LOG(LogTemp, Warning,
		TEXT("ATribeManager::GetItemPrice: No price entry found for class '%s'."),
		*ItemClass->GetName());
	return nullptr;
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
	// ── 1. Resolve the price object ───────────────────────────────────────────
	UObject* PriceObj = GetItemPrice(BuildingClass);
	if (!PriceObj)
	{
		UE_LOG(LogTemp, Error,
			TEXT("ATribeManager::Build: No price entry found for '%s'. Build aborted."),
			*BuildingClass->GetName());
		return nullptr;
	}

	// ── 2. Read the Resources cost map from the price object ──────────────────
	static const FName ResourcesPropertyName(TEXT("Resources"));

	FMapProperty* CostMapProp = FindFProperty<FMapProperty>(
		PriceObj->GetClass(), ResourcesPropertyName);

	if (CostMapProp)
	{
		const FByteProperty* KeyByte = CastField<FByteProperty>(CostMapProp->KeyProp);
		const FIntProperty*  ValInt  = CastField<FIntProperty>(CostMapProp->ValueProp);

		if (!KeyByte || !ValInt || !KeyByte->Enum)
		{
			UE_LOG(LogTemp, Error,
				TEXT("ATribeManager::Build: 'Resources' map on BP_ItemPrice has unexpected key/value types."));
			return nullptr;
		}

		// Snapshot costs into a plain array so we can check before deducting.
		TArray<TPair<FName, int32>> Costs;
		{
			FScriptMapHelper CostMap(CostMapProp,
				CostMapProp->ContainerPtrToValuePtr<void>(PriceObj));
			for (FScriptMapHelper::FIterator Iter(CostMap); Iter; ++Iter)
			{
				const uint8 EnumVal = KeyByte->GetPropertyValue(
					CostMap.GetKeyPtr(Iter.GetInternalIndex()));
				const FName ResType(
					KeyByte->Enum->GetAuthoredNameStringByValue(static_cast<int64>(EnumVal)));
				const int32 Required = ValInt->GetPropertyValue(
					CostMap.GetValuePtr(Iter.GetInternalIndex()));
				Costs.Emplace(ResType, Required);
			}
		}

		// ── 3. Affordability check ────────────────────────────────────────────
		for (const TPair<FName, int32>& Cost : Costs)
		{
			const int32 Available = GetResourceAmount(Cost.Key);
			if (Available < Cost.Value)
			{
				UE_LOG(LogTemp, Error,
					TEXT("ATribeManager::Build: Not enough '%s' to build '%s'. Required: %d, Available: %d."),
					*Cost.Key.ToString(), *BuildingClass->GetName(), Cost.Value, Available);
				return nullptr;
			}
		}

		// ── 4. Deduct resources ───────────────────────────────────────────────
		for (const TPair<FName, int32>& Cost : Costs)
		{
			DeductResourceAmount(Cost.Key, Cost.Value);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("ATribeManager::Build: Price object for '%s' has no 'Resources' map. Building for free."),
			*BuildingClass->GetName());
	}

	// ── 5. Spawn the building ─────────────────────────────────────────────────
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

	// Candidate points sorted by resource richness (most resources first).
	// Each point already satisfies the 100 m / 200 m storage-distance constraints.
	const TArray<FResourceLocationCandidate> Candidates = FindLocationsWithResources();

	if (Candidates.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("ATribeManager::FindFreeBuildLocation: FindLocationsWithResources returned no candidates."));
		return false;
	}

	static constexpr float ClearRadius = 500.f;   // 5 m clearance sphere

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

	// Iterate candidates best-first; return the first one that lies on the navmesh
	// and has no blocking actors within the clearance radius.
	for (const FResourceLocationCandidate& Entry : Candidates)
	{
		// Lift the candidate 5000 cm upward so ProjectPointToNavigation can snap
		// downward to the actual terrain surface regardless of the stored Z.
		const FVector Elevated(Entry.Location.X, Entry.Location.Y, Entry.Location.Z + 5000.f);

		FNavLocation NavLoc;
		if (!NavSys->ProjectPointToNavigation(Elevated, NavLoc, NavExtent))
		{
			continue;
		}

		// Raise the overlap sphere 150 cm above the surface to reduce false hits
		// against the landscape mesh, then strip remaining landscape hits.
		TArray<AActor*> HitActors;
		const FVector CheckPos = NavLoc.Location + FVector(0.f, 0.f, 150.f);
		UKismetSystemLibrary::SphereOverlapActors(
			World, CheckPos, ClearRadius,
			ObjectTypes, nullptr, IgnoreActors, HitActors);

		HitActors.RemoveAll([](const AActor* A)
		{
			return !A || A->IsA<ALandscapeProxy>();
		});

		if (!HitActors.IsEmpty()) continue;

		OutLocation = NavLoc.Location;
		UE_LOG(LogTemp, Log,
			TEXT("ATribeManager::FindFreeBuildLocation: Chose (%.0f, %.0f, %.0f) "
			     "with %d nearby resource(s)."),
			OutLocation.X, OutLocation.Y, OutLocation.Z, Entry.TotalCount);
		return true;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("ATribeManager::FindFreeBuildLocation: No free build spot found "
		     "among %d candidate(s)."),
		Candidates.Num());
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
