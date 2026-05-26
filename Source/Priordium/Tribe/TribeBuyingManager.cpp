// Copyright Priordium. All Rights Reserved.

#include "TribeBuyingManager.h"

#include "TribeManager.h"
#include "UTribeGenerator.h"

#include "GameFramework/Character.h"
#include "UObject/UnrealType.h"
#include "NavigationSystem.h"
#include "Kismet/KismetSystemLibrary.h"
#include "LandscapeProxy.h"

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

UTribeBuyingManager::UTribeBuyingManager()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// BeginPlay — create the ItemPrices instance and defer calculatePrices()
// ─────────────────────────────────────────────────────────────────────────────

void UTribeBuyingManager::BeginPlay()
{
	Super::BeginPlay();

	if (!ItemPrices) return;

	ItemPricesInstance = NewObject<UObject>(this, ItemPrices);
	if (ItemPricesInstance)
	{
		// Defer calculatePrices() by one tick so all world actors are fully
		// initialised before the Blueprint function body executes.
		GetWorld()->GetTimerManager().SetTimerForNextTick(
			this, &UTribeBuyingManager::CallCalculatePrices);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UTribeBuyingManager::BeginPlay: Failed to create ItemPrices instance from class '%s'."),
			*ItemPrices->GetName());
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal: owner access
// ─────────────────────────────────────────────────────────────────────────────

ATribeManager* UTribeBuyingManager::GetOwnerManager() const
{
	return Cast<ATribeManager>(GetOwner());
}

// ─────────────────────────────────────────────────────────────────────────────
// CallCalculatePrices
// ─────────────────────────────────────────────────────────────────────────────

void UTribeBuyingManager::CallCalculatePrices()
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
			TEXT("UTribeBuyingManager::CallCalculatePrices: calculatePrices() not found on '%s'."),
			*ItemPricesInstance->GetClass()->GetName());
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// GetItemPriceObject
// ─────────────────────────────────────────────────────────────────────────────

UObject* UTribeBuyingManager::GetItemPriceObject(TSubclassOf<AActor> ItemClass) const
{
	if (!ItemPricesInstance)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UTribeBuyingManager::GetItemPrice: ItemPricesInstance is null "
			     "(was ItemPrices set before BeginPlay?)."));
		return nullptr;
	}
	if (!ItemClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UTribeBuyingManager::GetItemPrice: ItemClass is null."));
		return nullptr;
	}

	FMapProperty* MapProp = FindFProperty<FMapProperty>(
		ItemPricesInstance->GetClass(), ItemPricesPricesPropertyName);
	if (!MapProp)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UTribeBuyingManager::GetItemPrice: Could not find map property '%s' on '%s'."),
			*ItemPricesPricesPropertyName.ToString(),
			*ItemPricesInstance->GetClass()->GetName());
		return nullptr;
	}

	FClassProperty*  KeyProp = CastField<FClassProperty>(MapProp->KeyProp);
	FObjectProperty* ValProp = CastField<FObjectProperty>(MapProp->ValueProp);
	if (!KeyProp || !ValProp)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UTribeBuyingManager::GetItemPrice: Map property '%s' has unexpected "
			     "key/value types (key=%s, value=%s)."),
			*ItemPricesPricesPropertyName.ToString(),
			*MapProp->KeyProp->GetClass()->GetName(),
			*MapProp->ValueProp->GetClass()->GetName());
		return nullptr;
	}

	FScriptMapHelper MapHelper(MapProp, MapProp->ContainerPtrToValuePtr<void>(ItemPricesInstance.Get()));
	for (FScriptMapHelper::FIterator Iter(MapHelper); Iter; ++Iter)
	{
		const UClass* StoredClass = Cast<UClass>(
			KeyProp->GetPropertyValue(MapHelper.GetKeyPtr(Iter.GetInternalIndex())));
		if (StoredClass == ItemClass)
		{
			return ValProp->GetPropertyValue(
				MapHelper.GetValuePtr(Iter.GetInternalIndex()));
		}
	}

	return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// GetItemPrice
// ─────────────────────────────────────────────────────────────────────────────

TArray<TPair<FName, int32>> UTribeBuyingManager::GetItemPrice(TSubclassOf<AActor> ItemClass) const
{
	UObject* PriceObj = GetItemPriceObject(ItemClass);
	if (!PriceObj)
	{
		UE_LOG(LogTemp, Error,
			TEXT("UTribeBuyingManager::GetItemPrice: No price entry found for '%s'."),
			*ItemClass->GetName());
		return {};
	}

	static const FName ResourcesPropertyName(TEXT("Resources"));

	FMapProperty* CostMapProp = FindFProperty<FMapProperty>(
		PriceObj->GetClass(), ResourcesPropertyName);
	if (!CostMapProp)
	{
		UE_LOG(LogTemp, Error,
			TEXT("UTribeBuyingManager::GetItemPrice: No 'Resources' map found on price object '%s'."),
			*PriceObj->GetClass()->GetName());
		return {};
	}

	const FByteProperty* KeyByte = CastField<FByteProperty>(CostMapProp->KeyProp);
	const FIntProperty*  ValInt  = CastField<FIntProperty>(CostMapProp->ValueProp);
	if (!KeyByte || !ValInt || !KeyByte->Enum)
	{
		UE_LOG(LogTemp, Error,
			TEXT("UTribeBuyingManager::GetItemPrice: 'Resources' map on BP_ItemPrice "
			     "has unexpected key/value types."));
		return {};
	}

	TArray<TPair<FName, int32>> Costs;
	FScriptMapHelper CostMap(CostMapProp, CostMapProp->ContainerPtrToValuePtr<void>(PriceObj));
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
	return Costs;
}

// ─────────────────────────────────────────────────────────────────────────────
// CanAfford / DeductResources
// ─────────────────────────────────────────────────────────────────────────────

bool UTribeBuyingManager::CanAfford(const TArray<TPair<FName, int32>>& Costs) const
{
	ATribeManager* Owner = GetOwnerManager();
	if (!Owner) return false;

	for (const TPair<FName, int32>& Cost : Costs)
	{
		if (Owner->GetResourceAmount(Cost.Key) < Cost.Value)
		{
			return false;
		}
	}
	return true;
}

bool UTribeBuyingManager::DeductResources(const TArray<TPair<FName, int32>>& Costs)
{
	ATribeManager* Owner = GetOwnerManager();
	if (!Owner) return false;

	for (const TPair<FName, int32>& Cost : Costs)
	{
		if (!Owner->DeductResourceAmount(Cost.Key, Cost.Value))
		{
			UE_LOG(LogTemp, Error,
				TEXT("UTribeBuyingManager::DeductResources: Failed to deduct %d of '%s'."),
				Cost.Value, *Cost.Key.ToString());
			return false;
		}
	}
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Build
// ─────────────────────────────────────────────────────────────────────────────

AActor* UTribeBuyingManager::Build(TSubclassOf<AActor> BuildingClass, FVector Location)
{
	ATribeManager* Owner = GetOwnerManager();
	if (!Owner) return nullptr;

	// ── 1. Price lookup ───────────────────────────────────────────────────────
	TArray<TPair<FName, int32>> Costs = GetItemPrice(BuildingClass);
	if (Costs.IsEmpty())
	{
		UE_LOG(LogTemp, Error,
			TEXT("UTribeBuyingManager::Build: No price entry found for '%s'. Build aborted."),
			*BuildingClass->GetName());
		return nullptr;
	}

	// ── 2. Affordability check ────────────────────────────────────────────────
	if (!CanAfford(Costs))
	{
		return nullptr;
	}

	// ── 3. Deduct resources ───────────────────────────────────────────────────
	if (!DeductResources(Costs))
	{
		UE_LOG(LogTemp, Error,
			TEXT("UTribeBuyingManager::Build: Failed to deduct resources for '%s'. Build aborted."),
			*BuildingClass->GetName());
		return nullptr;
	}

	// ── 4. Spawn the building ─────────────────────────────────────────────────
	return UTribeGenerator::SpawnBuilding(
		GetWorld(),
		Owner,
		BuildingClass,
		Location,
		Owner->TerrainHeightmap.Get(),
		Owner->TerrainSettings.Get(),
		Owner->TerrainLandscapeBuilder.Get(),
		Owner->TribeActor.Get(),
		Owner->TribeFolderPath);
}

// ─────────────────────────────────────────────────────────────────────────────
// CreateTribeMan
// ─────────────────────────────────────────────────────────────────────────────

ACharacter* UTribeBuyingManager::CreateTribeMan()
{
	ATribeManager* Owner = GetOwnerManager();
	if (!Owner) return nullptr;

	// ── 1. Class check ────────────────────────────────────────────────────────
	if (!Owner->TribeManClass)
	{
		UE_LOG(LogTemp, Error,
			TEXT("UTribeBuyingManager::CreateTribeMan: TribeManClass is not set."));
		return nullptr;
	}

	// ── 2. Price lookup ───────────────────────────────────────────────────────
	TArray<TPair<FName, int32>> Costs = GetItemPrice(TSubclassOf<AActor>(Owner->TribeManClass));
	if (Costs.IsEmpty())
	{
		UE_LOG(LogTemp, Error,
			TEXT("UTribeBuyingManager::CreateTribeMan: No price entry found for '%s'. Aborted."),
			*Owner->TribeManClass->GetName());
		return nullptr;
	}

	// ── 3. Affordability check ────────────────────────────────────────────────
	if (!CanAfford(Costs))
	{
		return nullptr;
	}

	// ── 4. Find a free spawn location near a storage ──────────────────────────
	FVector SpawnLocation;
	if (!FindFreeTribeManSpawnLocation(SpawnLocation))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UTribeBuyingManager::CreateTribeMan: No free spawn location found near any storage."));
		return nullptr;
	}

	// ── 5. Deduct resources ───────────────────────────────────────────────────
	if (!DeductResources(Costs))
	{
		UE_LOG(LogTemp, Error,
			TEXT("UTribeBuyingManager::CreateTribeMan: Failed to deduct resources. Aborted."));
		return nullptr;
	}

	// ── 6. Spawn via UTribeGenerator::SpawnTribeMen (Count=1, Radius=0) ───────
	TArray<ACharacter*> Spawned = UTribeGenerator::SpawnTribeMen(
		GetWorld(), Owner, SpawnLocation,
		TSubclassOf<AActor>(Owner->TribeManClass),
		/*Count=*/1, /*SpawnRadius=*/0.f,
		Owner->TribeActor.Get(), Owner->TribeFolderPath);

	if (Spawned.IsEmpty())
	{
		UE_LOG(LogTemp, Error,
			TEXT("UTribeBuyingManager::CreateTribeMan: UTribeGenerator::SpawnTribeMen failed for '%s'."),
			*Owner->TribeManClass->GetName());
		return nullptr;
	}

	return Spawned[0];
}

// ─────────────────────────────────────────────────────────────────────────────
// TryBuildStorage
// ─────────────────────────────────────────────────────────────────────────────

void UTribeBuyingManager::TryBuildStorage()
{
	ATribeManager* Owner = GetOwnerManager();
	if (!Owner) return;

	// Determine the class to build from an existing storage.
	AActor* ReferenceStorage = nullptr;
	for (const TObjectPtr<AActor>& S : Owner->storages)
	{
		if (S.Get()) { ReferenceStorage = S.Get(); break; }
	}
	if (!ReferenceStorage) return;

	FVector BuildLocation;
	if (!FindFreeBuildLocation(BuildLocation)) return;

	Build(ReferenceStorage->GetClass(), BuildLocation);

	// Refresh NearbyResources and StorageResourcePaths after the new storage is built.
	if (Owner->ResourcePaths)
	{
		Owner->ResourcePaths->RefreshPaths();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// FindFreeBuildLocation
// ─────────────────────────────────────────────────────────────────────────────

bool UTribeBuyingManager::FindFreeBuildLocation(FVector& OutLocation) const
{
	ATribeManager* Owner = GetOwnerManager();
	if (!Owner) return false;

	UWorld* World = GetWorld();
	if (!World) return false;

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys) return false;

	// Candidate points sorted by resource richness (most resources first).
	const TArray<FResourceLocationCandidate> Candidates = Owner->FindLocationsWithResources();
	if (Candidates.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UTribeBuyingManager::FindFreeBuildLocation: FindLocationsWithResources returned no candidates."));
		return false;
	}

	static constexpr float ClearRadius = 500.f;   // 5 m clearance sphere

	const TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes =
	{
		UEngineTypes::ConvertToObjectType(ECC_WorldStatic),
		UEngineTypes::ConvertToObjectType(ECC_WorldDynamic),
		UEngineTypes::ConvertToObjectType(ECC_Pawn),
	};
	const TArray<AActor*> IgnoreActors = { Owner };
	const FVector NavExtent(500.f, 500.f, 5000.f);

	for (const FResourceLocationCandidate& Entry : Candidates)
	{
		// Lift the candidate upward so ProjectPointToNavigation snaps down to terrain.
		const FVector Elevated(Entry.Location.X, Entry.Location.Y, Entry.Location.Z + 5000.f);

		FNavLocation NavLoc;
		if (!NavSys->ProjectPointToNavigation(Elevated, NavLoc, NavExtent)) continue;

		// Raise the overlap sphere above the surface to reduce false landscape hits.
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
		return true;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("UTribeBuyingManager::FindFreeBuildLocation: No free build spot found "
		     "among %d candidate(s)."),
		Candidates.Num());
	return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// FindFreeTribeManSpawnLocation
// ─────────────────────────────────────────────────────────────────────────────

bool UTribeBuyingManager::FindFreeTribeManSpawnLocation(FVector& OutLocation) const
{
	ATribeManager* Owner = GetOwnerManager();
	if (!Owner) return false;

	UWorld* World = GetWorld();
	if (!World) return false;

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys) return false;

	if (Owner->storages.IsEmpty()) return false;

	// Target spawn distance from the storage (~10 m = 1000 cm).
	static constexpr float SpawnRadius = 1000.f;
	static constexpr float ClearRadius = 100.f;   // 1 m — tight enough for a character
	const FVector NavExtent(200.f, 200.f, 5000.f);

	const TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes =
	{
		UEngineTypes::ConvertToObjectType(ECC_WorldStatic),
		UEngineTypes::ConvertToObjectType(ECC_WorldDynamic),
		UEngineTypes::ConvertToObjectType(ECC_Pawn),
	};
	const TArray<AActor*> IgnoreActors = { Owner };

	static constexpr int32 MaxAttempts = 20;

	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		const AActor* Storage =
			Owner->storages[FMath::RandRange(0, Owner->storages.Num() - 1)].Get();
		if (!Storage) continue;

		const float Angle = FMath::FRandRange(0.f, 2.f * PI);
		const FVector Candidate(
			Storage->GetActorLocation().X + FMath::Cos(Angle) * SpawnRadius,
			Storage->GetActorLocation().Y + FMath::Sin(Angle) * SpawnRadius,
			Storage->GetActorLocation().Z + 5000.f);

		FNavLocation NavLoc;
		if (!NavSys->ProjectPointToNavigation(Candidate, NavLoc, NavExtent)) continue;

		TArray<AActor*> HitActors;
		const FVector CheckPos = NavLoc.Location + FVector(0.f, 0.f, 50.f);
		UKismetSystemLibrary::SphereOverlapActors(
			World, CheckPos, ClearRadius,
			ObjectTypes, nullptr, IgnoreActors, HitActors);

		HitActors.RemoveAll([](const AActor* A)
		{
			return !A || A->IsA<ALandscapeProxy>();
		});

		if (!HitActors.IsEmpty()) continue;

		OutLocation = NavLoc.Location;
		return true;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("UTribeBuyingManager::FindFreeTribeManSpawnLocation: "
		     "No free spot found after %d attempts."),
		MaxAttempts);
	return false;
}
