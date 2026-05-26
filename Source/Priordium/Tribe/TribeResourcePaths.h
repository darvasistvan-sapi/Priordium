// Copyright Priordium. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TribeResourcePaths.generated.h"

class ACharacter;
class ANavigationData;
class UNavigationSystemV1;
class ATribeManager;
class TribeTask;

/**
 * A storage–resource pair with the navmesh path length between them.
 * Used internally by UTribeResourcePaths to maintain the sorted assignment queue.
 */
struct FStorageResourcePath
{
	TWeakObjectPtr<AActor> Storage;
	TWeakObjectPtr<AActor> Resource;
	float PathLength = 0.f;
};

/**
 * Actor component that owns all resource-path state for a tribe:
 *   • nearby resource census      (NearbyResources)
 *   • async navmesh path lengths   (StorageResourcePathLengths)
 *   • sorted free-path list        (StorageResourcePaths)
 *   • occupied-resource tracking   (OccupiedResources)
 *   • resource collection dispatch (OrderTribeMenToCollect / CollectResource)
 *   • stale-reference cleanup      (CheckNullReferences)
 *
 * Attach to ATribeManager (created via CreateDefaultSubobject in its constructor).
 * Methods that need TribeManager state delegate back via GetOwnerManager().
 */
UCLASS(ClassGroup=(Tribe), meta=(BlueprintSpawnableComponent))
class PRIORDIUM_API UTribeResourcePaths : public UActorComponent
{
	GENERATED_BODY()

public:
	UTribeResourcePaths();

	// ─────────────────────────────────────────────────────────────────────────
	// Public entry points
	// ─────────────────────────────────────────────────────────────────────────

	/**
	 * Recalculates the nearby-resource list and kicks off async navmesh path
	 * queries for every storage → resource pair.
	 * CalculateStorageResourcePaths() is invoked automatically once all
	 * callbacks have fired (PendingPathQueries reaches zero).
	 */
	UFUNCTION(BlueprintCallable, Category = "Tribe|Resources")
	void RefreshPaths();

	/**
	 * Removes stale (null/invalid) entries from NearbyResources,
	 * OccupiedResources, and StorageResourcePathLengths.
	 * Called by ATribeManager::CheckNullReferences() every Manage() tick.
	 */
	void CheckNullReferences();

	/**
	 * Iterates idle TribeMen and assigns the next free resource from
	 * StorageResourcePaths (shortest-path first).
	 * When the tribe is resource-abundant (more nearby than occupied),
	 * the nearest RaspBerry is preferred over the sorted list.
	 */
	void OrderTribeMenToCollect();

	/**
	 * Creates and executes a ResourceGatheringTask for TribeMan → Resource,
	 * snapping TribeMan to the navmesh first if needed, then marks the
	 * resource as occupied.
	 */
	void CollectResource(AActor* TribeMan, AActor* Resource);

	/**
	 * Returns the first NearbyResource of type RaspBerry that is not
	 * currently occupied, or nullptr if none exists.
	 */
	TObjectPtr<AActor> GetFirstNearbyRaspBerry() const;

	/**
	 * Removes Resource from OccupiedResources.
	 * Called by ATribeManager::resumeIdleTribeManTasks() when a task is
	 * abandoned after too many consecutive idle resumes.
	 */
	void RemoveOccupied(AActor* Resource);

	// ─────────────────────────────────────────────────────────────────────────
	// Count accessors
	// ─────────────────────────────────────────────────────────────────────────

	/** Number of resource actors currently within search range of any storage. */
	int32 NearbyResourceCount() const { return NearbyResources.Num(); }

	/** Number of resource actors currently assigned to a TribeMan. */
	int32 OccupiedResourceCount() const { return OccupiedResources.Num(); }

private:
	// ─────────────────────────────────────────────────────────────────────────
	// State
	// ─────────────────────────────────────────────────────────────────────────

	/** All resource actors within the search radius of any storage. */
	TArray<TObjectPtr<AActor>> NearbyResources;

	/** Resources currently assigned to a TribeMan (weak refs to avoid GC cycles). */
	TSet<TWeakObjectPtr<AActor>> OccupiedResources;

	/** storage → (resource → navmesh path length in cm). Populated asynchronously. */
	TMap<TWeakObjectPtr<AActor>, TMap<TWeakObjectPtr<AActor>, float>> StorageResourcePathLengths;

	/**
	 * Storage-resource pairs that are not occupied, sorted ascending by path
	 * length. Rebuilt by CalculateStorageResourcePaths() after each async
	 * query cycle finishes.
	 */
	TArray<FStorageResourcePath> StorageResourcePaths;

	/** Number of async navmesh path queries currently in flight. */
	int32 PendingPathQueries = 0;

	// ─────────────────────────────────────────────────────────────────────────
	// Internal helpers
	// ─────────────────────────────────────────────────────────────────────────

	/** Returns the owning ATribeManager, or nullptr if the owner is not one. */
	ATribeManager* GetOwnerManager() const;

	/** Repopulates NearbyResources via sphere overlap around each storage. */
	void CalculateNearbyResources();

	/**
	 * Flattens StorageResourcePathLengths into StorageResourcePaths,
	 * excluding occupied resources, and sorts ascending by path length.
	 */
	void CalculateStorageResourcePaths();

	/** Launches async path queries from Storage to every entry in NearbyResources. */
	void CalculateStorageResourcePathLengthsForStorage(
		TWeakObjectPtr<AActor> Storage,
		UWorld* WorldContext,
		UNavigationSystemV1* NavigationSystem,
		const ANavigationData* NavigationData);

	/** Launches a single async navmesh path query from Storage to Resource. */
	void CalculateStorageResourcePathLength(
		TWeakObjectPtr<AActor> Storage,
		TWeakObjectPtr<AActor> Resource,
		UNavigationSystemV1* NavigationSystem,
		const ANavigationData* NavigationData);

	// ─────────────────────────────────────────────────────────────────────────
	// Stale-reference checks (each returns true if at least one entry removed)
	// ─────────────────────────────────────────────────────────────────────────

	bool CheckNearbyResourcesInternal();
	bool CheckOccupiedResourcesInternal();
	bool CheckStorageResourcePathLengthsInternal();
};
