// Copyright Priordium. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class ACharacter;

class PRIORDIUM_API TribeTask
{
public:
	explicit TribeTask(ACharacter* InTribeMan);
	virtual ~TribeTask() = default;

	TObjectPtr<ACharacter> TribeMan;

	virtual bool Execute() = 0;

	// Returns the target resource associated with this task, if any.
	virtual AActor* GetTargetResource() const { return nullptr; }
};
