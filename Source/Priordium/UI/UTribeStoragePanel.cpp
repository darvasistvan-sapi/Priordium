// Copyright Priordium. All Rights Reserved.

#include "UTribeStoragePanel.h"

#include "TribeManager.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

// -------------------------------------------------------------------------
// RebuildWidget  (called before the Slate tree is built — correct place for
// programmatic UMG hierarchy construction)
// -------------------------------------------------------------------------

TSharedRef<SWidget> UTribeStoragePanel::RebuildWidget()
{
	// Guard against repeated calls (e.g. designer re-preview).
	if (!WidgetTree->RootWidget)
	{
		// ── Root: canvas so we can anchor to the bottom-right corner ──────
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		WidgetTree->RootWidget = Canvas;

		// ── Semi-transparent dark background ──────────────────────────────
		UBorder* Background = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("Background"));
		Background->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.55f));
		Background->SetPadding(FMargin(12.f, 8.f));

		UCanvasPanelSlot* BgSlot = Canvas->AddChildToCanvas(Background);
		// Anchor + pivot both at (1,1) = bottom-right corner
		BgSlot->SetAnchors(FAnchors(1.f, 1.f, 1.f, 1.f));
		BgSlot->SetAlignment(FVector2D(1.f, 1.f));
		BgSlot->SetPosition(FVector2D(-20.f, -20.f));   // 20 px margin from the edge
		BgSlot->SetAutoSize(true);

		// ── Vertical list that holds tribe rows ───────────────────────────
		TribeListBox = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(), TEXT("TribeListBox"));

		if (UBorderSlot* BorderSlot = Cast<UBorderSlot>(Background->GetContentSlot()))
		{
			BorderSlot->SetHorizontalAlignment(HAlign_Fill);
			BorderSlot->SetVerticalAlignment(VAlign_Fill);
		}
		Background->SetContent(TribeListBox);

		// ── Header text ───────────────────────────────────────────────────
		UTextBlock* Header = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("Header"));
		Header->SetText(FText::FromString(TEXT("Resources")));
		Header->SetColorAndOpacity(FSlateColor(FLinearColor::White));

		FSlateFontInfo HeaderFont = Header->GetFont();
		HeaderFont.Size = 13;
		Header->SetFont(HeaderFont);

		UVerticalBoxSlot* HeaderSlot = TribeListBox->AddChildToVerticalBox(Header);
		HeaderSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	}

	return Super::RebuildWidget();
}

// -------------------------------------------------------------------------
// NativeConstruct / NativeDestruct
// -------------------------------------------------------------------------

void UTribeStoragePanel::NativeConstruct()
{
	Super::NativeConstruct();

	// ── Initial data population ───────────────────────────────────────────
	Refresh();

	// ── Auto-refresh timer ────────────────────────────────────────────────
	if (RefreshInterval > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(
			RefreshTimerHandle,
			this,
			&UTribeStoragePanel::Refresh,
			RefreshInterval,
			/*bLoop=*/true);
	}
}

void UTribeStoragePanel::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}
	Super::NativeDestruct();
}

// -------------------------------------------------------------------------
// Refresh
// -------------------------------------------------------------------------

void UTribeStoragePanel::Refresh()
{
	if (!TribeListBox) return;

	// Remove all existing tribe rows (keep the first child = header).
	while (TribeListBox->GetChildrenCount() > 1)
	{
		TribeListBox->RemoveChildAt(TribeListBox->GetChildrenCount() - 1);
	}

	UWorld* World = GetWorld();
	if (!World) return;

	if (!TribeActorClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UTribeStoragePanel: TribeActorClass is not set — assign BP_Tribe in the widget's Details panel."));
		return;
	}

	for (TActorIterator<AActor> It(World, TribeActorClass); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor) continue;

		// ── Read TribeColor ───────────────────────────────────────────────
		const FStructProperty* ColorProp = FindFProperty<FStructProperty>(
			Actor->GetClass(), TribeColorPropertyName);
		if (!ColorProp || ColorProp->Struct != TBaseStructure<FLinearColor>::Get())
			continue;

		// ── Read Manager reference ────────────────────────────────────────
		const FObjectProperty* ManagerProp = FindFProperty<FObjectProperty>(
			Actor->GetClass(), TribeManagerPropertyName);
		if (!ManagerProp) continue;

		ATribeManager* TribeManager = Cast<ATribeManager>(
			ManagerProp->GetObjectPropertyValue_InContainer(Actor));
		// TribeManager may be null if the generator hasn't assigned it yet;
		// AddTribeRow handles this gracefully (shows "–" for inventory).

		const FLinearColor TribeColor =
			*ColorProp->ContainerPtrToValuePtr<FLinearColor>(Actor);

		AddTribeRow(TribeManager, TribeColor);
	}
}

// -------------------------------------------------------------------------
// AddTribeRow
// -------------------------------------------------------------------------

void UTribeStoragePanel::AddTribeRow(ATribeManager* TribeManager, const FLinearColor& TribeColor)
{
	if (!TribeListBox) return;

	// Aggregate inventory across all storage actors owned by this tribe.
	// If TribeManager is null (not yet assigned by the generator), the row
	// still renders with "–" inventory and will update on the next timer tick.
	TMap<FName, int32> TotalInventory;
	bool bInventoryFound = false;

	if (TribeManager)
	{
		for (const TObjectPtr<AActor>& Storage : TribeManager->storages)
		{
			if (!Storage) continue;
			if (AccumulateStorageInventory(Storage.Get(), TotalInventory))
			{
				bInventoryFound = true;
			}
		}
	}

	// ── Row container ─────────────────────────────────────────────────────
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass());

	UVerticalBoxSlot* RowSlot = TribeListBox->AddChildToVerticalBox(Row);
	RowSlot->SetPadding(FMargin(0.f, 2.f));

	// ── Tribe label ───────────────────────────────────────────────────────
	UTextBlock* TribeLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	const FString TribeName = (TribeManager && TribeManager->TribeActor)
		? TribeManager->TribeActor->GetActorNameOrLabel()
		: TEXT("Unknown Tribe");
	TribeLabel->SetText(FText::FromString(TribeName + TEXT(":  ")));
	TribeLabel->SetColorAndOpacity(FSlateColor(TribeColor));

	UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(TribeLabel);
	LabelSlot->SetVerticalAlignment(VAlign_Center);
	LabelSlot->SetPadding(FMargin(0.f, 0.f, 4.f, 0.f));

	// ── Resource summary ──────────────────────────────────────────────────
	const FText ResourceText = bInventoryFound
		? FormatInventory(TotalInventory)
		: FText::FromString(TEXT("–"));

	UTextBlock* ResourceLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ResourceLabel->SetText(ResourceText);
	ResourceLabel->SetColorAndOpacity(FSlateColor(TribeColor));

	UHorizontalBoxSlot* ResourceSlot = Row->AddChildToHorizontalBox(ResourceLabel);
	ResourceSlot->SetVerticalAlignment(VAlign_Center);
}

// -------------------------------------------------------------------------
// AccumulateStorageInventory
// -------------------------------------------------------------------------

bool UTribeStoragePanel::AccumulateStorageInventory(
	AActor*             StorageActor,
	TMap<FName, int32>& OutInventory) const
{
	if (!StorageActor) return false;

	// Look for a TMap<FName, int32> property named StorageInventoryPropertyName.
	FMapProperty* MapProp = FindFProperty<FMapProperty>(
		StorageActor->GetClass(), StorageInventoryPropertyName);

	if (!MapProp) return false;

	// ── Detect key type ───────────────────────────────────────────────────
	// Blueprint enums are stored as FByteProperty with an associated UEnum.
	const FByteProperty* KeyByte = CastField<FByteProperty>(MapProp->KeyProp);

	const UEnum* KeyEnumDef = KeyByte ? KeyByte->Enum : nullptr;

	if (!KeyEnumDef)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UTribeStoragePanel: '%s' on '%s' — key is not a Blueprint enum (FByteProperty+UEnum). "
			     "Actual key type: %s"),
			*StorageInventoryPropertyName.ToString(),
			*StorageActor->GetClass()->GetName(),
			*MapProp->KeyProp->GetClass()->GetName());
		return false;
	}

	// ── Detect value type ─────────────────────────────────────────────────
	const FIntProperty* ValInt = CastField<FIntProperty>(MapProp->ValueProp);
	if (!ValInt)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UTribeStoragePanel: '%s' on '%s' — value is not int32. Actual: %s"),
			*StorageInventoryPropertyName.ToString(),
			*StorageActor->GetClass()->GetName(),
			*MapProp->ValueProp->GetClass()->GetName());
		return false;
	}

	FScriptMapHelper MapHelper(MapProp, MapProp->ContainerPtrToValuePtr<void>(StorageActor));

	for (FScriptMapHelper::FIterator Iter(MapHelper); Iter; ++Iter)
	{
		const void* RawKey = MapHelper.GetKeyPtr(Iter.GetInternalIndex());

		// Read the byte value of the enum and convert to its authored name (e.g. "Fa")
		const int64 EnumVal = static_cast<int64>(KeyByte->GetPropertyValue(RawKey));
		const FName Key(KeyEnumDef->GetAuthoredNameStringByValue(EnumVal));

		const int32 Value = ValInt->GetPropertyValue(MapHelper.GetValuePtr(Iter.GetInternalIndex()));
		OutInventory.FindOrAdd(Key) += Value;
	}

	return true;
}

// -------------------------------------------------------------------------
// FormatInventory
// -------------------------------------------------------------------------

// static
FText UTribeStoragePanel::FormatInventory(const TMap<FName, int32>& Inventory)
{
	if (Inventory.IsEmpty())
	{
		return FText::FromString(TEXT("–"));
	}

	FString Result;
	for (const auto& Pair : Inventory)
	{
		if (!Result.IsEmpty()) Result += TEXT("  ");
		Result += FString::Printf(TEXT("%s: %d"), *Pair.Key.ToString(), Pair.Value);
	}
	return FText::FromString(Result);
}
