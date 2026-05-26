// Copyright Priordium. All Rights Reserved.

#include "TradeOffer.h"
#include "TribeManager.h"

UTradeOffer::UTradeOffer() {}

TArray<FTradeLogEntry> UTradeOffer::RecentTrades;

bool UTradeOffer::Exchange(const TArray<TPair<FName, int32>>& SenderGives,
                           const TArray<TPair<FName, int32>>& SenderTakes)
{
	if (!Sender)
	{
		UE_LOG(LogTemp, Warning, TEXT("UTradeOffer::Exchange: Sender is null."));
		return false;
	}
	if (!Recipient)
	{
		UE_LOG(LogTemp, Warning, TEXT("UTradeOffer::Exchange: Recipient is null."));
		return false;
	}

	// ── Validate SenderGives against Offered ─────────────────────────────────
	// Every resource that the Sender wants to give must:
	//   • appear in Offered
	//   • have an amount that does not exceed the Offered limit
	for (const TPair<FName, int32>& Give : SenderGives)
	{
		if (Give.Value <= 0)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("UTradeOffer::Exchange: SenderGives amount for '%s' must be > 0."),
				*Give.Key.ToString());
			return false;
		}

		const TPair<FName, int32>* OfferedEntry = Offered.FindByPredicate(
			[&Give](const TPair<FName, int32>& P) { return P.Key == Give.Key; });

		if (!OfferedEntry)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("UTradeOffer::Exchange: '%s' is not listed in Offered."),
				*Give.Key.ToString());
			return false;
		}
		if (Give.Value > OfferedEntry->Value)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("UTradeOffer::Exchange: SenderGives amount %d for '%s' exceeds the Offered limit of %d."),
				Give.Value, *Give.Key.ToString(), OfferedEntry->Value);
			return false;
		}
	}

	// ── Validate SenderTakes against Requested ────────────────────────────────
	// Every resource that the Sender wants to receive must:
	//   • appear in Requested
	//   • have an amount that does not exceed the Requested limit
	for (const TPair<FName, int32>& Take : SenderTakes)
	{
		if (Take.Value <= 0)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("UTradeOffer::Exchange: SenderTakes amount for '%s' must be > 0."),
				*Take.Key.ToString());
			return false;
		}

		const TPair<FName, int32>* RequestedEntry = Requested.FindByPredicate(
			[&Take](const TPair<FName, int32>& P) { return P.Key == Take.Key; });

		if (!RequestedEntry)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("UTradeOffer::Exchange: '%s' is not listed in Requested."),
				*Take.Key.ToString());
			return false;
		}
		if (Take.Value > RequestedEntry->Value)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("UTradeOffer::Exchange: SenderTakes amount %d for '%s' exceeds the Requested limit of %d."),
				Take.Value, *Take.Key.ToString(), RequestedEntry->Value);
			return false;
		}
	}

	// ── Affordability checks ──────────────────────────────────────────────────
	// Sender must actually own every resource it plans to give.
	for (const TPair<FName, int32>& Give : SenderGives)
	{
		if (Sender->GetResourceAmount(Give.Key) < Give.Value)
		{
			UE_LOG(LogTemp, Log,
				TEXT("UTradeOffer::Exchange: Sender cannot afford %d of '%s'."),
				Give.Value, *Give.Key.ToString());
			return false;
		}
	}
	// Recipient must actually own every resource the Sender wants to take.
	for (const TPair<FName, int32>& Take : SenderTakes)
	{
		if (Recipient->GetResourceAmount(Take.Key) < Take.Value)
		{
			UE_LOG(LogTemp, Log,
				TEXT("UTradeOffer::Exchange: Recipient cannot afford %d of '%s'."),
				Take.Value, *Take.Key.ToString());
			return false;
		}
	}

	// ── Execute exchange (atomic with rollback) ───────────────────────────────
	// Phase 1: Sender transfers SenderGives → Recipient.
	TArray<TPair<FName, int32>> CompletedGives;
	for (const TPair<FName, int32>& Give : SenderGives)
	{
		if (!Sender->DeductResourceAmount(Give.Key, Give.Value))
		{
			UE_LOG(LogTemp, Error,
				TEXT("UTradeOffer::Exchange: Failed to deduct '%s' from Sender. Reverting."),
				*Give.Key.ToString());
			for (const TPair<FName, int32>& Done : CompletedGives)
			{
				Recipient->DeductResourceAmount(Done.Key, Done.Value);
				Sender->AddResourceAmount(Done.Key, Done.Value);
			}
			return false;
		}
		Recipient->AddResourceAmount(Give.Key, Give.Value);
		CompletedGives.Add(Give);
	}

	// Phase 2: Recipient transfers SenderTakes → Sender.
	TArray<TPair<FName, int32>> CompletedTakes;
	for (const TPair<FName, int32>& Take : SenderTakes)
	{
		if (!Recipient->DeductResourceAmount(Take.Key, Take.Value))
		{
			UE_LOG(LogTemp, Error,
				TEXT("UTradeOffer::Exchange: Failed to deduct '%s' from Recipient. Reverting all."),
				*Take.Key.ToString());
			// Revert partial Phase 2.
			for (const TPair<FName, int32>& Done : CompletedTakes)
			{
				Sender->DeductResourceAmount(Done.Key, Done.Value);
				Recipient->AddResourceAmount(Done.Key, Done.Value);
			}
			// Revert all of Phase 1.
			for (const TPair<FName, int32>& Done : CompletedGives)
			{
				Recipient->DeductResourceAmount(Done.Key, Done.Value);
				Sender->AddResourceAmount(Done.Key, Done.Value);
			}
			return false;
		}
		Sender->AddResourceAmount(Take.Key, Take.Value);
		CompletedTakes.Add(Take);
	}

	UE_LOG(LogTemp, Log,
		TEXT("UTradeOffer::Exchange: Exchange completed successfully between '%s' and '%s'."),
		*Sender->GetName(), *Recipient->GetName());

	// Record the trade for the UI trade log.
	FTradeLogEntry Entry;
	Entry.SenderName    = (Sender->TribeActor)    ? Sender->TribeActor->GetActorNameOrLabel()    : Sender->GetActorNameOrLabel();
	Entry.RecipientName = (Recipient->TribeActor) ? Recipient->TribeActor->GetActorNameOrLabel() : Recipient->GetActorNameOrLabel();
	Entry.SenderGave      = SenderGives;
	Entry.SenderReceived  = SenderTakes;
	Entry.WorldTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	RecentTrades.Add(Entry);

	return true;
}
