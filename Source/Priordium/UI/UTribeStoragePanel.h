// Copyright Priordium. All Rights Reserved.
//
// UTribeStoragePanel.h
// HUD widget that shows each tribe's stored resources in the bottom-right corner.
// One row per tribe, tinted with the tribe's colour.
//
// The panel iterates all actors in the world and identifies "Tribe" actors by
// reflection: any actor that exposes both a TribeColorPropertyName (FLinearColor)
// and a TribeManagerPropertyName (ATribeManager*) property is treated as a tribe.
//
// Resource amounts are read from storage actors via reflection:
//   the storage actor must expose a TMap<FName, int32> property whose name matches
//   StorageInventoryPropertyName (default "StoredResources").

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UTribeStoragePanel.generated.h"

class ATribeManager;
class UVerticalBox;
class UTextBlock;
class UBorder;

UCLASS()
class PRIORDIUM_API UTribeStoragePanel : public UUserWidget
{
	GENERATED_BODY()

public:

	// -------------------------------------------------------------------------
	// Configuration
	// -------------------------------------------------------------------------

	/**
	 * The BP_Tribe class to search for in the world.
	 * Assign BP_Tribe here in the widget's Details panel.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tribe UI")
	TSubclassOf<AActor> TribeActorClass;

	/**
	 * Name of the FLinearColor property on BP_Tribe actors that holds the
	 * tribe's display colour.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tribe UI")
	FName TribeColorPropertyName = FName(TEXT("TribeColor"));

	/**
	 * Name of the ATribeManager* object property on BP_Tribe actors.
	 * Must match the variable name in the Blueprint exactly.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tribe UI")
	FName TribeManagerPropertyName = FName(TEXT("Manager"));

	/**
	 * Name of the TMap<FName, int32> property on BP_Storage actors that holds
	 * the stored resource amounts.  Set this to match your Blueprint variable.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tribe UI")
	FName StorageInventoryPropertyName = FName(TEXT("StoredResources"));

	/** Seconds between automatic panel refreshes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tribe UI",
		meta = (ClampMin = "0.5"))
	float RefreshInterval = 5.f;

	// -------------------------------------------------------------------------
	// Public API
	// -------------------------------------------------------------------------

	/** Rebuilds the panel contents by scanning all Tribe actors in the world. */
	UFUNCTION(BlueprintCallable, Category = "Tribe UI")
	void Refresh();

protected:

	/**
	 * Called before the Slate widget tree is built — the correct place to
	 * create the UMG widget hierarchy programmatically in C++.
	 */
	virtual TSharedRef<SWidget> RebuildWidget() override;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:

	// -------------------------------------------------------------------------
	// Widget references (created in RebuildWidget)
	// -------------------------------------------------------------------------

	UPROPERTY()
	TObjectPtr<UVerticalBox> TribeListBox;

	// -------------------------------------------------------------------------
	// Timer
	// -------------------------------------------------------------------------

	FTimerHandle RefreshTimerHandle;

	// -------------------------------------------------------------------------
	// Helpers
	// -------------------------------------------------------------------------

	/**
	 * Builds a single tribe row and appends it to TribeListBox.
	 * @param TribeManager  The tribe's manager (for storage access and label).
	 * @param TribeColor    Colour read from the BP_Tribe actor.
	 */
	void AddTribeRow(ATribeManager* TribeManager, const FLinearColor& TribeColor);

	/**
	 * Reads the TMap<FName, int32> property named StorageInventoryPropertyName
	 * from the given storage actor and accumulates the values into OutInventory.
	 * Returns true if the property was found.
	 */
	bool AccumulateStorageInventory(
		AActor*                    StorageActor,
		TMap<FName, int32>&        OutInventory) const;

	/** Formats an inventory map into a single display string, e.g. "Wood: 3  Stone: 2". */
	static FText FormatInventory(const TMap<FName, int32>& Inventory);
};
