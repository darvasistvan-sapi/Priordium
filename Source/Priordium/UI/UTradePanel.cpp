// Copyright Priordium. All Rights Reserved.

#include "UTradePanel.h"
#include "TradeOffer.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"

#include "Engine/World.h"

// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<SWidget> UTradePanel::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		// ── Root canvas — anchored to the top-right corner ────────────────
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		WidgetTree->RootWidget = Canvas;

		// ── Semi-transparent dark background ─────────────────────────────
		UBorder* Background = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("Background"));
		Background->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.55f));
		Background->SetPadding(FMargin(12.f, 8.f));

		UCanvasPanelSlot* BgSlot = Canvas->AddChildToCanvas(Background);
		// Anchor + pivot at (1, 0) = top-right corner
		BgSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 0.f));
		BgSlot->SetAlignment(FVector2D(1.f, 0.f));
		BgSlot->SetPosition(FVector2D(-20.f, 20.f));   // 20 px margin from edges
		BgSlot->SetAutoSize(true);

		// ── Vertical list of trade entries ────────────────────────────────
		TradeListBox = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(), TEXT("TradeListBox"));

		if (UBorderSlot* BorderSlot = Cast<UBorderSlot>(Background->GetContentSlot()))
		{
			BorderSlot->SetHorizontalAlignment(HAlign_Fill);
			BorderSlot->SetVerticalAlignment(VAlign_Fill);
		}
		Background->SetContent(TradeListBox);

		// ── Header label ──────────────────────────────────────────────────
		UTextBlock* Header = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("Header"));
		Header->SetText(FText::FromString(TEXT("Recent Trades")));
		Header->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.85f, 0.f)));

		FSlateFontInfo HeaderFont = Header->GetFont();
		HeaderFont.Size = 12;
		Header->SetFont(HeaderFont);

		UVerticalBoxSlot* HeaderSlot = TradeListBox->AddChildToVerticalBox(Header);
		HeaderSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	}

	return Super::RebuildWidget();
}

void UTradePanel::NativeConstruct()
{
	Super::NativeConstruct();

	Refresh();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RefreshTimerHandle, this, &UTradePanel::Refresh,
			RefreshInterval, /*bLoop=*/true);
	}
}

void UTradePanel::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}

	Super::NativeDestruct();
}

void UTradePanel::Refresh()
{
	if (!TradeListBox) return;

	UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;

	// Rebuild the list (keep the header at index 0, clear the rest).
	while (TradeListBox->GetChildrenCount() > 1)
	{
		TradeListBox->RemoveChildAt(1);
	}

	if (UTradeOffer::RecentTrades.IsEmpty())
	{
		UTextBlock* EmptyLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		EmptyLabel->SetText(FText::FromString(TEXT("–")));
		EmptyLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)));
		TradeListBox->AddChildToVerticalBox(EmptyLabel);
		return;
	}

	// Show the last 4 trades, newest first.
	const int32 StartIdx = UTradeOffer::RecentTrades.Num() - 1;
	const int32 EndIdx   = FMath::Max(0, UTradeOffer::RecentTrades.Num() - 4);
	for (int32 i = StartIdx; i >= EndIdx; --i)
	{
		const FTradeLogEntry& Entry = UTradeOffer::RecentTrades[i];

		// Line 1: "SenderName → RecipientName"
		const FString HeaderStr = FString::Printf(
			TEXT("%s  →  %s"), *Entry.SenderName, *Entry.RecipientName);

		UTextBlock* EntryHeader = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		EntryHeader->SetText(FText::FromString(HeaderStr));
		EntryHeader->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.9f, 0.9f)));

		UVerticalBoxSlot* EntryHeaderSlot = TradeListBox->AddChildToVerticalBox(EntryHeader);
		EntryHeaderSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));

		// Line 2: "  Gave: Wood=3  Berry=2"
		if (!Entry.SenderGave.IsEmpty())
		{
			UTextBlock* GaveLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			GaveLabel->SetText(FText::FromString(
				TEXT("  Gave:     ") + FormatResources(Entry.SenderGave)));
			GaveLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 1.f, 0.6f)));
			TradeListBox->AddChildToVerticalBox(GaveLabel);
		}

		// Line 3: "  Received: Stone=1"
		if (!Entry.SenderReceived.IsEmpty())
		{
			UTextBlock* RecvLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			RecvLabel->SetText(FText::FromString(
				TEXT("  Received: ") + FormatResources(Entry.SenderReceived)));
			RecvLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.8f, 1.f)));
			TradeListBox->AddChildToVerticalBox(RecvLabel);
		}
	}
}

FString UTradePanel::FormatResources(const TArray<TPair<FName, int32>>& Items)
{
	FString Result;
	for (const TPair<FName, int32>& Item : Items)
	{
		Result += FString::Printf(TEXT("%s=%d  "), *Item.Key.ToString(), Item.Value);
	}
	return Result.TrimEnd();
}
