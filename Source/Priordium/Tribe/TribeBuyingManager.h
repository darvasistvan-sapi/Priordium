// Copyright Priordium. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TribeBuyingManager.generated.h"

class ATribeManager;
class ACharacter;

/**
 * Actor component that encapsulates all purchasing logic for a tribe:
 * price lookup, affordability checks, resource deduction, building
 * construction, and TribeMan recruitment.
 *
 * Attach to ATribeManager (created via CreateDefaultSubobject in its constructor).
 * Spawning, location scouting, and navmesh queries delegate back to the owning
 * ATribeManager via GetOwnerManager().
 */
UCLASS(ClassGroup=(Tribe), meta=(BlueprintSpawnableComponent))
class PRIORDIUM_API UTribeBuyingManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UTribeBuyingManager();

	// ─────────────────────────────────────────────────────────────────────────
	// Properties
	// ─────────────────────────────────────────────────────────────────────────

	/**
	 * The BP_ItemPrices Blueprint class.
	 * An instance is created at BeginPlay; all GetItemPrice() lookups use it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buying")
	TSubclassOf<UObject> ItemPrices;

	/**
	 * Name of the TMap<TSubclassOf<AActor>, BP_ItemPrice_C> property on BP_ItemPrices.
	 * Must match the Blueprint variable name exactly (default: "Prices").
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buying")
	FName ItemPricesPricesPropertyName = FName(TEXT("Prices"));

	// ─────────────────────────────────────────────────────────────────────────
	// Initialisation
	// ─────────────────────────────────────────────────────────────────────────

	virtual void BeginPlay() override;

	// ─────────────────────────────────────────────────────────────────────────
	// Public interface
	// ─────────────────────────────────────────────────────────────────────────

	/**
	 * Looks up the cost for ItemClass in the ItemPrices object.
	 * Returns a flat list of (ResourceTypeName, Amount) pairs, or an empty
	 * array if the price is not found or the property layout is unexpected.
	 */
	TArray<TPair<FName, int32>> GetItemPrice(TSubclassOf<AActor> ItemClass) const;

	/**
	 * Returns true if the tribe can afford every (ResourceType, Amount) pair
	 * in Costs (checked against the owning TribeManager's storages).
	 */
	bool CanAfford(const TArray<TPair<FName, int32>>& Costs) const;

	/**
	 * Deducts each (ResourceType, Amount) in Costs from the owning
	 * TribeManager's storages in order.
	 * Returns true only if every deduction succeeded.
	 */
	bool DeductResources(const TArray<TPair<FName, int32>>& Costs);

	/**
	 * Spawns a building of the given class at Location, snaps it to the terrain,
	 * optionally flattens the landscape, and registers it with the tribe's storages.
	 * Deducts the construction cost via GetItemPrice first; returns nullptr if the
	 * price is not found or there are insufficient resources.
	 */
	UFUNCTION(BlueprintCallable, Category = "Buying")
	AActor* Build(TSubclassOf<AActor> BuildingClass, FVector Location);

	/**
	 * Spawns a new TribeMan of the owning tribe's TribeManClass near a random
	 * storage (~10 m away) on a free navmesh position.
	 * Deducts the cost via GetItemPrice first.
	 * Returns the new ACharacter* on success, nullptr on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Buying")
	ACharacter* CreateTribeMan();

	/**
	 * If there is enough wood, finds a free build location near an existing
	 * storage and calls Build() to construct a new storage.
	 * Called every Manage() tick; the wood cost naturally limits the build rate.
	 */
	void TryBuildStorage();

private:
	// ─────────────────────────────────────────────────────────────────────────
	// Internal state
	// ─────────────────────────────────────────────────────────────────────────

	/** Runtime instance created from ItemPrices at BeginPlay. */
	UPROPERTY()
	TObjectPtr<UObject> ItemPricesInstance;

	// ─────────────────────────────────────────────────────────────────────────
	// Internal helpers
	// ─────────────────────────────────────────────────────────────────────────

	/** Returns the owning ATribeManager, or nullptr if the owner is not one. */
	ATribeManager* GetOwnerManager() const;

	/**
	 * Returns the raw BP_ItemPrice_C UObject* for ItemClass from ItemPricesInstance.
	 * Internal helper used by GetItemPrice().
	 */
	UObject* GetItemPriceObject(TSubclassOf<AActor> ItemClass) const;

	/** Deferred one-tick callback that calls calculatePrices() on ItemPricesInstance. */
	void CallCalculatePrices();

	/**
	 * Finds a random world position within 1 km of an existing storage that:
	 *   - lies on the navigation mesh, and
	 *   - has no overlapping actors within a 5 m clearance radius.
	 * Returns true and sets OutLocation on success.
	 */
	bool FindFreeBuildLocation(FVector& OutLocation) const;

	/**
	 * Finds a world position approximately 10 m from a randomly chosen storage
	 * that lies on the navigation mesh and has no overlapping actors within a
	 * 1 m clearance radius (suitable for spawning a character).
	 * Returns true and sets OutLocation on success.
	 */
	bool FindFreeTribeManSpawnLocation(FVector& OutLocation) const;
};
