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

	// BP_TribeMan instances (ACharacter-derived)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tribe")
	TArray<TObjectPtr<ACharacter>> tribeMen;

	// BP_Storage instances
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tribe")
	TArray<TObjectPtr<AActor>> storages;

	// BP_Resource class - assign BP_Resource in the editor
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tribe")
	TSubclassOf<AActor> resourceBaseClass;

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

	// The BP_Tribe actor that owns this manager.
	// Passed to SpawnBuilding() so new storages get their "Tribe" property set.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tribe")
	TObjectPtr<AActor> TribeActor;

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

	/**
	 * Spawns a building of the given class at Location, snaps it to the terrain,
	 * optionally flattens the landscape under it, and adds it to storages.
	 * Deducts the wood construction cost first; returns nullptr if insufficient.
	 * Uses the same logic as UTribeGenerator::SpawnBuilding.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tribe")
	AActor* Build(TSubclassOf<AActor> BuildingClass, FVector Location);

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
	 * Looks up and returns the BP_ItemPrice_C object for the given building class
	 * from the ItemPrices object (BP_ItemPrices). Uses UE property reflection to
	 * read the TMap<TSubclassOf<AActor>, BP_ItemPrice_C> named ItemPricesPricesPropertyName.
	 * Returns nullptr if ItemPrices is not set, the class is not found, or the map
	 * property cannot be resolved.
	 *
	 * @param ItemClass  The building/item class to look up the price for.
	 * @return           The BP_ItemPrice_C UObject for that class, or nullptr.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tribe")
	UObject* GetItemPrice(TSubclassOf<AActor> ItemClass) const;

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

protected:
	virtual void BeginPlay() override;

private:


	// Instance created from ItemPrices class at BeginPlay.
	UPROPERTY()
	TObjectPtr<UObject> ItemPricesInstance;

	// TribeMan -> their current task
	TMap<TObjectPtr<ACharacter>, TSharedPtr<TribeTask>> tribeManTasks;

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
	 * Deducts Amount of ResourceType from storages (spread across multiple if needed).
	 * Returns true if the full amount was successfully deducted.
	 */
	bool DeductResourceAmount(FName ResourceType, int32 Amount);

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

	ACharacter* getFreeTribeMan() const;
	AActor* getNearestResource(TSubclassOf<AActor> ResourceType) const;
	AActor* getStorageNearestResource(AActor* Storage, TSubclassOf<AActor> ResourceType) const;

	// -------------------------------------------------------------------------
	// Stale-reference checks (each returns true if at least one entry was removed)
	// -------------------------------------------------------------------------

	bool checkNearbyResources();
	bool checkOccupiedResources();
	bool checkTribeMen();
	bool checkStorages();
	bool checkTribeManTasks();
	bool checkStorageResourcePathLengths();
};
