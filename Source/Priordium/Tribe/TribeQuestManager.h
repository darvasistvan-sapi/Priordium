// Copyright Priordium. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TribeQuestManager.generated.h"

class ATribeManager;
class AQuestManager;
class UQuest;
class UTradeOffer;

/**
 * Actor component that encapsulates all quest- and trade-offer logic for a tribe.
 *
 * Attach to ATribeManager (created via CreateDefaultSubobject in its constructor).
 * The component delegates resource reads/writes back to the owning ATribeManager
 * via GetOwnerManager().
 */
UCLASS(ClassGroup=(Tribe), meta=(BlueprintSpawnableComponent))
class PRIORDIUM_API UTribeQuestManager : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Convenience alias used throughout this component. */
	using FQuestNeeds = TArray<TPair<UQuest*, TArray<TPair<FName, int32>>>>;

	UTribeQuestManager();

	// ─────────────────────────────────────────────────────────────────────────
	// Properties
	// ─────────────────────────────────────────────────────────────────────────

	/**
	 * The shared AQuestManager for all tribes.
	 * Assigned by UTribeGenerator after spawning.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	TObjectPtr<AQuestManager> QuestManager;

	/** Pending trade offers received from other tribes. Cleared every Manage() tick. */
	UPROPERTY()
	TArray<TObjectPtr<UTradeOffer>> ReceivedTradeOffers;

	/**
	 * Rolling window of quests used when broadcasting outgoing trade offers.
	 * Accumulates one quest per CheckQuests() call, then resets when all quests
	 * in QuestManager have been cycled through.
	 */
	UPROPERTY()
	TArray<TObjectPtr<UQuest>> TradeQuests;

	// ─────────────────────────────────────────────────────────────────────────
	// Public interface
	// ─────────────────────────────────────────────────────────────────────────

	/**
	 * Main quest tick: called every Manage() cycle.
	 *   1. Completes the most affordable quest if all resources are available.
	 *   2. Executes any received trade offers that help gather quest resources.
	 *   3. Broadcasts a new outgoing trade offer based on surplus / deficit.
	 */
	UFUNCTION(BlueprintCallable, Category = "Quest")
	void CheckQuests();

	/**
	 * Attempts to complete the given quest for the owning tribe.
	 *
	 * Steps: validate → affordability check → deduct resources →
	 *        QuestManager::RegisterCompletion().
	 *
	 * Returns true on success, false if the tribe cannot afford the quest or
	 * any argument is invalid.
	 */
	UFUNCTION(BlueprintCallable, Category = "Quest")
	bool CompleteQuest(UQuest* Quest);

	/**
	 * For every quest in QuestManager->Quests, calculates still-missing resource
	 * amounts (Required − Owned, floored at 0).
	 * Returns pairs sorted ascending by total missing amount (most affordable first).
	 * Returns an empty array if QuestManager is not set or has no quests.
	 */
	FQuestNeeds GetQuestResourceNeeds() const;

	/**
	 * Derives Surplus / Deficit from the given quests and broadcasts a UTradeOffer
	 * to every other tribe in ATribeManager::AllTribeManagers.
	 *
	 * Offered   = resource types where the owning tribe has surplus above quest needs.
	 * Requested = resource types the owning tribe still needs (deficit).
	 *
	 * Does nothing if Quests is empty, none of the quests are active, or there
	 * is no surplus to offer.
	 */
	void CreateTradeOfferForQuests(const TArray<UQuest*>& Quests);

	/**
	 * Reviews ReceivedTradeOffers and executes any exchange that helps gather
	 * resources for the given quests while guaranteeing:
	 *   1. No quest-reserved resource is traded away (only surplus is given).
	 *   2. Sent total == received total (1:1 quantity balance).
	 */
	void ExecuteTradeOffersForQuests(const TArray<UQuest*>& Quests);

private:
	// ─────────────────────────────────────────────────────────────────────────
	// Internal helpers
	// ─────────────────────────────────────────────────────────────────────────

	/** Returns the owning ATribeManager, or nullptr if the owner is not one. */
	ATribeManager* GetOwnerManager() const;

	/**
	 * Calls GetQuestResourceNeeds() and removes every entry whose quest pointer
	 * is not contained in Quests. Returns the filtered array.
	 */
	FQuestNeeds FilterActiveQuestNeeds(const TArray<UQuest*>& Quests) const;

	/**
	 * From the given quest-needs array, aggregates each quest's Requirements into
	 * a per-type TotalRequired map, then derives:
	 *   OutSurplus[type] = max(0, CurrentStock − TotalRequired[type])
	 *   OutDeficit[type] = max(0, TotalRequired[type] − CurrentStock)
	 *
	 * Both out-maps are cleared before writing.
	 */
	void ComputeSurplusAndDeficit(
		const FQuestNeeds&  QuestNeeds,
		TMap<FName, int32>& OutSurplus,
		TMap<FName, int32>& OutDeficit) const;
};
