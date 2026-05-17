// Copyright Priordium. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TribeTask.h"

class AActor;
class ACharacter;

class PRIORDIUM_API ResourceGatheringTask : public TribeTask
{
public:
	ResourceGatheringTask(ACharacter* InTribeMan, AActor* InResource);

	TObjectPtr<AActor> Resource;

	bool Execute() override;
	AActor* GetTargetResource() const override { return Resource.Get(); }
};
