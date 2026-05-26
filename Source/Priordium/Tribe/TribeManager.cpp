// Copyright Priordium. All Rights Reserved.

#include "TribeManager.h"

#include "UTribeGenerator.h"
#include "Quest.h"
#include "QuestManager.h"
#include "TradeOffer.h"

#include "UObject/UnrealType.h"

#include "GameFramework/Character.h"
#include "Engine/EngineTypes.h"
#include "LandscapeProxy.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "AI/Navigation/NavQueryFilter.h"
#include "AIController.h"
#include "TribeTask.h"
#include "ResourceGatheringTask.h"
#include "UMapGeneratorSettings.h"

// ─────────────────────────────────────────────────────────────────────────────
// Storage map accessor
// Resolves the FMapProperty (key = FByteProperty+UEnum, value = FIntProperty)
// named StorageInventoryPropertyName on a storage actor.
// ─────────────────────────────────────────────────────────────────────────────
// Shorthand for the verbose quest-needs type used by GetQuestResourceNeeds().
using FQuestNeeds = TArray<TPair<UQuest*, TArray<TPair<FName, int32>>>>;

namespace
{
	// ── Storage reflection helper ─────────────────────────────────────────────

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

	// ── Trade static helpers ──────────────────────────────────────────────────

	/**
	 * For each item in Items whose key exists in Budget with a value > 0,
	 * adds { key, min(item.Value, budget) } to the result.
	 * Items whose key is absent from Budget or has a budget of 0 are dropped.
	 *
	 * Used to build:
	 *   WantItems    = FilterByBudget(Offer->Offered,    Deficit)
	 *   CanGiveItems = FilterByBudget(Offer->Requested,  Surplus)
	 */
	TArray<TPair<FName, int32>> FilterByBudget(
		const TArray<TPair<FName, int32>>& Items,
		const TMap<FName, int32>&          Budget)
	{
		TArray<TPair<FName, int32>> Result;
		for (const TPair<FName, int32>& Item : Items)
		{
			const int32* BudgetPtr = Budget.Find(Item.Key);
			if (!BudgetPtr || *BudgetPtr <= 0) continue;
			const int32 Capped = FMath::Min(Item.Value, *BudgetPtr);
			if (Capped > 0) Result.Add({ Item.Key, Capped });
		}
		return Result;
	}

	/**
	 * Balances WantItems and CanGiveItems so that the output arrays satisfy:
	 *   sum(OutSenderGives.Value) == sum(OutSenderTakes.Value) == ExchangeTotal
	 * where ExchangeTotal = min(TotalWant, TotalCanGive).
	 *
	 * The side with the larger total is trimmed greedily: items are consumed in
	 * order until the budget (ExchangeTotal) is exhausted.
	 *
	 * Returns false (and leaves the out-arrays empty) if ExchangeTotal == 0.
	 */
	bool BalanceExchangeItems(
		const TArray<TPair<FName, int32>>& WantItems,
		const TArray<TPair<FName, int32>>& CanGiveItems,
		TArray<TPair<FName, int32>>&       OutSenderGives,
		TArray<TPair<FName, int32>>&       OutSenderTakes)
	{
		int32 TotalWant = 0;
		for (const TPair<FName, int32>& P : WantItems)    TotalWant    += P.Value;

		int32 TotalCanGive = 0;
		for (const TPair<FName, int32>& P : CanGiveItems) TotalCanGive += P.Value;

		const int32 ExchangeTotal = FMath::Min(TotalWant, TotalCanGive);
		if (ExchangeTotal <= 0) return false;

		if (TotalWant <= TotalCanGive)
		{
			// Receive the full WantItems; trim CanGiveItems to match.
			OutSenderGives = WantItems;
			int32 Remaining = ExchangeTotal;
			for (const TPair<FName, int32>& Item : CanGiveItems)
			{
				if (Remaining <= 0) break;
				const int32 Give = FMath::Min(Item.Value, Remaining);
				OutSenderTakes.Add({ Item.Key, Give });
				Remaining -= Give;
			}
		}
		else
		{
			// Give the full CanGiveItems; trim WantItems to match.
			OutSenderTakes = CanGiveItems;
			int32 Remaining = ExchangeTotal;
			for (const TPair<FName, int32>& Item : WantItems)
			{
				if (Remaining <= 0) break;
				const int32 Receive = FMath::Min(Item.Value, Remaining);
				OutSenderGives.Add({ Item.Key, Receive });
				Remaining -= Receive;
			}
		}

		// Invariant: both sides must carry the same total.
		{
			int32 GivesSum = 0, TakesSum = 0;
			for (const TPair<FName, int32>& P : OutSenderGives) GivesSum += P.Value;
			for (const TPair<FName, int32>& P : OutSenderTakes) TakesSum += P.Value;
			check(GivesSum == TakesSum);
		}
		return true;
	}

} // namespace

// Static member definition — one list shared across all instances.
TArray<TWeakObjectPtr<ATribeManager>> ATribeManager::AllTribeManagers;

// ─────────────────────────────────────────────────────────────────────────────

ATribeManager::ATribeManager()
{
    // Ignore updating on every frame, this actor only works when calculateStorageResourcePathLengths() is called.
    PrimaryActorTick.bCanEverTick = false;
}

void ATribeManager::BeginPlay()
{
    Super::BeginPlay();

    // Register this instance so other systems can reach every active tribe.
    AllTribeManagers.Add(this);

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
	// This prevents double-Execute(): orderTribeMenToCollect calls Execute() when
	// assigning a task, and if resumeIdleTribeManTasks ran afterwards it would see
	// velocity=0 (physics not yet updated) and call Execute() a second time on the
	// same tick, breaking the Blueprint state machine.
	resumeIdleTribeManTasks();

	// Assign tasks to TribeMen that have no task yet.
	orderTribeMenToCollect();

	if (0.9 * nearbyResources.Num() <= OccupiedResources.Num())
	{
		TryBuildStorage();
	}

	if (tribeMen.Num() < 50 && OccupiedResources.Num() < nearbyResources.Num())
	{
		CreateTribeMan();
	}

	CheckQuests();
}

void ATribeManager::CheckQuests()
{
	const FQuestNeeds QuestResourceNeeds = GetQuestResourceNeeds();

	// If the most affordable quest has no missing resources → complete it.
	// GetQuestResourceNeeds sorts ascending by total missing amount, so index 0
	// is always the quest closest to completion.
	if (QuestResourceNeeds.Num() > 0 && QuestResourceNeeds[0].Value.Num() == 0)
	{
		CompleteQuest(QuestResourceNeeds[0].Key);
		return;
	}

	if (QuestResourceNeeds.Num() > 0)
	{
		ExecuteTradeOffersForQuests(TArray<UQuest*>{ QuestResourceNeeds[0].Key });
	}
	ReceivedTradeOffers.Empty();

	// ── Broadcast outgoing trade offers (0.5% chance per tick) ──────────────
	if (!QuestManager || QuestManager->Quests.IsEmpty()) return;

	//if (FMath::FRand() <= 0.2f)
	//{
		if (TradeQuests.Num() >= QuestManager->Quests.Num())
		{
			TradeQuests.Empty();
		}

		TradeQuests.Add(QuestManager->Quests[TradeQuests.Num()]);

		// Pass only the first 3 elements to avoid overly broad offers.
		const int32 SliceCount = FMath::Min(TradeQuests.Num(), 3);
		TArray<UQuest*> QuestSlice;
		for (int32 i = 0; i < SliceCount; ++i)
		{
			QuestSlice.Add(TradeQuests[i].Get());
		}
		CreateTradeOfferForQuests(QuestSlice);
	//}
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

		// raspBerry resources are scaled by the nearbyResources / occupiedResources
		// ratio to reflect their relative abundance.  Guard against division by
		// zero: when nothing is occupied the denominator is treated as 1.
		if (TypeName == RaspBerryResourceTypeName && nearbyResources.Num() > 0)
		{
			const int32 Denominator = FMath::Max(1, OccupiedResources.Num());
			const float Multiplier  = static_cast<float>(nearbyResources.Num())
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
// Item price lookup
// Resolves the FMapProperty (key = FClassProperty, value = FObjectProperty)
// named ItemPricesPricesPropertyName on the ItemPrices object and returns the
// BP_ItemPrice_C instance whose key matches ItemClass.
// ─────────────────────────────────────────────────────────────────────────────

TArray<TPair<FName, int32>> ATribeManager::GetItemPrice(TSubclassOf<AActor> ItemClass) const
{
	UObject* PriceObj = GetItemPriceObject(ItemClass);
	if (!PriceObj)
	{
		UE_LOG(LogTemp, Error,
			TEXT("ATribeManager::GetItemPrice: No price entry found for '%s'."),
			*ItemClass->GetName());
		return {};
	}

	static const FName ResourcesPropertyName(TEXT("Resources"));

	FMapProperty* CostMapProp = FindFProperty<FMapProperty>(
		PriceObj->GetClass(), ResourcesPropertyName);

	if (!CostMapProp)
	{
		UE_LOG(LogTemp, Error,
			TEXT("ATribeManager::GetItemPrice: No 'Resources' map found on price object '%s'."),
			*PriceObj->GetClass()->GetName());
		return {};
	}

	const FByteProperty* KeyByte = CastField<FByteProperty>(CostMapProp->KeyProp);
	const FIntProperty*  ValInt  = CastField<FIntProperty>(CostMapProp->ValueProp);

	if (!KeyByte || !ValInt || !KeyByte->Enum)
	{
		UE_LOG(LogTemp, Error,
			TEXT("ATribeManager::GetItemPrice: 'Resources' map on BP_ItemPrice has unexpected key/value types."));
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

UObject* ATribeManager::GetItemPriceObject(TSubclassOf<AActor> ItemClass) const
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
// Quest resource needs
// ─────────────────────────────────────────────────────────────────────────────

TArray<TPair<UQuest*, TArray<TPair<FName, int32>>>> ATribeManager::GetQuestResourceNeeds() const
{
	TArray<TPair<UQuest*, TArray<TPair<FName, int32>>>> Result;

	if (!QuestManager) return Result;

	for (const TObjectPtr<UQuest>& QuestPtr : QuestManager->Quests)
	{
		UQuest* Quest = QuestPtr.Get();
		if (!Quest) continue;

		// Calculate still-missing amounts for each requirement.
		TArray<TPair<FName, int32>> Missing;
		int32 TotalMissing = 0;

		for (const TPair<FName, int32>& Req : Quest->Requirements)
		{
			const int32 Have    = GetResourceAmount(Req.Key);
			const int32 Lacking = FMath::Max(0, Req.Value - Have);
			if (Lacking > 0)
			{
				Missing.Add({ Req.Key, Lacking });
				TotalMissing += Lacking;
			}
		}

		Result.Add({ Quest, MoveTemp(Missing) });
	}

	// Sort ascending by total missing amount (most affordable first).
	Result.Sort([this](
		const TPair<UQuest*, TArray<TPair<FName, int32>>>& A,
		const TPair<UQuest*, TArray<TPair<FName, int32>>>& B)
	{
		auto SumMissing = [](const TArray<TPair<FName, int32>>& Missing) -> int32
		{
			int32 Total = 0;
			for (const TPair<FName, int32>& P : Missing) Total += P.Value;
			return Total;
		};
		return SumMissing(A.Value) < SumMissing(B.Value);
	});

	return Result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Quest completion
// ─────────────────────────────────────────────────────────────────────────────

bool ATribeManager::CompleteQuest(UQuest* Quest)
{
	if (!Quest)
	{
		UE_LOG(LogTemp, Warning, TEXT("ATribeManager::CompleteQuest: Quest is null."));
		return false;
	}

	if (!QuestManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("ATribeManager::CompleteQuest: QuestManager is not set."));
		return false;
	}

	// ── 2. Affordability check ────────────────────────────────────────────────
	if (!CanAfford(Quest->Requirements))
	{
		UE_LOG(LogTemp, Log,
			TEXT("ATribeManager::CompleteQuest: Cannot afford quest requirements."));
		return false;
	}

	// Log current inventory vs. quest requirements so it is easy to verify
	// that the affordability check passed correctly.
	{
		FString InventoryStr;
		FString RequirementsStr;
		for (const TPair<FName, int32>& Req : Quest->Requirements)
		{
			InventoryStr    += FString::Printf(TEXT("%s=%d "), *Req.Key.ToString(), GetResourceAmount(Req.Key));
			RequirementsStr += FString::Printf(TEXT("%s=%d "), *Req.Key.ToString(), Req.Value);
		}
		UE_LOG(LogTemp, Log,
			TEXT("ATribeManager::CompleteQuest: '%s' | Inventory: [%s] | Requirements: [%s]"),
			*GetName(),
			*InventoryStr.TrimEnd(),
			*RequirementsStr.TrimEnd());
	}

	// ── 3. Deduct resources ───────────────────────────────────────────────────
	if (!DeductResources(Quest->Requirements))
	{
		UE_LOG(LogTemp, Error,
			TEXT("ATribeManager::CompleteQuest: Resource deduction failed mid-way. Aborting."));
		return false;
	}

	// ── 4. Register completion — also removes the quest and spawns a replacement
	QuestManager->RegisterCompletion(TribeActor, Quest);

	UE_LOG(LogTemp, Log, TEXT("ATribeManager::CompleteQuest: Quest completed by tribe '%s'."),
		TribeActor ? *TribeActor->GetName() : TEXT("<null>"));

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Trade helpers (instance methods)
// ─────────────────────────────────────────────────────────────────────────────

FQuestNeeds ATribeManager::FilterActiveQuestNeeds(const TArray<UQuest*>& Quests) const
{
	FQuestNeeds Needs = GetQuestResourceNeeds();
	Needs.RemoveAll([&Quests](const TPair<UQuest*, TArray<TPair<FName, int32>>>& Entry)
	{
		return !Quests.Contains(Entry.Key);
	});
	return Needs;
}

void ATribeManager::ComputeSurplusAndDeficit(
	const FQuestNeeds&    QuestNeeds,
	TMap<FName, int32>&   OutSurplus,
	TMap<FName, int32>&   OutDeficit) const
{
	// Aggregate each quest's full Requirements into a per-type total.
	// Using Requirements (not just the Missing amounts from QuestNeeds) ensures
	// that two quests sharing the same resource type are both protected.
	TMap<FName, int32> TotalRequired;
	for (const TPair<UQuest*, TArray<TPair<FName, int32>>>& Entry : QuestNeeds)
	{
		const UQuest* Quest = Entry.Key;
		if (!Quest) continue;
		for (const TPair<FName, int32>& Req : Quest->Requirements)
		{
			TotalRequired.FindOrAdd(Req.Key) += Req.Value;
		}
	}

	OutSurplus.Empty();
	OutDeficit.Empty();
	for (const TPair<FName, int32>& Req : TotalRequired)
	{
		const int32 Have = GetResourceAmount(Req.Key);
		OutSurplus.Add(Req.Key, FMath::Max(0, Have - Req.Value));
		OutDeficit.Add(Req.Key,  FMath::Max(0, Req.Value - Have));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// TradeOffer factory
// ─────────────────────────────────────────────────────────────────────────────

void ATribeManager::CreateTradeOfferForQuests(const TArray<UQuest*>& Quests)
{
	
	if (Quests.IsEmpty()) return;

	// ── 1. Derive Surplus and Deficit from the given quests ───────────────────
	const FQuestNeeds QuestNeeds = FilterActiveQuestNeeds(Quests);
	if (QuestNeeds.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("ATribeManager::CreateTradeOfferForQuests: "
			     "None of the supplied quests are active in QuestManager."));
		return;
	}

	TMap<FName, int32> Surplus, Deficit;
	ComputeSurplusAndDeficit(QuestNeeds, Surplus, Deficit);

	// Offered   = resource types where this tribe has surplus above quest needs.
	// Requested = resource types this tribe still needs to gather (deficit).
	TArray<TPair<FName, int32>> OfferedItems;
	for (const TPair<FName, int32>& S : Surplus)
		if (S.Value > 0) OfferedItems.Add(S);

	TArray<TPair<FName, int32>> RequestedItems;
	for (const TPair<FName, int32>& D : Deficit)
		if (D.Value > 0) RequestedItems.Add(D);

	if (OfferedItems.IsEmpty())
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("ATribeManager::CreateTradeOfferForQuests: "
			     "No surplus to offer — no offers created."));
		return;
	}

	// ── Log inventory and offer content (once, before broadcast) ────────────
	{
		// Inventory: current stock for every resource type appearing in the offer.
		FString InventoryStr;
		TSet<FName> Seen;
		auto AppendStock = [&](const FName& Type)
		{
			if (!Seen.Contains(Type))
			{
				InventoryStr += FString::Printf(TEXT("%s=%d "), *Type.ToString(), GetResourceAmount(Type));
				Seen.Add(Type);
			}
		};
		for (const TPair<FName, int32>& Item : OfferedItems)   AppendStock(Item.Key);
		for (const TPair<FName, int32>& Item : RequestedItems) AppendStock(Item.Key);

		FString OfferedStr;
		for (const TPair<FName, int32>& Item : OfferedItems)
			OfferedStr += FString::Printf(TEXT("%s=%d "), *Item.Key.ToString(), Item.Value);

		FString RequestedStr;
		for (const TPair<FName, int32>& Item : RequestedItems)
			RequestedStr += FString::Printf(TEXT("%s=%d "), *Item.Key.ToString(), Item.Value);

		UE_LOG(LogTemp, Warning,
			TEXT("ATribeManager::CreateTradeOfferForQuests: '%s' | Inventory: [%s] | Offered: [%s] | Requested: [%s]"),
			*GetName(),
			*InventoryStr.TrimEnd(),
			*OfferedStr.TrimEnd(),
			*RequestedStr.TrimEnd());
	}

	// ── 2. Broadcast one offer to every other active TribeManager ─────────────
	// Each recipient gets its own UTradeOffer object (no sharing between them).
	// Stale weak-pointer entries are skipped silently.
	int32 CreatedCount = 0;
	for (const TWeakObjectPtr<ATribeManager>& WeakRecipient : AllTribeManagers)
	{
		ATribeManager* Recipient = WeakRecipient.Get();
		if (!Recipient || Recipient == this) continue;

		// Skip if this TribeManager already has a pending offer in the
		// recipient's inbox — avoid stacking duplicate offers.
		for (const TObjectPtr<UTradeOffer>& Existing : Recipient->ReceivedTradeOffers)
		{
			if (Existing)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("ATribeManager::CreateTradeOfferForQuests: "
					     "Skipping '%s' — a pending offer from '%s' already exists."),
					*Recipient->GetName(), *GetName());
			}
		}
		const bool bAlreadySent = Recipient->ReceivedTradeOffers.ContainsByPredicate(
			[this](const TObjectPtr<UTradeOffer>& Existing)
			{
				return Existing && Existing->Sender == this;
			});

		if (bAlreadySent)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("ATribeManager::CreateTradeOfferForQuests: "
				     "Skipping '%s' — a pending offer from '%s' already exists."),
				*Recipient->GetName(), *GetName());
			continue;
		}

		UTradeOffer* Offer = NewObject<UTradeOffer>(this);
		Offer->Sender    = this;
		Offer->Recipient = Recipient;
		Offer->Offered   = OfferedItems;    // value-copy: each recipient owns its object
		Offer->Requested = RequestedItems;

		Recipient->ReceivedTradeOffers.Add(Offer);
		++CreatedCount;

		UE_LOG(LogTemp, Warning,
			TEXT("ATribeManager::CreateTradeOfferForQuests: "
			     "Offer sent from '%s' to '%s'."),
			*GetName(), *Recipient->GetName());
	}

	if (CreatedCount == 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("ATribeManager::CreateTradeOfferForQuests: "
			     "No other active TribeManagers found in AllTribeManagers."));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Trade-offer execution for quest resource gathering
// ─────────────────────────────────────────────────────────────────────────────

void ATribeManager::ExecuteTradeOffersForQuests(const TArray<UQuest*>& Quests)
{
	if (ReceivedTradeOffers.IsEmpty() || Quests.IsEmpty()) return;

	const FQuestNeeds QuestNeeds = FilterActiveQuestNeeds(Quests);
	if (QuestNeeds.IsEmpty())
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("ATribeManager::ExecuteTradeOffersForQuests: "
			     "None of the supplied quests are active in QuestManager."));
		return;
	}

	// Surplus / Deficit are kept mutable: updated after each exchange so that
	// every subsequent offer is evaluated against the current inventory state.
	TMap<FName, int32> Surplus, Deficit;
	ComputeSurplusAndDeficit(QuestNeeds, Surplus, Deficit);

	for (UTradeOffer* Offer : ReceivedTradeOffers)
	{
		if (!Offer || !Offer->Sender) continue;

		// What we want to receive: Offered items capped by our Deficit.
		const TArray<TPair<FName, int32>> WantItems = FilterByBudget(Offer->Offered, Deficit);
		if (WantItems.IsEmpty()) continue;

		// What we can give in return: Requested items capped by our Surplus.
		const TArray<TPair<FName, int32>> CanGiveItems = FilterByBudget(Offer->Requested, Surplus);
		if (CanGiveItems.IsEmpty() && Offer->Requested.Num() > 0)
		{
			UE_LOG(LogTemp, Log,
				TEXT("ATribeManager::ExecuteTradeOffersForQuests: "
				     "Skipping offer from '%s' — no surplus to give."),
				*Offer->Sender->GetName());
			continue;
		}

		// Balance to equal totals: sum(SenderGives) == sum(SenderTakes).
		TArray<TPair<FName, int32>> SenderGives, SenderTakes;
		if (!BalanceExchangeItems(WantItems, CanGiveItems, SenderGives, SenderTakes)) continue;

		if (Offer->Exchange(SenderGives, SenderTakes))
		{
			int32 ExchangeTotal = 0;
			for (const TPair<FName, int32>& P : SenderGives) ExchangeTotal += P.Value;

			UE_LOG(LogTemp, Log,
				TEXT("ATribeManager::ExecuteTradeOffersForQuests: "
				     "Exchange with '%s' completed (%d units each side)."),
				*Offer->Sender->GetName(), ExchangeTotal);

			for (const TPair<FName, int32>& Given : SenderTakes)
			{
				int32& S = Surplus.FindOrAdd(Given.Key);
				S = FMath::Max(0, S - Given.Value);
			}
			for (const TPair<FName, int32>& Received : SenderGives)
			{
				int32& D = Deficit.FindOrAdd(Received.Key);
				D = FMath::Max(0, D - Received.Value);
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("ATribeManager::ExecuteTradeOffersForQuests: "
				     "Exchange with '%s' failed."),
				*Offer->Sender->GetName());
		}
	}
}

bool ATribeManager::CanAfford(TArray<TPair<FName, int32>> Costs) const
{
	for (const TPair<FName, int32>& Cost : Costs)
	{
		const int32 Available = GetResourceAmount(Cost.Key);
		if (Available < Cost.Value)
		{
			return false;
		}
	}
	return true;
}

bool ATribeManager::DeductResources(TArray<TPair<FName, int32>> Costs)
{
	for (const TPair<FName, int32>& Cost : Costs)
	{
		if (!DeductResourceAmount(Cost.Key, Cost.Value))
		{
			UE_LOG(LogTemp, Error,
				TEXT("ATribeManager::DeductResources: Failed to deduct %d of '%s'."),
				Cost.Value, *Cost.Key.ToString());
			return false;
		}
	}
	return true;
}

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

ACharacter* ATribeManager::CreateTribeMan()
{
	// ── 1. Class check ────────────────────────────────────────────────────────
	if (!TribeManClass)
	{
		UE_LOG(LogTemp, Error, TEXT("ATribeManager::CreateTribeMan: TribeManClass is not set."));
		return nullptr;
	}

	// ── 2. Price lookup ───────────────────────────────────────────────────────
	TArray<TPair<FName, int32>> Costs = GetItemPrice(TribeManClass);
	if (Costs.Num() == 0)
	{
		UE_LOG(LogTemp, Error,
			TEXT("ATribeManager::CreateTribeMan: No price entry found for '%s'. Aborted."),
			*TribeManClass->GetName());
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
			TEXT("ATribeManager::CreateTribeMan: No free spawn location found near any storage."));
		return nullptr;
	}

	// ── 5. Deduct resources ───────────────────────────────────────────────────
	if (!DeductResources(Costs))
	{
		UE_LOG(LogTemp, Error,
			TEXT("ATribeManager::CreateTribeMan: Failed to deduct resources. Aborted."));
		return nullptr;
	}

	// ── 6. Spawn via UTribeGenerator::SpawnTribeMen (Count=1, Radius=0) ───────
	// No heightmap — SpawnLocation.Z is already on the navmesh surface.
	TArray<ACharacter*> Spawned = UTribeGenerator::SpawnTribeMen(
		GetWorld(), this, SpawnLocation,
		TribeManClass,
		/*Count=*/1, /*SpawnRadius=*/0.f,
		TribeActor.Get(), TribeFolderPath);
	// Note: SpawnTribeMen already calls tribeMen.AddUnique for each spawned character.

	if (Spawned.IsEmpty())
	{
		UE_LOG(LogTemp, Error,
			TEXT("ATribeManager::CreateTribeMan: UTribeGenerator::SpawnTribeMen failed for '%s'."),
			*TribeManClass->GetName());
		return nullptr;
	}

	return Spawned[0];
}

bool ATribeManager::FindFreeTribeManSpawnLocation(FVector& OutLocation) const
{
	UWorld* World = GetWorld();
	if (!World) return false;

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys) return false;

	if (storages.IsEmpty()) return false;

	// Target spawn distance from the storage (~10 m = 1000 cm).
	static constexpr float SpawnRadius   = 1000.f;
	// Half-width of the nav query box (Z is large to handle uneven terrain).
	static constexpr float ClearRadius   = 100.f;   // 1 m — tight enough for a character
	const FVector NavExtent(200.f, 200.f, 5000.f);

	// Object types to check for overlap (avoid spawning inside/on top of other actors).
	const TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes =
	{
		UEngineTypes::ConvertToObjectType(ECC_WorldStatic),
		UEngineTypes::ConvertToObjectType(ECC_WorldDynamic),
		UEngineTypes::ConvertToObjectType(ECC_Pawn),
	};
	const TArray<AActor*> IgnoreActors = { const_cast<ATribeManager*>(this) };

	static constexpr int32 MaxAttempts = 20;

	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		// Pick a random storage as the anchor point.
		const AActor* Storage = storages[FMath::RandRange(0, storages.Num() - 1)].Get();
		if (!Storage) continue;

		// Random direction at the target radius around the storage.
		const float Angle = FMath::FRandRange(0.f, 2.f * PI);
		const FVector Candidate(
			Storage->GetActorLocation().X + FMath::Cos(Angle) * SpawnRadius,
			Storage->GetActorLocation().Y + FMath::Sin(Angle) * SpawnRadius,
			Storage->GetActorLocation().Z + 5000.f);   // elevate so projection snaps down

		// Project onto the navigation mesh.
		FNavLocation NavLoc;
		if (!NavSys->ProjectPointToNavigation(Candidate, NavLoc, NavExtent)) continue;

		// Overlap check — raise slightly off the surface to avoid landscape hits.
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
		TEXT("ATribeManager::FindFreeTribeManSpawnLocation: No free spot found after %d attempts."),
		MaxAttempts);
	return false;
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
	calculateStorageResourcePathLengths();  // refresh nearbyResources and storageResourcePaths after the new storage is built
}

AActor* ATribeManager::Build(TSubclassOf<AActor> BuildingClass, FVector Location)
{
	// ── 1. Resolve the price object ───────────────────────────────────────────
	TArray<TPair<FName, int32>> Costs = GetItemPrice(BuildingClass);
	if (Costs.Num() == 0)
	{
		UE_LOG(LogTemp, Error,
			TEXT("ATribeManager::Build: No price entry found for '%s'. Build aborted."),
			*BuildingClass->GetName());
		return nullptr;
	}

	if (!CanAfford(Costs))
	{
		return nullptr;
	}

	// ── 4. Deduct resources ───────────────────────────────────────────────
	if (!DeductResources(Costs))
	{
		UE_LOG(LogTemp, Error,
			TEXT("ATribeManager::Build: Failed to deduct resources for '%s'. Build aborted."),
			*BuildingClass->GetName());
		return nullptr;
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
	// Normal mode: iterate storageResourcePaths in order (shortest path first).
	// Each entry is consumed at most once per tick via OccupiedResources.
	int32 PathIndex = 0;
	TObjectPtr<AActor> NearbyRaspBerry = GetFirstNearbyRaspBerry();

	for (ACharacter* TribeMan : tribeMen)
	{
		if (!TribeMan) continue;
		if (tribeManTasks.Contains(TribeMan)) continue;

		if (nearbyResources.Num() > OccupiedResources.Num())
		{
			if (NearbyRaspBerry != nullptr)
			{
				collectResource(TribeMan, NearbyRaspBerry.Get());
				NearbyRaspBerry = GetFirstNearbyRaspBerry();
			}
			else
			{
				// Find the next free resource (normal shortest-path order).
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
		
	}
}

TObjectPtr<AActor> ATribeManager::GetFirstNearbyRaspBerry() const
{
	for (const TObjectPtr<AActor>& NearbyResource : nearbyResources)
	{
		if (!NearbyResource) continue;
		if (OccupiedResources.Contains(NearbyResource)) continue;

		const FByteProperty* TypeProp =
			FindFProperty<FByteProperty>(NearbyResource->GetClass(), ResourceTypePropertyName);
		if (!TypeProp || !TypeProp->Enum) continue;

		const uint8 EnumVal = TypeProp->GetPropertyValue_InContainer(NearbyResource.Get());
		const FName TypeName(TypeProp->Enum->GetAuthoredNameStringByValue(
			static_cast<int64>(EnumVal)));

		if (TypeName == RaspBerryResourceTypeName)
		{
			return NearbyResource;
		}
	}
	return nullptr;
}

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
				OccupiedResources.Remove(TWeakObjectPtr<AActor>(TargetRes));
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
				OccupiedResources.Remove(TWeakObjectPtr<AActor>(TargetRes));
			}
			tribeManTasks.Remove(TribeMan);
			TribeManResumeFailures.Remove(TribeMan);
			RescueTribeManToNavmesh(TribeMan);
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

	static constexpr float SearchRadius = 5000.f;

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

	if (tribeManTasks.Contains(Cast<ACharacter>(TribeMan)))
	{
		UE_LOG(LogTemp, Warning, TEXT("TribeManager: Attempted to assign a resource to a busy tribeman '%s'."), *TribeMan->GetName());
		return;
	}

	if (OccupiedResources.Contains(TWeakObjectPtr<AActor>(Resource)))
	{
		UE_LOG(LogTemp, Warning, TEXT("TribeManager: Attempted to assign an already occupied resource '%s'."), *Resource->GetName());
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
				UE_LOG(LogTemp, Log, TEXT("TribeManager::collectResource: Snapping '%s' to navmesh (was %.1f cm off)."),
					*TribeMan->GetName(), FMath::Sqrt(OffNavmeshDistSq));
				TribeMan->SetActorLocation(NavLoc.Location, false, nullptr, ETeleportType::TeleportPhysics);
			}
		}
	}

	TSharedPtr<ResourceGatheringTask> resourceTask = MakeShared<ResourceGatheringTask>(Cast<ACharacter>(TribeMan), Resource);
	resourceTask->Execute();
	tribeManTasks.Add(Cast<ACharacter>(TribeMan), resourceTask);
	OccupiedResources.Add(TWeakObjectPtr<AActor>(Resource));
}
