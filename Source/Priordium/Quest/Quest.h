// Copyright Priordium. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Quest.generated.h"

/**
 * Represents a single quest.
 *
 * Requirements maps each resource type name (e.g. FName("Wood")) to the
 * amount needed to complete the quest. The key matches the authored enum
 * entry names used by BP_Resource and BP_Storage throughout the project.
 */
UCLASS(BlueprintType, Blueprintable)
class PRIORDIUM_API UQuest : public UObject
{
	GENERATED_BODY()

public:
	UQuest();

	/**
	 * Resource requirements: each pair is (resource type name, required amount).
	 * Names are authored enum entry names, e.g. FName("Wood"), FName("Stone").
	 * A quest is complete when every entry is satisfied.
	 * Not exposed as UPROPERTY — TPair is not a USTRUCT and cannot be
	 * processed by UHT. Use a USTRUCT wrapper if Blueprint access is needed.
	 */
	TArray<TPair<FName, int32>> Requirements;
};
