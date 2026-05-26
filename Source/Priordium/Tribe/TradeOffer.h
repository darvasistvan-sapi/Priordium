// Copyright Priordium. All Rights Reserved.
//
// TradeOffer.h
// Represents a trade proposal between two tribes.
// Sender offers a list of resources in exchange for a list from the Recipient.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "TradeOffer.generated.h"

class ATribeManager;

/**
 * One completed trade exchange recorded for the UI trade log.
 * Appended by UTradeOffer::Exchange() on success.
 */
struct FTradeLogEntry
{
	FString SenderName;
	FString RecipientName;
	TArray<TPair<FName, int32>> SenderGave;
	TArray<TPair<FName, int32>> SenderReceived;
	float WorldTimeSeconds = 0.f; // for expiry
};

UCLASS(BlueprintType, Blueprintable)
class PRIORDIUM_API UTradeOffer : public UObject
{
	GENERATED_BODY()

public:

	UTradeOffer();

	/** The tribe that initiates the trade. */
	UPROPERTY(BlueprintReadWrite, Category = "Trade")
	TObjectPtr<ATribeManager> Sender;

	/** The tribe that receives the trade proposal. */
	UPROPERTY(BlueprintReadWrite, Category = "Trade")
	TObjectPtr<ATribeManager> Recipient;

	/**
	 * Resources the Sender offers to the Recipient.
	 * Each pair: (resource type name, maximum amount the Sender is willing to give).
	 * Names must match authored E_ResourceType enum entries (e.g. "Wood").
	 * Not exposed as UPROPERTY — TPair is not a USTRUCT.
	 */
	TArray<TPair<FName, int32>> Offered;

	/**
	 * Resources the Sender requests from the Recipient in return.
	 * Each pair: (resource type name, maximum amount the Sender wants to receive).
	 * Names must match authored E_ResourceType enum entries (e.g. "RaspBerry").
	 * Not exposed as UPROPERTY — TPair is not a USTRUCT.
	 */
	TArray<TPair<FName, int32>> Requested;

	/**
	 * Executes the resource exchange between Sender and Recipient.
	 *
	 * Both parameters specify the concrete amounts for this particular transaction.
	 * Every entry is validated against Offered / Requested: the actual amounts
	 * must not exceed the limits stored in those arrays.
	 *
	 * Steps:
	 *  1. Validates Sender and Recipient are set.
	 *  2. For each entry in SenderGives:  resource must be in Offered  and amount <= Offered amount.
	 *  3. For each entry in SenderTakes:  resource must be in Requested and amount <= Requested amount.
	 *  4. Affordability check: Sender has enough for SenderGives, Recipient has enough for SenderTakes.
	 *  5. Atomic execution with full rollback on any failure.
	 *
	 * Returns true if the exchange completed successfully.
	 *
	 * @param SenderGives   Resources and amounts the Sender transfers to the Recipient.
	 *                      Each amount must not exceed the corresponding entry in Offered.
	 * @param SenderTakes   Resources and amounts the Sender receives from the Recipient.
	 *                      Each amount must not exceed the corresponding entry in Requested.
	 */
	bool Exchange(const TArray<TPair<FName, int32>>& SenderGives,
	              const TArray<TPair<FName, int32>>& SenderTakes);

	/**
	 * Global log of recently completed exchanges, populated by Exchange().
	 * Read by UTradePanel to display live trade activity.
	 * Entries older than TradeLogLifetime seconds are pruned by UTradePanel on refresh.
	 */
	static TArray<FTradeLogEntry> RecentTrades;
	static constexpr float TradeLogLifetime = 15.f;
};
