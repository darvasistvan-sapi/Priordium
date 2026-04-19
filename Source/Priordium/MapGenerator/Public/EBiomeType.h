// Copyright Priordium. All Rights Reserved.
//
// EBiomeType.h
// Biome types that occur in the game world.
// SOLID: Single Responsibility -- only this one enum is in this file.

#pragma once

#include "CoreMinimal.h"
#include "EBiomeType.generated.h"

/**
 * EBiomeType
 * Biome types that occur in the game world.
 * Each biome type determines the nature of the terrain, the resources that appear there, and the weather.
 */
UENUM(BlueprintType)
enum class EBiomeType : uint8
{
	Ocean      UMETA(DisplayName = "Ocean"),
	Beach      UMETA(DisplayName = "Beach"),
	Plains     UMETA(DisplayName = "Plains"),
	Forest     UMETA(DisplayName = "Forest"),
	Hills      UMETA(DisplayName = "Hills"),
	Mountain   UMETA(DisplayName = "Mountain"),
	River      UMETA(DisplayName = "River"),
	Lake       UMETA(DisplayName = "Lake"),
	Swamp      UMETA(DisplayName = "Swamp")
};

