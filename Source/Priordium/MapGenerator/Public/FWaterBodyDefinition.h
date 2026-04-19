// Copyright Priordium. All Rights Reserved.
//
// FWaterBodyDefinition.h
// Data definition for a generated water body (river, lake, or ocean).
// SOLID: Single Responsibility -- stores exclusively water body geometry and type data.
//
// Uses EWaterBodyType from the Water plugin (WaterBodyTypes.h).

#pragma once

#include "CoreMinimal.h"
#include "WaterBodyTypes.h"
#include "FWaterBodyDefinition.generated.h"

/**
 * FWaterBodyDefinition
 * Describes a water body to be spawned by UWaterSystemBuilder.
 *
 * - WaterType:    River, Lake, or Ocean  (EWaterBodyType from Water plugin)
 * - SplinePoints: World-space control points for the water body shape/path
 * - Width:        Width of the water body in centimetres (rivers: source narrow, mouth wide)
 * - Depth:        Depth of the water body in centimetres
 */
USTRUCT(BlueprintType)
struct PRIORDIUM_API FWaterBodyDefinition
{
	GENERATED_BODY()

	/** The type of water body. Uses the Water plugin's EWaterBodyType. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water",
		meta = (ToolTip = "Type of the generated water body: River, Lake, or Ocean."))
	EWaterBodyType WaterType = EWaterBodyType::River;

	/** World-space spline control points defining the shape or path of the water body. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water",
		meta = (ToolTip = "World-space spline points. Rivers: source to mouth. Lakes/Ocean: boundary contour."))
	TArray<FVector> SplinePoints;

	/** Width of the water body in centimetres (rivers: mouth width, used for zone bounds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water",
		meta = (ToolTip = "Width in cm. Rivers: mouth (maximum) width, used for zone bounds.", ClampMin = "1.0"))
	float Width = 200.0f;

	/**
	 * Per-spline-point widths in cm (parallel to SplinePoints).
	 * Rivers only: index 0 = source (narrow), last = mouth (wide).
	 * Applied to UWaterSplineMetadata::RiverWidth during spawning.
	 * Empty for lakes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water",
		meta = (ToolTip = "Per-point river widths in cm. Source is narrow, mouth is wide. Parallel to SplinePoints."))
	TArray<float> PointWidths;

	/** Depth of the water body in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water",
		meta = (ToolTip = "Depth in cm.", ClampMin = "1.0"))
	float Depth = 100.0f;

	/**
	 * Index of a linked water body in the WaterBodyDefinitions array.
	 * For a terminus lake (placed at a river's flat-basin end):
	 *   stores the index of the parent river definition.
	 * -1 = no link.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water")
	int32 LinkedBodyIndex = -1;
};
