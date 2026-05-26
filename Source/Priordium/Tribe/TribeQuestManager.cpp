// Copyright Priordium. All Rights Reserved.

#include "TribeQuestManager.h"

#include "TribeManager.h"
#include "Quest.h"
#include "QuestManager.h"
#include "TradeOffer.h"

// ─────────────────────────────────────────────────────────────────────────────
// File-local trade helpers
// (moved from TribeManager.cpp anonymous namespace)
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
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

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

UTribeQuestManager::UTribeQuestManager()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal: owner access
// ─────────────────────────────────────────────────────────────────────────────

ATribeManager* UTribeQuestManager::GetOwnerManager() const
{
	return Cast<ATribeManager>(GetOwner());
}

// ─────────────────────────────────────────────────────────────────────────────
// CheckQuests
// ─────────────────────────────────────────────────────────────────────────────

void UTribeQuestManager::CheckQuests()
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

	// ── Broadcast outgoing trade offers ──────────────────────────────────────
	if (!QuestManager || QuestManager->Quests.IsEmpty()) return;

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
}

// ─────────────────────────────────────────────────────────────────────────────
// GetQuestResourceNeeds
// ─────────────────────────────────────────────────────────────────────────────

UTribeQuestManager::FQuestNeeds UTribeQuestManager::GetQuestResourceNeeds() const
{
	FQuestNeeds Result;

	ATribeManager* Owner = GetOwnerManager();
	if (!Owner || !QuestManager) return Result;

	for (const TObjectPtr<UQuest>& QuestPtr : QuestManager->Quests)
	{
		UQuest* Quest = QuestPtr.Get();
		if (!Quest) continue;

		// Calculate still-missing amounts for each requirement.
		TArray<TPair<FName, int32>> Missing;
		int32 TotalMissing = 0;

		for (const TPair<FName, int32>& Req : Quest->Requirements)
		{
			const int32 Have    = Owner->GetResourceAmount(Req.Key);
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
	Result.Sort([](
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
// CompleteQuest
// ─────────────────────────────────────────────────────────────────────────────

bool UTribeQuestManager::CompleteQuest(UQuest* Quest)
{
	if (!Quest)
	{
		UE_LOG(LogTemp, Warning, TEXT("UTribeQuestManager::CompleteQuest: Quest is null."));
		return false;
	}

	if (!QuestManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("UTribeQuestManager::CompleteQuest: QuestManager is not set."));
		return false;
	}

	ATribeManager* Owner = GetOwnerManager();
	if (!Owner)
	{
		UE_LOG(LogTemp, Warning, TEXT("UTribeQuestManager::CompleteQuest: Owner ATribeManager is null."));
		return false;
	}

	// ── Affordability check ───────────────────────────────────────────────────
	if (!Owner->BuyingHandler || !Owner->BuyingHandler->CanAfford(Quest->Requirements))
	{
		UE_LOG(LogTemp, Log,
			TEXT("UTribeQuestManager::CompleteQuest: Cannot afford quest requirements."));
		return false;
	}

	// Log current inventory vs. requirements.
	{
		FString InventoryStr;
		FString RequirementsStr;
		for (const TPair<FName, int32>& Req : Quest->Requirements)
		{
			InventoryStr    += FString::Printf(TEXT("%s=%d "), *Req.Key.ToString(), Owner->GetResourceAmount(Req.Key));
			RequirementsStr += FString::Printf(TEXT("%s=%d "), *Req.Key.ToString(), Req.Value);
		}
		UE_LOG(LogTemp, Log,
			TEXT("UTribeQuestManager::CompleteQuest: '%s' | Inventory: [%s] | Requirements: [%s]"),
			*Owner->GetName(),
			*InventoryStr.TrimEnd(),
			*RequirementsStr.TrimEnd());
	}

	// ── Deduct resources ──────────────────────────────────────────────────────
	if (!Owner->BuyingHandler->DeductResources(Quest->Requirements))
	{
		UE_LOG(LogTemp, Error,
			TEXT("UTribeQuestManager::CompleteQuest: Resource deduction failed mid-way. Aborting."));
		return false;
	}

	// ── Register completion ───────────────────────────────────────────────────
	QuestManager->RegisterCompletion(Owner->TribeActor, Quest);

	UE_LOG(LogTemp, Log, TEXT("UTribeQuestManager::CompleteQuest: Quest completed by tribe '%s'."),
		Owner->TribeActor ? *Owner->TribeActor->GetName() : TEXT("<null>"));

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// FilterActiveQuestNeeds
// ─────────────────────────────────────────────────────────────────────────────

UTribeQuestManager::FQuestNeeds UTribeQuestManager::FilterActiveQuestNeeds(
	const TArray<UQuest*>& Quests) const
{
	FQuestNeeds Needs = GetQuestResourceNeeds();
	Needs.RemoveAll([&Quests](const TPair<UQuest*, TArray<TPair<FName, int32>>>& Entry)
	{
		return !Quests.Contains(Entry.Key);
	});
	return Needs;
}

// ─────────────────────────────────────────────────────────────────────────────
// ComputeSurplusAndDeficit
// ─────────────────────────────────────────────────────────────────────────────

void UTribeQuestManager::ComputeSurplusAndDeficit(
	const FQuestNeeds&  QuestNeeds,
	TMap<FName, int32>& OutSurplus,
	TMap<FName, int32>& OutDeficit) const
{
	ATribeManager* Owner = GetOwnerManager();

	// Aggregate each quest's full Requirements into a per-type total.
	// Using Requirements (not just Missing) ensures two quests sharing a resource
	// type are both fully protected.
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

	if (!Owner) return;

	for (const TPair<FName, int32>& Req : TotalRequired)
	{
		const int32 Have = Owner->GetResourceAmount(Req.Key);
		OutSurplus.Add(Req.Key, FMath::Max(0, Have - Req.Value));
		OutDeficit.Add(Req.Key, FMath::Max(0, Req.Value - Have));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// CreateTradeOfferForQuests
// ─────────────────────────────────────────────────────────────────────────────

void UTribeQuestManager::CreateTradeOfferForQuests(const TArray<UQuest*>& Quests)
{
	if (Quests.IsEmpty()) return;

	ATribeManager* Owner = GetOwnerManager();
	if (!Owner) return;

	// ── 1. Derive Surplus and Deficit from the given quests ───────────────────
	const FQuestNeeds QuestNeeds = FilterActiveQuestNeeds(Quests);
	if (QuestNeeds.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UTribeQuestManager::CreateTradeOfferForQuests: "
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
			TEXT("UTribeQuestManager::CreateTradeOfferForQuests: "
			     "No surplus to offer — no offers created."));
		return;
	}

	// ── Log inventory and offer content (once, before broadcast) ─────────────
	{
		FString InventoryStr;
		TSet<FName> Seen;
		auto AppendStock = [&](const FName& Type)
		{
			if (!Seen.Contains(Type))
			{
				InventoryStr += FString::Printf(TEXT("%s=%d "), *Type.ToString(), Owner->GetResourceAmount(Type));
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
			TEXT("UTribeQuestManager::CreateTradeOfferForQuests: '%s' | Inventory: [%s] | Offered: [%s] | Requested: [%s]"),
			*Owner->GetName(),
			*InventoryStr.TrimEnd(),
			*OfferedStr.TrimEnd(),
			*RequestedStr.TrimEnd());
	}

	// ── 2. Broadcast one offer to every other active TribeManager ─────────────
	int32 CreatedCount = 0;
	for (const TWeakObjectPtr<ATribeManager>& WeakRecipient : ATribeManager::AllTribeManagers)
	{
		ATribeManager* Recipient = WeakRecipient.Get();
		if (!Recipient || Recipient == Owner) continue;

		// Skip if this tribe already has a pending offer in the recipient's inbox.
		const bool bAlreadySent = Recipient->QuestHandler->ReceivedTradeOffers.ContainsByPredicate(
			[Owner](const TObjectPtr<UTradeOffer>& Existing)
			{
				return Existing && Existing->Sender == Owner;
			});

		if (bAlreadySent)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("UTribeQuestManager::CreateTradeOfferForQuests: "
				     "Skipping '%s' — a pending offer from '%s' already exists."),
				*Recipient->GetName(), *Owner->GetName());
			continue;
		}

		UTradeOffer* Offer = NewObject<UTradeOffer>(Owner);
		Offer->Sender    = Owner;
		Offer->Recipient = Recipient;
		Offer->Offered   = OfferedItems;    // value-copy: each recipient owns its object
		Offer->Requested = RequestedItems;

		Recipient->QuestHandler->ReceivedTradeOffers.Add(Offer);
		++CreatedCount;

		UE_LOG(LogTemp, Warning,
			TEXT("UTribeQuestManager::CreateTradeOfferForQuests: "
			     "Offer sent from '%s' to '%s'."),
			*Owner->GetName(), *Recipient->GetName());
	}

	if (CreatedCount == 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UTribeQuestManager::CreateTradeOfferForQuests: "
			     "No other active TribeManagers found in AllTribeManagers."));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// ExecuteTradeOffersForQuests
// ─────────────────────────────────────────────────────────────────────────────

void UTribeQuestManager::ExecuteTradeOffersForQuests(const TArray<UQuest*>& Quests)
{
	if (ReceivedTradeOffers.IsEmpty() || Quests.IsEmpty()) return;

	const FQuestNeeds QuestNeeds = FilterActiveQuestNeeds(Quests);
	if (QuestNeeds.IsEmpty())
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("UTribeQuestManager::ExecuteTradeOffersForQuests: "
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
				TEXT("UTribeQuestManager::ExecuteTradeOffersForQuests: "
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
				TEXT("UTribeQuestManager::ExecuteTradeOffersForQuests: "
				     "Exchange with '%s' completed (%d units each side)."),
				*Offer->Sender->GetName(), ExchangeTotal);

			// Update local Surplus/Deficit to reflect the completed exchange.
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
				TEXT("UTribeQuestManager::ExecuteTradeOffersForQuests: "
				     "Exchange with '%s' failed."),
				*Offer->Sender->GetName());
		}
	}
}
