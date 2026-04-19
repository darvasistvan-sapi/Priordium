// Copyright Priordium. All Rights Reserved.
//
// AHeightmapTestActor.h
// Tests the UHeightmapGenerator component in PIE.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AHeightmapTestActor.generated.h"

class UHeightmapGenerator;
class UMapGeneratorSettings;

UCLASS()
class PRIORDIUM_API AHeightmapTestActor : public AActor
{
	GENERATED_BODY()

public:

	AHeightmapTestActor();

	/**
	 * Assign a DA_TestMapSettings Data Asset in the editor.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Generator Test")
	TObjectPtr<UMapGeneratorSettings> TestSettings = nullptr;

	/**
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Generator Test",
		meta = (ClampMin = "4", ClampMax = "1025"))
	int32 TestResolution = 64;

protected:

	virtual void BeginPlay() override;

private:

	UPROPERTY()
	TObjectPtr<UHeightmapGenerator> HeightmapComponent;
};


