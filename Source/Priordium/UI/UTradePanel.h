// Copyright Priordium. All Rights Reserved.
//
// UTradePanel.h
// HUD widget that shows recent trade exchanges in the top-right corner.
// Reads UTradeOffer::RecentTrades and removes entries older than
// UTradeOffer::TradeLogLifetime seconds on each refresh.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UTradePanel.generated.h"

class UVerticalBox;
class UTextBlock;
class UBorder;

UCLASS()
class PRIORDIUM_API UTradePanel : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Seconds between automatic panel refreshes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade UI",
		meta = (ClampMin = "0.5"))
	float RefreshInterval = 2.f;

	/** Rebuilds the panel from UTradeOffer::RecentTrades. */
	UFUNCTION(BlueprintCallable, Category = "Trade UI")
	void Refresh();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:

	UPROPERTY()
	TObjectPtr<UVerticalBox> TradeListBox;

	FTimerHandle RefreshTimerHandle;

	/** Formats a resource list into a display string, e.g. "Wood=3  Berry=2". */
	static FString FormatResources(const TArray<TPair<FName, int32>>& Items);
};
