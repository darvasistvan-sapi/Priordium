// Copyright Priordium. All Rights Reserved.
//
// UQuestPanel.h
// HUD widget that shows all active quests (from the world's AQuestManager)
// in the bottom-left corner of the screen.
// One row per quest, listing each resource requirement.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UQuestPanel.generated.h"

class AQuestManager;
class UVerticalBox;
class UTextBlock;
class UBorder;

UCLASS()
class PRIORDIUM_API UQuestPanel : public UUserWidget
{
	GENERATED_BODY()

public:

	// -------------------------------------------------------------------------
	// Configuration
	// -------------------------------------------------------------------------

	/**
	 * The AQuestManager Blueprint subclass to search for in the world.
	 * Assign BP_QuestManager (or the C++ class) here in the widget's Details panel.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest UI")
	TSubclassOf<AQuestManager> QuestManagerClass;

	/**
	 * Name of the FLinearColor property on BP_Tribe actors that holds the
	 * tribe's display colour. Used to tint the completion count rows.
	 * Must match the Blueprint variable name exactly.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest UI")
	FName TribeColorPropertyName = FName(TEXT("TribeColor"));

	/** Seconds between automatic panel refreshes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest UI",
		meta = (ClampMin = "0.5"))
	float RefreshInterval = 5.f;

	// -------------------------------------------------------------------------
	// Public API
	// -------------------------------------------------------------------------

	/** Rebuilds the panel contents by reading all quests from the world's QuestManager. */
	UFUNCTION(BlueprintCallable, Category = "Quest UI")
	void Refresh();

protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:

	UPROPERTY()
	TObjectPtr<UVerticalBox> QuestListBox;

	FTimerHandle RefreshTimerHandle;

	/**
	 * Adds a "Tribe N: X completed" row for each tribe in CompletedCounts.
	 * Tribe colour is read via reflection using TribeColorPropertyName.
	 * @param QuestManager  The manager whose CompletedCounts to display.
	 */
	void AddCompletionRows(const AQuestManager* QuestManager);

	/**
	 * Builds a single quest row and appends it to QuestListBox.
	 * @param QuestIndex   Zero-based index used in the row label.
	 * @param Requirements Resource requirements array of the quest.
	 */
	void AddQuestRow(int32 QuestIndex, const TArray<TPair<FName, int32>>& Requirements);

	/** Formats a requirements array into a display string, e.g. "Wood x50  RaspBerry x100". */
	static FText FormatRequirements(const TArray<TPair<FName, int32>>& Requirements);
};
