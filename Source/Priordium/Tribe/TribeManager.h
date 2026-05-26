// Copyright Priordium. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TribeManager.generated.h"

class ACharacter;
class ANavigationData;
class UNavigationSystemV1;
class TribeTask;
class UHeightmapGenerator;
class UMapGeneratorSettings;
class ULandscapeBuilder;
class AQuestManager;
class UQuest;
class UTradeOffer;

/** A storage–resource pair with the navmesh path length between them. */
struct FStorageResourcePath
{
	TWeakObjectPtr<AActor> Storage;
	TWeakObjectPtr<AActor> Resource;
	float PathLength = 0.f;
};

/**
 * One entry in the sorted result of FindLocationsWithResources().
 * Holds the candidate world position, a per-type resource breakdown,
 * and the pre-computed total so sorting does not need to re-sum.
 */
USTRUCT(BlueprintType)
struct FResourceLocationCandidate
{
	GENERATED_BODY()

	/** World position of the grid point. */
	UPROPERTY(BlueprintReadOnly, Category = "Tribe")
	FVector Location = FVector::ZeroVector;

	/** Number of resource actors per resource type near this location. */
	UPROPERTY(BlueprintReadOnly, Category = "Tribe")
	TMap<FName, int32> Resources;

	/** Sum of all values in Resources — used for sorting. */
	UPROPERTY(BlueprintReadOnly, Category = "Tribe")
	int32 TotalCount = 0;
};

UCLASS()
class PRIORDIUM_API ATribeManager : public AActor
{
	GENERATED_BODY()

public:
	ATribeManager();

	/**
	 * Registry of every ATribeManager spawned in the current world.
	 * Populated in BeginPlay, cleaned up in EndPlay.
	 * TWeakObjectPtr is used so the static array never prevents GC:
	 * destroyed entries silently become invalid and are filtered on use.
	 */
	static TArray<TWeakObjectPtr<ATribeManager>> AllTribeManagers;

	// BP_TribeMan instances (ACharacter-derived)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tribe")
	TArray<TObjectPtr<ACharacter>> tribeMen;

	// BP_Storage instances
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tribe")
	TArray<TObjectPtr<AActor>> storages;

	// BP_Storage class - assign BP_Storage in the editor.
	// Used by GetAllWorldStorages() to find every storage actor in the level.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tribe")
	TSubclassOf<AActor> StorageClass;

	// BP_Resource class - assign BP_Resource in the editor
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tribe")
	TSubclassOf<AActor> resourceBaseClass;

	// BP_TribeMan class - assign in the editor
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tribe")
	TSubclassOf<ACharacter> TribeManClass;

	// Colour of this tribe – set by UTribeGenerator from the BP_Tribe TribeColor property.
	// Used by the HUD to tint each tribe's resource row.
	UPROPERTY(BlueprintReadWrite, Category = "Tribe")
	FLinearColor TribeColor = FLinearColor::White;

	/**
	 * Name of the TMap<E_ResourceType, int32> property on BP_Storage actors.
	 * Must match the Blueprint variable name exactly.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tribe")
	FName StorageInventoryPropertyName = FName(TEXT("StoredResources"));

	/**
	 * Name of the E_ResourceType property on BP_Resource actors.
	 * Must match the Blueprint variable name exactly.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tribe")
	FName ResourceTypePropertyName = FName(TEXT("Type"));

	/**
	 * Name of the int32 amount property on BP_Resource actors.
	 * CountResourcesNearLocation sums this value instead of counting actors.
	 * Must match the Blueprint variable name exactly.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tribe")
	FName ResourceAmountPropertyName = FName(TEXT("ResourceAmount"));

	/**
	 * Authored name of the wood / timber entry in E_ResourceType.
	 * Used by Build() to check and deduct the construction cost.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tribe")
	FName WoodResourceTypeName = FName(TEXT("Wood"));

	/**
	 * Authored name of the raspberry entry in E_ResourceType.
	 * When nearbyResources > occupiedResources, TribeMen are directed to
	 * collect only this resource type.  CountResourcesNearLocation also
	 * scales its amount by the nearbyResources / occupiedResources ratio
	 * for this type.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tribe")
	FName RaspBerryResourceTypeName = FName(TEXT("RaspBerry"));

	// -------------------------------------------------------------------------
	// Terrain references (set by UTribeGenerator after spawning)
	// Used by Build() for terrain snapping and landscape flattening.
	// -------------------------------------------------------------------------

	UPROPERTY()
	TObjectPtr<const UHeightmapGenerator> TerrainHeightmap;

	UPROPERTY()
	TObjectPtr<const UMapGeneratorSettings> TerrainSettings;

	UPROPERTY()
	TObjectPtr<ULandscapeBuilder> TerrainLandscapeBuilder;

	UPROPERTY()
	TArray<TObjectPtr<UTradeOffer>> ReceivedTradeOffers;

	UPROPERTY()
	TArray<TObjectPtr<UQuest>> TradeQuests;

	// The BP_Tribe actor that owns this manager.
	// Passed to SpawnBuilding() so new storages get their "Tribe" property set.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tribe")
	TObjectPtr<AActor> TribeActor;

	/**
	 * The shared QuestManager for all tribes.
	 * Set by UTribeGenerator after spawning so every tribe can read/register quests.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tribe")
	TObjectPtr<AQuestManager> QuestManager;

	/**
	 * The BP_ItemPrices Blueprint class. TribeManager creates one instance from it
	 * at BeginPlay and uses that instance for all GetItemPrice() lookups.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tribe")
	TSubclassOf<UObject> ItemPrices;

	/**
	 * Name of the TMap<TSubclassOf<AActor>, BP_ItemPrice_C> property on BP_ItemPrices.
	 * Must match the Blueprint variable name exactly (default: "Prices").
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tribe")
	FName ItemPricesPricesPropertyName = FName(TEXT("Prices"));

	// Outliner folder path for this tribe (e.g. "Tribes/Tribe_0").
	// Passed to SpawnBuilding() so newly built actors land in the correct folder.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tribe")
	FName TribeFolderPath;

	// -------------------------------------------------------------------------
	// Public methods
	// -------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Tribe")
	void CheckQuests();

	/**
	 * Spawns a building of the given class at Location, snaps it to the terrain,
	 * optionally flattens the landscape under it, and adds it to storages.
	 * Deducts the construction cost via GetItemPrice first; returns nullptr if
	 * the price is not found or there are insufficient resources.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tribe")
	AActor* Build(TSubclassOf<AActor> BuildingClass, FVector Location);

	/**
	 * Spawns a new TribeMan of TribeManClass near a random storage (≈ 10 m away)
	 * on a free navmesh position. Deducts the cost via GetItemPrice first.
	 * Returns the new ACharacter* on success, nullptr on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tribe")
	ACharacter* CreateTribeMan();

	/**
	 * Returns the total amount of the given resource across all storages.
	 * @param ResourceType  Authored enum entry name, e.g. FName("Wood").
	 */
	UFUNCTION(BlueprintCallable, Category = "Tribe")
	int32 GetResourceAmount(FName ResourceType) const;

	/**
	 * Counts all BP_Resource actors within 100 m of Location, grouped by resource type.
	 * Resources that lie within 100 m of any storage are excluded — they are already
	 * "covered" by an existing storage and should not be double-counted.
	 * Uses ResourceTypePropertyName to read the E_ResourceType enum from each resource actor.
	 *
	 * @param Location  World position to search around (XYZ, distance is 3-D).
	 * @return          Map of authored resource-type name → count of actors of that type.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tribe")
	TMap<FName, int32> CountResourcesNearLocation(FVector Location) const;

	/**
	 * Scans a 20 m grid around all existing storages and returns every valid candidate
	 * point sorted descending by total resource count (most resources first).
	 *
	 * Candidate points must satisfy both:
	 *   • within  200 m of at least one storage  (reachable from an existing base)
	 *   • at least 100 m from every storage       (not already served by an existing storage)
	 *
	 * Points with zero resources are included at the end of the list (TotalCount == 0).
	 * Returns an empty array if there are no storages or no valid grid points.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tribe")
	TArray<FResourceLocationCandidate> FindLocationsWithResources() const;

	/**
	 * Looks up the cost for ItemClass in the ItemPrices object and returns it as a
	 * flat list of (ResourceTypeName, Amount) pairs ready for CanAfford / DeductResources.
	 * Returns an empty array if ItemPrices is not set, the class has no entry, or the
	 * 'Resources' map property cannot be resolved.
	 *
	 * @param ItemClass  The building/item class to look up the price for.
	 * @return           Cost pairs, or empty on any failure.
	 */
	TArray<TPair<FName, int32>> GetItemPrice(TSubclassOf<AActor> ItemClass) const;

	/**
	 * For every quest in QuestManager->Quests, calculates the still-missing
	 * resource amounts (RequiredAmount - GetResourceAmount, floored at 0).
	 * Returns a list of (Quest, missing requirements) pairs sorted ascending
	 * by the total missing amount — the most affordable quest comes first.
	 * Requirements that are already fully satisfied are omitted from the inner array.
	 * Returns an empty array if QuestManager is not set or has no quests.
	 */
	TArray<TPair<UQuest*, TArray<TPair<FName, int32>>>> GetQuestResourceNeeds() const;

	/**
	 * Reviews RecievedTradeOffers and executes any exchange that helps gather
	 * resources for the given quests while guaranteeing:
	 *   1. No quest-reserved resource is traded away (only surplus is given).
	 *   2. Sent total == received total (1:1 quantity balance).
	 *
	 * Internally delegates to FilterActiveQuestNeeds, ComputeSurplusAndDeficit,
	 * FilterByBudget, and BalanceExchangeItems (see private helpers).
	 *
	 * @param Quests  Active quests whose resource needs must be protected.
	 */
	void ExecuteTradeOffersForQuests(const TArray<UQuest*>& Quests);

	/**
	 * Derives Surplus / Deficit from the given quests and broadcasts a
	 * UTradeOffer to every other tribe currently in AllTribeManagers.
	 *
	 * For each other ATribeManager:
	 *   Offered   = resource types where this tribe has a surplus above its
	 *               combined quest requirements (safe to give away).
	 *   Requested = resource types this tribe still needs to gather (deficit).
	 *
	 * Each offer is added to the recipient's RecievedTradeOffers array.
	 * Does nothing if there is no surplus and no deficit, or if none of the
	 * supplied quests are active in QuestManager.
	 *
	 * @param Quests  Active quests used to derive Surplus / Deficit.
	 */
	void CreateTradeOfferForQuests(const TArray<UQuest*>& Quests);

	/**
	 * Attempts to complete the given quest for this tribe.
	 *
	 * Steps:
	 *  1. Validates Quest and QuestManager are set.
	 *  2. Checks that the tribe can afford every requirement in Quest->Requirements
	 *     (summed across all storages via GetResourceAmount).
	 *  3. Deducts the required resources from storages.
	 *  4. Removes the quest from QuestManager->Quests.
	 *  5. Calls QuestManager->RegisterCompletion(TribeActor) to increment the counter.
	 *
	 * Returns true if the quest was successfully completed, false otherwise
	 * (insufficient resources, null quest, quest not in manager, etc.).
	 */
	UFUNCTION(BlueprintCallable, Category = "Tribe")
	bool CompleteQuest(UQuest* Quest);

	/**
	 * Adds Amount of ResourceType to the first storage that already contains
	 * that resource type key. Returns true on success.
	 * Used by trade exchange to credit incoming resources.
	 */
	bool AddResourceAmount(FName ResourceType, int32 Amount);

	/**
	 * Deducts Amount of ResourceType from storages (spread across multiple if needed).
	 * Returns true if the full amount was successfully deducted.
	 */
	bool DeductResourceAmount(FName ResourceType, int32 Amount);

	/** Main update entry point. Called every 10 seconds via timer. */
	void Manage();

	/** Removes stale references from all collections, then cleans dependent ones. */
	void CheckNullReferences();

	/** Populates storageResourcePathLengths via async navmesh path queries. */
	UFUNCTION(BlueprintCallable, Category = "Tribe")
	void calculateStorageResourcePathLengths();

	/**
	 * Flattens storageResourcePathLengths into storageResourcePaths,
	 * excluding occupied resources, and sorts ascending by path length.
	 */
	void calculateStorageResourcePaths();

	/** Assigns a ResourceGatheringTask to TribeMan for the given Resource. */
	void collectResource(AActor* TribeMan, AActor* Resource);

	TObjectPtr<AActor> GetFirstNearbyRaspBerry() const;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:


	// Instance created from ItemPrices class at BeginPlay.
	UPROPERTY()
	TObjectPtr<UObject> ItemPricesInstance;

	// TribeMan -> their current task
	TMap<TObjectPtr<ACharacter>, TSharedPtr<TribeTask>> tribeManTasks;

	// How many consecutive Manage() ticks a TribeMan stayed idle after Execute()
	// was called. Cleared when they start moving; task is dropped after threshold.
	TMap<TObjectPtr<ACharacter>, int32> TribeManResumeFailures;

	TSet<TWeakObjectPtr<AActor>> OccupiedResources;

	// All resources within search radius of any storage
	TArray<TObjectPtr<AActor>> nearbyResources;

	// storage -> (resource -> navmesh path length in cm)
	TMap<TWeakObjectPtr<AActor>, TMap<TWeakObjectPtr<AActor>, float>> storageResourcePathLengths;

	// Storage-resource pairs that are not occupied, sorted ascending by path length.
	// Populated by calculateStorageResourcePaths().
	TArray<FStorageResourcePath> storageResourcePaths;

	// Timer handle for the 10-second Manage tick
	FTimerHandle ManageTimerHandle;

	// Number of async path queries currently in flight.
	// When it reaches 0, calculateStorageResourcePaths() is called automatically.
	int32 PendingPathQueries = 0;

	// -------------------------------------------------------------------------
	// Internal path-length calculation
	// -------------------------------------------------------------------------

	void CalculateStorageResourcePathLengths(
		TWeakObjectPtr<AActor> Storage,
		UWorld* WorldContext,
		UNavigationSystemV1* NavigationSystem,
		const ANavigationData* NavigationData
	);
	void CalculateStorageResourcePathLength(
		TWeakObjectPtr<AActor> Storage,
		TWeakObjectPtr<AActor> Resource,
		UNavigationSystemV1* NavigationSystem,
		const ANavigationData* NavigationData
	);

	// -------------------------------------------------------------------------
	// Internal helpers
	// -------------------------------------------------------------------------

	/**
	 * Returns the raw BP_ItemPrice_C UObject* for ItemClass from ItemPricesInstance.
	 * Internal helper used by GetItemPrice().
	 */
	UObject* GetItemPriceObject(TSubclassOf<AActor> ItemClass) const;
	bool DeductResources(TArray<TPair<FName, int32>> Costs);
	bool CanAfford(TArray<TPair<FName, int32>> Costs) const;

	// ── Trade helpers ─────────────────────────────────────────────────────────

	/**
	 * Calls GetQuestResourceNeeds() and removes every entry whose quest pointer
	 * is not contained in Quests. Returns the filtered array.
	 */
	TArray<TPair<UQuest*, TArray<TPair<FName, int32>>>> FilterActiveQuestNeeds(
		const TArray<UQuest*>& Quests) const;

	/**
	 * From the given quest-needs array, aggregates each quest's Requirements into
	 * a per-type TotalRequired map, then derives:
	 *   OutSurplus[type] = max(0, GetResourceAmount(type) − TotalRequired[type])
	 *   OutDeficit[type] = max(0, TotalRequired[type] − GetResourceAmount(type))
	 *
	 * Both out-maps are cleared before writing.
	 */
	void ComputeSurplusAndDeficit(
		const TArray<TPair<UQuest*, TArray<TPair<FName, int32>>>>& QuestNeeds,
		TMap<FName, int32>& OutSurplus,
		TMap<FName, int32>& OutDeficit) const;

	/**
	 * If there is enough wood, finds a free build location near an existing
	 * storage and calls Build() to construct a new storage.
	 * Called every Manage() tick; the wood cost naturally limits the build rate.
	 */
	void TryBuildStorage();

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

	void calculateNearbyResources();

	/** Assigns a free resource from storageResourcePaths to each idle TribeMan. */
	void orderTribeMenToCollect();

	/**
	 * Iterates all TribeMen. For each one that has a task assigned in tribeManTasks
	 * but is currently idle (velocity ≈ 0), re-executes their task so they resume work
	 * after reaching a destination or being interrupted.
	 */
	void resumeIdleTribeManTasks();

	/** Deferred one-tick callback that calls calculatePrices() on ItemPricesInstance. */
	void CallCalculatePrices();

	/**
	 * Returns the world locations of all storage actors currently present in the world.
	 * Uses StorageClass (if set) via GetAllActorsOfClass; falls back to the
	 * class of the first valid entry in this->storages if StorageClass is null.
	 * Returns an empty array if neither source provides a class.
	 */
	TArray<FVector> GetAllWorldStorageLocations() const;

	ACharacter* getFreeTribeMan() const;
	AActor* getNearestResource(TSubclassOf<AActor> ResourceType) const;
	AActor* getStorageNearestResource(AActor* Storage, TSubclassOf<AActor> ResourceType) const;

	// -------------------------------------------------------------------------
	// Stale-reference checks (each returns true if at least one entry was removed)
	// -------------------------------------------------------------------------

	/**
	 * Projects TribeMan's current location onto the nearest navmesh point and
	 * issues a simple MoveToLocation so they escape positions with no navmesh
	 * (e.g. after being pushed off by another character's collision).
	 */
	void RescueTribeManToNavmesh(ACharacter* TribeMan);

	bool checkNearbyResources();
	bool checkOccupiedResources();
	bool checkTribeMen();
	bool checkStorages();
	bool checkTribeManTasks();
	bool checkStorageResourcePathLengths();
};
