// Copyright Priordium. All Rights Reserved.
//
// ALandscapeBuilderTestActor.h
// Tests the ULandscapeBuilder component in PIE.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ALandscapeBuilderTestActor.generated.h"

class ULandscapeBuilder;

UCLASS()
class PRIORDIUM_API ALandscapeBuilderTestActor : public AActor
{
	GENERATED_BODY()

public:

	ALandscapeBuilderTestActor();

protected:

	virtual void BeginPlay() override;

private:

	UPROPERTY()
	TObjectPtr<ULandscapeBuilder> LandscapeBuilderComponent;
};


