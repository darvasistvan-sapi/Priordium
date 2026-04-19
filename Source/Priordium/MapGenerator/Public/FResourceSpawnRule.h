// Copyright Priordium. All Rights Reserved.
//
// FResourceSpawnRule.h
// Structure describing the spawn rule for a resource type.
// SOLID: Single Responsibility -- only this one structure is in this file.

#pragma once

#include "CoreMinimal.h"
#include "EBiomeType.h"
#include "EClimateZone.h"
#include "GameplayTagContainer.h"
#include "FResourceSpawnRule.generated.h"

/**
 * FResourceSpawnRule
 * Defines on which biomes and climate zones a given resource type appears,
 * with what density, cluster configuration, and water proximity constraints.
 */
USTRUCT(BlueprintType)
struct PRIORDIUM_API FResourceSpawnRule
{
	GENERATED_BODY()

	/** GameplayTag identifier for this resource (e.g. Resource.Mineral.Iron, Resource.Animal.Deer). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource",
		meta = (ToolTip = "GameplayTag identifier for this resource type (e.g. Resource.Mineral.Iron)."))
	FGameplayTag ResourceTag;

	/**
	 * Actor classes to spawn for this resource. Can be Blueprints or C++ classes.
	 * Within a single cluster each class is used at most once before any class repeats,
	 * so the mesh variety inside a cluster equals min(ClusterSize, ActorClasses.Num()).
	 * If only one entry is provided the behaviour is identical to a uniform cluster.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource",
		meta = (ToolTip = "Actor classes to spawn for this resource. Each class appears at most once per cluster before repeating."))
	TArray<TSoftClassPtr<AActor>> ActorClasses;

	/** Biome types where this resource can spawn. Empty = all biomes allowed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource",
		meta = (ToolTip = "Biome types where this resource can spawn. Empty means all biomes allowed."))
	TArray<EBiomeType> AllowedBiomes;

	/** Climate zones where this resource can spawn. Empty = all climate zones allowed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource",
		meta = (ToolTip = "Climate zones where this resource can spawn. Empty means all climate zones allowed."))
	TArray<EClimateZone> AllowedClimateZones;

	/** Minimum normalized height [0.0, 1.0] for this resource to appear. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource",
		meta = (ToolTip = "Minimum normalized height [0.0, 1.0] for this resource to appear.",
				ClampMin = "0.0", ClampMax = "1.0"))
	float MinHeight = 0.0f;

	/** Maximum normalized height [0.0, 1.0] for this resource to appear. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource",
		meta = (ToolTip = "Maximum normalized height [0.0, 1.0] for this resource to appear.",
				ClampMin = "0.0", ClampMax = "1.0"))
	float MaxHeight = 1.0f;

	/** Spawn density [0.0, 1.0]. 0 = never, 1 = maximum density. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource",
		meta = (ToolTip = "Spawn density from 0.0 (none) to 1.0 (maximum).",
				ClampMin = "0.0", ClampMax = "1.0"))
	float Density = 0.5f;

	/** Minimum number of resources in a single cluster. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource",
		meta = (ToolTip = "Minimum number of resources in a single cluster.",
				ClampMin = "1", ClampMax = "100"))
	int32 ClusterSizeMin = 1;

	/** Maximum number of resources in a single cluster. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource",
		meta = (ToolTip = "Maximum number of resources in a single cluster.",
				ClampMin = "1", ClampMax = "100"))
	int32 ClusterSizeMax = 5;

	/** World-unit radius for cluster scatter around the cluster center. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource",
		meta = (ToolTip = "World-unit radius for cluster scatter around the cluster center.",
				ClampMin = "0.0"))
	float ClusterRadius = 500.0f;

	/** Minimum world-unit distance from any water body. 0 means no minimum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource",
		meta = (ToolTip = "Minimum world-unit distance from any water body. 0 means no minimum.",
				ClampMin = "0.0"))
	float MinWaterDistance = 0.0f;

	/** Maximum world-unit distance from any water body. Negative means no maximum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource",
		meta = (ToolTip = "Maximum world-unit distance from any water body. Negative means no maximum."))
	float MaxWaterDistance = -1.0f;
};
