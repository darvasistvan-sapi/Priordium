// Copyright Priordium. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Quest.h"
#include "QuestManager.generated.h"

/**
 * Manages all quests in the game.
 *
 * - Quests         : the full list of available/active quests
 * - CompletedCounts: tracks how many quests each tribe has completed
 *                    (key = BP_Tribe actor, value = completed quest count)
 */
UCLASS(BlueprintType, Blueprintable)
class PRIORDIUM_API AQuestManager : public AActor
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

public:
	AQuestManager();

	/** All quests known to the manager (active, completed, pending). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	TArray<TObjectPtr<UQuest>> Quests;

	/**
	 * Number of quests completed per tribe.
	 * Key   : BP_Tribe actor that owns the tribe.
	 * Value : total number of quests that tribe has completed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	TMap<TObjectPtr<AActor>, int32> CompletedCounts;

	// -------------------------------------------------------------------------
	// Random quest generation settings
	// -------------------------------------------------------------------------

	/**
	 * Pool of resource type names to draw from when generating a random quest.
	 * Must match the authored E_ResourceType enum entry names exactly
	 * (e.g. "Wood", "RaspBerry").
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	TArray<FName> PossibleResourceTypes;

	/** Minimum number of distinct resource types required in a generated quest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest",
		meta = (ClampMin = "1", UIMin = "1"))
	int32 MinRequirementCount = 1;

	/** Maximum number of distinct resource types required in a generated quest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest",
		meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxRequirementCount = 3;

	/** Minimum amount required per resource type in a generated quest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest",
		meta = (ClampMin = "1", UIMin = "1"))
	int32 MinAmount = 1;

	/** Maximum amount required per resource type in a generated quest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest",
		meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxAmount = 10;

	// -------------------------------------------------------------------------
	// Public helpers
	// -------------------------------------------------------------------------

	/**
	 * Creates a new UQuest with randomly chosen requirements drawn from
	 * PossibleResourceTypes, adds it to Quests, and returns it.
	 *
	 * The number of distinct resource types is chosen uniformly in
	 * [MinRequirementCount, MaxRequirementCount], clamped to the size of
	 * PossibleResourceTypes.  Each required amount is chosen uniformly in
	 * [MinAmount, MaxAmount].
	 *
	 * Returns nullptr if PossibleResourceTypes is empty.
	 */
	UFUNCTION(BlueprintCallable, Category = "Quest")
	UQuest* CreateRandomQuest();

	/**
	 * Adds a quest to the managed list.
	 * Does nothing if Quest is null or already present.
	 */
	UFUNCTION(BlueprintCallable, Category = "Quest")
	void AddQuest(UQuest* Quest);

	/**
	 * Removes a quest from the managed list.
	 * Does nothing if Quest is null or not present.
	 */
	UFUNCTION(BlueprintCallable, Category = "Quest")
	void RemoveQuest(UQuest* Quest);

	/**
	 * Records one completed quest for the given tribe.
	 * Removes Quest from Quests and immediately generates a replacement via
	 * CreateRandomQuest() to keep the quest pool size stable.
	 * Creates the CompletedCounts entry if the tribe has not completed any quest yet.
	 *
	 * @param TribeActor  The BP_Tribe actor that completed the quest.
	 * @param Quest       The quest that was completed (will be removed from Quests).
	 */
	UFUNCTION(BlueprintCallable, Category = "Quest")
	void RegisterCompletion(AActor* TribeActor, UQuest* Quest);

	/**
	 * Returns how many quests the given tribe has completed.
	 * Returns 0 if the tribe has no entry.
	 *
	 * @param TribeActor  The BP_Tribe actor to query.
	 */
	UFUNCTION(BlueprintCallable, Category = "Quest")
	int32 GetCompletedCount(AActor* TribeActor) const;
};
