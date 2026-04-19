// Copyright Priordium. All Rights Reserved.
//
// FSeasonProfile.h
// Season temperature modifier profile for climate zones.
// SOLID: Single Responsibility -- stores exclusively season modifier data.

#pragma once

#include "CoreMinimal.h"
#include "FSeasonProfile.generated.h"

/**
 * FSeasonProfile
 * Defines seasonal temperature modifiers applied on top of the base cell temperature.
 * TimeOfYear = 0.0 -> Winter (WinterTempModifier applied)
 * TimeOfYear = 0.5 -> Summer (SummerTempModifier applied)
 * Smooth cosine interpolation between the two extremes.
 */
USTRUCT(BlueprintType)
struct PRIORDIUM_API FSeasonProfile
{
	GENERATED_BODY()

	/** Temperature modifier added in summer (TimeOfYear = 0.5). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climate",
		meta = (ToolTip = "Temperature modifier added in summer (TimeOfYear=0.5). Positive = warmer.", ClampMin = "-1.0", ClampMax = "1.0"))
	float SummerTempModifier = 0.1f;

	/** Temperature modifier added in winter (TimeOfYear = 0.0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climate",
		meta = (ToolTip = "Temperature modifier added in winter (TimeOfYear=0.0). Negative = colder.", ClampMin = "-1.0", ClampMax = "1.0"))
	float WinterTempModifier = -0.15f;
};
