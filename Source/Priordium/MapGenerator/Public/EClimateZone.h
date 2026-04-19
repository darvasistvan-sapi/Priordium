// Copyright Priordium. All Rights Reserved.
//
// EClimateZone.h
// Climate zones that influence biome distribution and resources.
// SOLID: Single Responsibility -- only this one enum is in this file.

#pragma once

#include "CoreMinimal.h"
#include "EClimateZone.generated.h"

/**
 * EClimateZone
 * Climate zones for the map generator.
 * The climate zone determines temperature, precipitation, and the biome types that occur.
 */
UENUM(BlueprintType)
enum class EClimateZone : uint8
{
	Tropical      UMETA(DisplayName = "Tropical"),
	Temperate     UMETA(DisplayName = "Temperate"),
	Continental   UMETA(DisplayName = "Continental"),
	Subarctic     UMETA(DisplayName = "Subarctic")
};

