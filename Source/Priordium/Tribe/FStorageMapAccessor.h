// Copyright Priordium. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/UnrealType.h"     // FMapProperty, FByteProperty, FIntProperty, FindFProperty, CastField
#include "GameFramework/Actor.h"    // AActor

/**
 * Lightweight reflection helper that resolves the inventory TMap on a storage actor.
 *
 * Expected property layout on the storage Blueprint:
 *   TMap<EResourceType, int32>  (key = FByteProperty+UEnum, value = FIntProperty)
 *
 * Usage:
 *   FStorageMapAccessor Acc = FStorageMapAccessor::Resolve(StorageActor, PropertyName);
 *   if (Acc.IsValid()) { ... use Acc.MapProp / Acc.KeyByte / Acc.KeyEnum / Acc.ValInt ... }
 */
struct FStorageMapAccessor
{
	FMapProperty*        MapProp = nullptr;
	const FByteProperty* KeyByte = nullptr;
	const UEnum*         KeyEnum = nullptr;
	const FIntProperty*  ValInt  = nullptr;

	/** Returns true when all four fields were resolved successfully. */
	bool IsValid() const { return MapProp && KeyByte && KeyEnum && ValInt; }

	/**
	 * Attempts to resolve the named TMap property on Actor.
	 * Returns an invalid accessor (IsValid() == false) if:
	 *   - the property is not found,
	 *   - the key is not a byte-backed enum, or
	 *   - the value is not int32.
	 */
	static FStorageMapAccessor Resolve(AActor* Actor, FName PropName)
	{
		FStorageMapAccessor Out;
		if (!Actor) return Out;

		Out.MapProp = FindFProperty<FMapProperty>(Actor->GetClass(), PropName);
		if (!Out.MapProp) return Out;

		Out.KeyByte = CastField<FByteProperty>(Out.MapProp->KeyProp);
		Out.ValInt  = CastField<FIntProperty>(Out.MapProp->ValueProp);
		if (Out.KeyByte) Out.KeyEnum = Out.KeyByte->Enum;

		return Out;
	}
};
