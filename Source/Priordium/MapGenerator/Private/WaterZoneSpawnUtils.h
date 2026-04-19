#pragma once

#include "CoreMinimal.h"
#include "FWaterBodyDefinition.h"

class AActor;
class UClass;
class UWorld;

namespace WaterZoneSpawnUtils
{
	void MakeActorComponentsMovable(AActor* Actor);
	void MakeSplineMeshComponentsMovable(AActor* Actor);

	struct FWaterZoneLayout
	{
		FVector2D Center;
		FVector2D HalfSize;
		float MapSize = 0.0f;
		float WidthPaddingFactor = 0.0f;
		float WidthPadding = 0.0f;
		float ZonePaddingRaw = 0.0f;
		float ZonePaddingScale = 0.0f;
		float ZonePadding = 0.0f;
		FVector2D ZoneExtent;
		FVector ZoneLocation = FVector::ZeroVector;
		int32 TargetTilesPerAxis = 0;
		float DesiredTileSize = 0.0f;
	};

	bool TryBuildWaterZoneLayout(
		const FWaterBodyDefinition& Def,
		int32 ZoneIndex,
		float CellSize,
		FWaterZoneLayout& OutLayout);

	AActor* SpawnZoneActor(UWorld* World, UClass* ZoneClass, const FVector& ZoneLocation);
	void ApplyZoneExtent(AActor* Zone, const FVector2D& ZoneExtent);
	int32 ConfigureWaterMeshTiling(AActor* Zone, int32 ZoneIndex, int32 TargetTilesPerAxis, float DesiredTileSize);
	void ApplyBoundsBoxExtent(AActor* Zone, int32 ZoneIndex, const FVector2D& ZoneExtent);
}
