// Copyright Priordium. All Rights Reserved.

#include "UQuestPanel.h"
#include "QuestManager.h"
#include "Quest.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

// ─────────────────────────────────────────────────────────────────────────────
// RebuildWidget
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<SWidget> UQuestPanel::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		// ── Root canvas — lets us anchor to an exact corner ───────────────────
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		WidgetTree->RootWidget = Canvas;

		// ── Semi-transparent dark background ──────────────────────────────────
		UBorder* Background = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("Background"));
		Background->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.55f));
		Background->SetPadding(FMargin(12.f, 8.f));

		UCanvasPanelSlot* BgSlot = Canvas->AddChildToCanvas(Background);
		// Anchor + pivot both at (0,1) = bottom-left corner
		BgSlot->SetAnchors(FAnchors(0.f, 1.f, 0.f, 1.f));
		BgSlot->SetAlignment(FVector2D(0.f, 1.f));
		BgSlot->SetPosition(FVector2D(20.f, -20.f));   // 20 px margin from the edge
		BgSlot->SetAutoSize(true);

		// ── Vertical list for quest rows ──────────────────────────────────────
		QuestListBox = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(), TEXT("QuestListBox"));

		if (UBorderSlot* BorderSlot = Cast<UBorderSlot>(Background->GetContentSlot()))
		{
			BorderSlot->SetHorizontalAlignment(HAlign_Fill);
			BorderSlot->SetVerticalAlignment(VAlign_Fill);
		}
		Background->SetContent(QuestListBox);

		// ── Header ────────────────────────────────────────────────────────────
		UTextBlock* Header = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("Header"));
		Header->SetText(FText::FromString(TEXT("Quests")));
		Header->SetColorAndOpacity(FSlateColor(FLinearColor::White));

		FSlateFontInfo HeaderFont = Header->GetFont();
		HeaderFont.Size = 13;
		Header->SetFont(HeaderFont);

		UVerticalBoxSlot* HeaderSlot = QuestListBox->AddChildToVerticalBox(Header);
		HeaderSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	}

	return Super::RebuildWidget();
}

// ─────────────────────────────────────────────────────────────────────────────
// NativeConstruct / NativeDestruct
// ─────────────────────────────────────────────────────────────────────────────

void UQuestPanel::NativeConstruct()
{
	Super::NativeConstruct();

	Refresh();

	if (RefreshInterval > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(
			RefreshTimerHandle,
			this,
			&UQuestPanel::Refresh,
			RefreshInterval,
			/*bLoop=*/true);
	}
}

void UQuestPanel::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}
	Super::NativeDestruct();
}

// ─────────────────────────────────────────────────────────────────────────────
// Refresh
// ─────────────────────────────────────────────────────────────────────────────

void UQuestPanel::Refresh()
{
	if (!QuestListBox) return;

	// Remove all quest rows (keep the first child = header).
	while (QuestListBox->GetChildrenCount() > 1)
	{
		QuestListBox->RemoveChildAt(QuestListBox->GetChildrenCount() - 1);
	}

	UWorld* World = GetWorld();
	if (!World) return;

	if (!QuestManagerClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UQuestPanel: QuestManagerClass is not set — assign it in the widget's Details panel."));
		return;
	}

	// Find the first QuestManager of the configured class in the world.
	AQuestManager* QuestManager = nullptr;
	for (TActorIterator<AQuestManager> It(World, QuestManagerClass); It; ++It)
	{
		QuestManager = *It;
		break;
	}

	if (!QuestManager)
	{
		UTextBlock* EmptyLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		EmptyLabel->SetText(FText::FromString(TEXT("No quests")));
		EmptyLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.f)));
		QuestListBox->AddChildToVerticalBox(EmptyLabel);
		return;
	}

	if (QuestManager->Quests.IsEmpty())
	{
		UTextBlock* EmptyLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		EmptyLabel->SetText(FText::FromString(TEXT("No active quests")));
		EmptyLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.f)));
		QuestListBox->AddChildToVerticalBox(EmptyLabel);
		return;
	}

	// ── Tribe completion counts (above quest list) ────────────────────────────
	AddCompletionRows(QuestManager);

	// ── Separator ─────────────────────────────────────────────────────────────
	UTextBlock* Sep = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Sep->SetText(FText::FromString(TEXT("──────────────────")));
	Sep->SetColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.4f, 0.4f, 1.f)));
	FSlateFontInfo SepFont = Sep->GetFont();
	SepFont.Size = 8;
	Sep->SetFont(SepFont);
	UVerticalBoxSlot* SepSlot = QuestListBox->AddChildToVerticalBox(Sep);
	SepSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 4.f));

	// ── Quest list ────────────────────────────────────────────────────────────
	int32 QuestIndex = 0;
	for (const TObjectPtr<UQuest>& QuestPtr : QuestManager->Quests)
	{
		if (const UQuest* Quest = QuestPtr.Get())
		{
			AddQuestRow(QuestIndex, Quest->Requirements);
			++QuestIndex;
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// AddCompletionRows
// ─────────────────────────────────────────────────────────────────────────────

void UQuestPanel::AddCompletionRows(const AQuestManager* QuestManager)
{
	if (!QuestManager || !QuestListBox) return;

	if (QuestManager->CompletedCounts.IsEmpty())
	{
		// Show a placeholder so the section is never empty.
		UTextBlock* NoneLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		NoneLabel->SetText(FText::FromString(TEXT("No completions yet")));
		NoneLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.f)));
		FSlateFontInfo F = NoneLabel->GetFont();
		F.Size = 10;
		NoneLabel->SetFont(F);
		QuestListBox->AddChildToVerticalBox(NoneLabel);
		return;
	}

	for (const TPair<TObjectPtr<AActor>, int32>& Pair : QuestManager->CompletedCounts)
	{
		const AActor* TribeActor = Pair.Key.Get();
		const int32   Count      = Pair.Value;

		// Try to read the tribe colour via reflection; fall back to white.
		FLinearColor TribeColor = FLinearColor::White;
		if (TribeActor)
		{
			const FStructProperty* ColorProp = FindFProperty<FStructProperty>(
				TribeActor->GetClass(), TribeColorPropertyName);
			if (ColorProp && ColorProp->Struct == TBaseStructure<FLinearColor>::Get())
			{
				TribeColor = *ColorProp->ContainerPtrToValuePtr<FLinearColor>(TribeActor);
			}
		}

		const FString TribeName = TribeActor ? TribeActor->GetActorNameOrLabel() : TEXT("Unknown Tribe");

		UTextBlock* Row = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Row->SetText(FText::FromString(
			FString::Printf(TEXT("%s:  %d completed"), *TribeName, Count)));
		Row->SetColorAndOpacity(FSlateColor(TribeColor));

		FSlateFontInfo RowFont = Row->GetFont();
		RowFont.Size = 11;
		Row->SetFont(RowFont);

		UVerticalBoxSlot* RowSlot = QuestListBox->AddChildToVerticalBox(Row);
		RowSlot->SetPadding(FMargin(0.f, 2.f));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// AddQuestRow
// ─────────────────────────────────────────────────────────────────────────────

void UQuestPanel::AddQuestRow(int32 QuestIndex, const TArray<TPair<FName, int32>>& Requirements)
{
	if (!QuestListBox) return;

	// ── Quest label ───────────────────────────────────────────────────────────
	UTextBlock* QuestLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	QuestLabel->SetText(FText::FromString(
		FString::Printf(TEXT("Quest %d:  "), QuestIndex + 1)));
	QuestLabel->SetColorAndOpacity(FSlateColor(FLinearColor::Yellow));

	FSlateFontInfo LabelFont = QuestLabel->GetFont();
	LabelFont.Size = 11;
	QuestLabel->SetFont(LabelFont);

	UVerticalBoxSlot* LabelSlot = QuestListBox->AddChildToVerticalBox(QuestLabel);
	LabelSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));

	// ── Requirements text ─────────────────────────────────────────────────────
	UTextBlock* ReqLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ReqLabel->SetText(FormatRequirements(Requirements));
	ReqLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));

	FSlateFontInfo ReqFont = ReqLabel->GetFont();
	ReqFont.Size = 10;
	ReqLabel->SetFont(ReqFont);

	UVerticalBoxSlot* ReqSlot = QuestListBox->AddChildToVerticalBox(ReqLabel);
	ReqSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 2.f));
}

// ─────────────────────────────────────────────────────────────────────────────
// FormatRequirements
// ─────────────────────────────────────────────────────────────────────────────

// static
FText UQuestPanel::FormatRequirements(const TArray<TPair<FName, int32>>& Requirements)
{
	if (Requirements.IsEmpty())
	{
		return FText::FromString(TEXT("–"));
	}

	FString Result;
	for (const TPair<FName, int32>& Pair : Requirements)
	{
		if (!Result.IsEmpty()) Result += TEXT("   ");
		Result += FString::Printf(TEXT("%d %s"), Pair.Value, *Pair.Key.ToString());
	}
	return FText::FromString(Result);
}
