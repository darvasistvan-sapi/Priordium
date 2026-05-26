// Copyright Priordium. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TribeQuestManager.h"
#include "TribeBuyingManager.h"
#include "TribeResourcePaths.h"
#include "TribeManager.generated.h"

class ACharacter;
class TribeTask;
class UHeightmapGenerator;
class UMapGeneratorSettings;
class ULandscapeBuilder;
class UTradeOffer;

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

	// The BP_Tribe actor that owns this manager.
	// Passed to SpawnBuilding() so new storages get their "Tribe" property set.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tribe")
	TObjectPtr<AActor> TribeActor;

	/**
	 * Quest and trade-offer subsystem for this tribe.
	 * Created in the constructor; holds QuestManager, ReceivedTradeOffers,
	 * TradeQuests, and all quest/trade logic.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	TObjectPtr<UTribeQuestManager> QuestHandler;

	/**
	 * Purchasing subsystem for this tribe.
	 * Created in the constructor; holds ItemPrices, price lookup, affordability
	 * checks, resource deduction, building construction, and TribeMan recruitment.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Buying")
	TObjectPtr<UTribeBuyingManager> BuyingHandler;

	/**
	 * Resource-path subsystem for this tribe.
	 * Owns nearby resource census, async navmesh path lengths, occupied-resource
	 * tracking, and TribeMan collection dispatch.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resources")
	TObjectPtr<UTribeResourcePaths> ResourcePaths;

	// Outliner folder path for this tribe (e.g. "Tribes/Tribe_0").
	// Passed to SpawnBuilding() so newly built actors land in the correct folder.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tribe")
	FName TribeFolderPath;

	// -------------------------------------------------------------------------
	// Public methods
	// -------------------------------------------------------------------------

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

	/** Main update entry point. Called every few seconds via timer. */
	void Manage();

	/** Removes stale references from TribeMan/storage collections, then delegates
	 *  resource-path cleanup to ResourcePaths->CheckNullReferences(). */
	void CheckNullReferences();

	// -------------------------------------------------------------------------
	// Task registry accessors
	// Used by UTribeResourcePaths to assign and query TribeMan tasks without
	// exposing the private tribeManTasks map directly.
	// -------------------------------------------------------------------------

	/** Returns true if TribeMan already has an assigned task. */
	bool HasTribeManTask(ACharacter* TribeMan) const;

	/** Registers Task as the active task for TribeMan. */
	void AssignTribeManTask(ACharacter* TribeMan, TSharedPtr<TribeTask> Task);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	// TribeMan -> their current task
	TMap<TObjectPtr<ACharacter>, TSharedPtr<TribeTask>> tribeManTasks;

	// How many consecutive Manage() ticks a TribeMan stayed idle after Execute()
	// was called. Cleared when they start moving; task is dropped after threshold.
	TMap<TObjectPtr<ACharacter>, int32> TribeManResumeFailures;

	// Timer handle for the Manage tick
	FTimerHandle ManageTimerHandle;

	// -------------------------------------------------------------------------
	// Internal helpers
	// -------------------------------------------------------------------------

	/**
	 * Iterates all TribeMen. For each one that has a task assigned in tribeManTasks
	 * but is currently idle (velocity ≈ 0), re-executes their task so they resume work
	 * after reaching a destination or being interrupted.
	 */
	void resumeIdleTribeManTasks();

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

	bool checkTribeMen();
	bool checkStorages();
	bool checkTribeManTasks();
};
