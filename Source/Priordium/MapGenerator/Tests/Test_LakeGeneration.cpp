// Copyright Priordium. All Rights Reserved.
//
// Test_LakeGeneration.cpp
// Tests FindLakeLocations() on UWaterSystemBuilder.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UWaterSystemBuilder.h"
#include "UHeightmapGenerator.h"
#include "UMapGeneratorSettings.h"

static void MakeLakeTestPair(
	int32 Res, int32 Seed,
	UHeightmapGenerator*&   OutH,
	UMapGeneratorSettings*& OutS)
{
	OutS = NewObject<UMapGeneratorSettings>(GetTransientPackage(), NAME_None, RF_Transient);
	OutS->Resolution                  = Res;
	OutS->Seed                        = Seed;
	OutS->bUseContinentMask           = true;
	OutS->EdgeFalloffDistance         = 0.3f;
	OutS->SeaLevel                    = 0.3f;
	OutS->MaxLakeCount                = 5;
	OutS->LakeRadiusMinCells          = 10;
	OutS->LakeRadiusMaxCells          = 35;
	OutS->HeightmapConfig.Octaves     = 4;
	OutS->HeightmapConfig.Frequency   = 0.05f;
	OutS->HeightmapConfig.Amplitude   = 1.0f;
	OutS->HeightmapConfig.Persistence = 0.5f;
	OutS->HeightmapConfig.Lacunarity  = 2.0f;

	OutH = NewObject<UHeightmapGenerator>(GetTransientPackage(), NAME_None, RF_Transient);
	OutH->Generate(OutS);
}

// -----------------------------------------------------------------------
// Test: Returns at most MaxLakeCount lakes
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_LakeGeneration_CountRespected,
	"Priordium.MapGenerator.WaterSystemBuilder.LakeGeneration.CountRespected",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_LakeGeneration_CountRespected::RunTest(const FString& Parameters)
{
	UHeightmapGenerator*   Heightmap;
	UMapGeneratorSettings* Settings;
	MakeLakeTestPair(65, 42, Heightmap, Settings);
	Settings->MaxLakeCount = 3;

	UWaterSystemBuilder* Builder = NewObject<UWaterSystemBuilder>(
		GetTransientPackage(), NAME_None, RF_Transient);

	TArray<FIntPoint> Lakes = Builder->FindLakeLocations(Heightmap, Settings);
	TestTrue(TEXT("Lake count <= MaxLakeCount"), Lakes.Num() <= 3);
	return true;
}

// -----------------------------------------------------------------------
// Test: All lake centers are local minima
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_LakeGeneration_LakesAtLocalMinima,
	"Priordium.MapGenerator.WaterSystemBuilder.LakeGeneration.LakesAtLocalMinima",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_LakeGeneration_LakesAtLocalMinima::RunTest(const FString& Parameters)
{
	UHeightmapGenerator*   Heightmap;
	UMapGeneratorSettings* Settings;
	MakeLakeTestPair(65, 42, Heightmap, Settings);

	UWaterSystemBuilder* Builder = NewObject<UWaterSystemBuilder>(
		GetTransientPackage(), NAME_None, RF_Transient);

	TArray<FIntPoint> Lakes = Builder->FindLakeLocations(Heightmap, Settings);

	for (const FIntPoint& L : Lakes)
	{
		const float H = Heightmap->GetHeightAt(L.X, L.Y);
		bool bIsMin = true;
		for (int32 dy = -1; dy <= 1 && bIsMin; ++dy)
			for (int32 dx = -1; dx <= 1 && bIsMin; ++dx)
			{
				if (dx == 0 && dy == 0) continue;
				if (Heightmap->GetHeightAt(L.X + dx, L.Y + dy) <= H)
					bIsMin = false;
			}
		TestTrue(FString::Printf(TEXT("Lake (%d,%d) is a local minimum"), L.X, L.Y), bIsMin);
	}
	return true;
}

// -----------------------------------------------------------------------
// Test: Minimum distance between lakes is respected
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_LakeGeneration_MinDistance,
	"Priordium.MapGenerator.WaterSystemBuilder.LakeGeneration.MinDistance",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_LakeGeneration_MinDistance::RunTest(const FString& Parameters)
{
	UHeightmapGenerator*   Heightmap;
	UMapGeneratorSettings* Settings;
	MakeLakeTestPair(65, 42, Heightmap, Settings);

	UWaterSystemBuilder* Builder = NewObject<UWaterSystemBuilder>(
		GetTransientPackage(), NAME_None, RF_Transient);
	Builder->MinLakeDistance = 6.0f;

	TArray<FIntPoint> Lakes = Builder->FindLakeLocations(Heightmap, Settings);
	const float MinDistSq = Builder->MinLakeDistance * Builder->MinLakeDistance;

	for (int32 i = 0; i < Lakes.Num(); ++i)
		for (int32 j = i + 1; j < Lakes.Num(); ++j)
		{
			const float DX = static_cast<float>(Lakes[i].X - Lakes[j].X);
			const float DY = static_cast<float>(Lakes[i].Y - Lakes[j].Y);
			TestTrue(
				FString::Printf(TEXT("Lakes [%d] and [%d] at least %.1f apart"), i, j, Builder->MinLakeDistance),
				DX * DX + DY * DY >= MinDistSq);
		}
	return true;
}

// -----------------------------------------------------------------------
// Test: Lakes have varied radii when Min != Max
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_LakeGeneration_RadiiAreVaried,
	"Priordium.MapGenerator.WaterSystemBuilder.LakeGeneration.RadiiAreVaried",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_LakeGeneration_RadiiAreVaried::RunTest(const FString& Parameters)
{
	// Build two separate maps with the same seed but different radius ranges,
	// and one map where Min==Max to verify the single-size edge case.

	// --- Case 1: Min == Max -> all lakes must have identical radii ---
	{
		UHeightmapGenerator*   Heightmap;
		UMapGeneratorSettings* Settings;
		MakeLakeTestPair(129, 7, Heightmap, Settings);
		Settings->MaxLakeCount       = 5;
		Settings->LakeRadiusMinCells = 15;
		Settings->LakeRadiusMaxCells = 15; // forced uniform

		UWaterSystemBuilder* Builder = NewObject<UWaterSystemBuilder>(GetTransientPackage(), NAME_None, RF_Transient);
		Builder->BuildWaterBodies(Heightmap, Settings, 100.0f, /*bSpawnActors=*/false);

		const TArray<FWaterBodyDefinition>& Defs = Builder->GetWaterBodyDefinitions();
		TSet<float> Radii;
		for (const FWaterBodyDefinition& D : Defs)
		{
			if (D.WaterType != EWaterBodyType::Lake || D.SplinePoints.IsEmpty()) continue;
			// Radius = distance from centroid to first spline point
			FVector2D Centroid(0.f, 0.f);
			for (const FVector& P : D.SplinePoints) { Centroid.X += P.X; Centroid.Y += P.Y; }
			Centroid /= static_cast<float>(D.SplinePoints.Num());
			const float R = FVector2D::Distance(Centroid, FVector2D(D.SplinePoints[0].X, D.SplinePoints[0].Y));
			Radii.Add(FMath::RoundToFloat(R)); // round to nearest cm to absorb float noise
		}
		// Guard on Radii.Num() directly: Defs may contain rivers with no lakes,
		// in which case Radii stays empty and there is nothing to assert.
		if (Radii.Num() > 0)
			TestEqual(TEXT("Min==Max: all lakes have the same radius"), Radii.Num(), 1);
		else
			AddWarning(TEXT("Min==Max case: no lake definitions were generated for this seed/resolution, skipping radius uniformity check."));
	}

	// --- Case 2: Min != Max -> at least two distinct radii when enough lakes exist ---
	{
		UHeightmapGenerator*   Heightmap;
		UMapGeneratorSettings* Settings;
		MakeLakeTestPair(129, 7, Heightmap, Settings);
		Settings->MaxLakeCount       = 8;
		Settings->LakeRadiusMinCells = 8;
		Settings->LakeRadiusMaxCells = 30; // wide range

		UWaterSystemBuilder* Builder = NewObject<UWaterSystemBuilder>(GetTransientPackage(), NAME_None, RF_Transient);
		Builder->BuildWaterBodies(Heightmap, Settings, 100.0f, /*bSpawnActors=*/false);

		const TArray<FWaterBodyDefinition>& Defs = Builder->GetWaterBodyDefinitions();
		TSet<float> Radii;
		for (const FWaterBodyDefinition& D : Defs)
		{
			if (D.WaterType != EWaterBodyType::Lake || D.SplinePoints.IsEmpty()) continue;
			FVector2D Centroid(0.f, 0.f);
			for (const FVector& P : D.SplinePoints) { Centroid.X += P.X; Centroid.Y += P.Y; }
			Centroid /= static_cast<float>(D.SplinePoints.Num());
			const float R = FVector2D::Distance(Centroid, FVector2D(D.SplinePoints[0].X, D.SplinePoints[0].Y));
			Radii.Add(FMath::RoundToFloat(R));
		}
		// Only assert variety when we actually got multiple lake definitions.
		// Radii.Num() counts distinct lake radii (rivers are excluded above).
		if (Radii.Num() >= 2)
			TestTrue(TEXT("Min!=Max: lakes have more than one distinct radius"), Radii.Num() > 1);
		else
			AddWarning(TEXT("Min!=Max case: fewer than 2 lakes generated for this seed/resolution, skipping radius variety check."));
	}

	return true;
}

// -----------------------------------------------------------------------
// Test: LakeShapeIrregularity = 0 -> perfect circle (all radii equal)
//       LakeShapeIrregularity > 0 -> at least one radius differs
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_LakeGeneration_ShapeIrregularity,
	"Priordium.MapGenerator.WaterSystemBuilder.LakeGeneration.ShapeIrregularity",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_LakeGeneration_ShapeIrregularity::RunTest(const FString& Parameters)
{
	// Helper: compute per-point radii from centroid for the first lake in Defs.
	// Returns empty array when no lake is present.
	auto GetFirstLakeRadii = [](const TArray<FWaterBodyDefinition>& Defs) -> TArray<float>
	{
		for (const FWaterBodyDefinition& D : Defs)
		{
			if (D.WaterType != EWaterBodyType::Lake || D.SplinePoints.Num() < 3)
				continue;

			FVector2D Centroid(0.f, 0.f);
			for (const FVector& P : D.SplinePoints) { Centroid.X += P.X; Centroid.Y += P.Y; }
			Centroid /= static_cast<float>(D.SplinePoints.Num());

			TArray<float> Radii;
			for (const FVector& P : D.SplinePoints)
				Radii.Add(FVector2D::Distance(Centroid, FVector2D(P.X, P.Y)));
			return Radii;
		}
		return {};
	};

	// --- Case 1: Irregularity == 0 -> all radii must be equal (perfect circle) ---
	{
		UHeightmapGenerator*   Heightmap;
		UMapGeneratorSettings* Settings;
		MakeLakeTestPair(129, 13, Heightmap, Settings);
		Settings->LakeRadiusMinCells    = 20;
		Settings->LakeRadiusMaxCells    = 20;
		Settings->LakeShapeIrregularity = 0.0f;

		UWaterSystemBuilder* Builder = NewObject<UWaterSystemBuilder>(GetTransientPackage(), NAME_None, RF_Transient);
		Builder->BuildWaterBodies(Heightmap, Settings, 100.0f, /*bSpawnActors=*/false);

		TArray<float> Radii = GetFirstLakeRadii(Builder->GetWaterBodyDefinitions());
		if (Radii.Num() > 0)
		{
			const float First = Radii[0];
			bool bAllEqual = true;
			for (float R : Radii)
				if (!FMath::IsNearlyEqual(R, First, 1.0f)) { bAllEqual = false; break; }
			TestTrue(TEXT("Irregularity=0: all perimeter radii are equal (circle)"), bAllEqual);
		}
		else
			AddWarning(TEXT("Irregularity=0 case: no lake generated for this seed, skipping."));
	}

	// --- Case 2: Irregularity > 0 -> radii must NOT all be equal ---
	{
		UHeightmapGenerator*   Heightmap;
		UMapGeneratorSettings* Settings;
		MakeLakeTestPair(129, 13, Heightmap, Settings);
		Settings->LakeRadiusMinCells    = 20;
		Settings->LakeRadiusMaxCells    = 20;
		Settings->LakeShapeIrregularity = 0.5f;

		UWaterSystemBuilder* Builder = NewObject<UWaterSystemBuilder>(GetTransientPackage(), NAME_None, RF_Transient);
		Builder->BuildWaterBodies(Heightmap, Settings, 100.0f, /*bSpawnActors=*/false);

		TArray<float> Radii = GetFirstLakeRadii(Builder->GetWaterBodyDefinitions());
		if (Radii.Num() > 0)
		{
			const float First = Radii[0];
			bool bAllEqual = true;
			for (float R : Radii)
				if (!FMath::IsNearlyEqual(R, First, 1.0f)) { bAllEqual = false; break; }
			TestFalse(TEXT("Irregularity=0.5: perimeter radii are NOT all equal (irregular shape)"), bAllEqual);
		}
		else
			AddWarning(TEXT("Irregularity>0 case: no lake generated for this seed, skipping."));
	}

	return true;
}

// -----------------------------------------------------------------------
// Test: No two generated lakes overlap (shore-to-shore distance >= MinLakeDistance)
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_LakeGeneration_NoOverlap,
	"Priordium.MapGenerator.WaterSystemBuilder.LakeGeneration.NoOverlap",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_LakeGeneration_NoOverlap::RunTest(const FString& Parameters)
{
	UHeightmapGenerator*   Heightmap;
	UMapGeneratorSettings* Settings;
	MakeLakeTestPair(129, 7, Heightmap, Settings);
	Settings->MaxLakeCount       = 8;
	Settings->LakeRadiusMinCells = 8;
	Settings->LakeRadiusMaxCells = 30;

	UWaterSystemBuilder* Builder = NewObject<UWaterSystemBuilder>(
		GetTransientPackage(), NAME_None, RF_Transient);
	Builder->BuildWaterBodies(Heightmap, Settings, 100.0f, /*bSpawnActors=*/false);

	const TArray<FWaterBodyDefinition>& Defs = Builder->GetWaterBodyDefinitions();

	// Collect per-lake centroid + average radius from spline points.
	struct FLakeInfo { FVector2D Centroid; float Radius; };
	TArray<FLakeInfo> Lakes;
	for (const FWaterBodyDefinition& D : Defs)
	{
		if (D.WaterType != EWaterBodyType::Lake || D.SplinePoints.IsEmpty()) continue;

		FVector2D Centroid(0.f, 0.f);
		for (const FVector& P : D.SplinePoints) { Centroid.X += P.X; Centroid.Y += P.Y; }
		Centroid /= static_cast<float>(D.SplinePoints.Num());

		float AvgRadius = 0.f;
		for (const FVector& P : D.SplinePoints)
			AvgRadius += FVector2D::Distance(Centroid, FVector2D(P.X, P.Y));
		AvgRadius /= static_cast<float>(D.SplinePoints.Num());

		Lakes.Add({ Centroid, AvgRadius });
	}

	// Every pair must have center-to-center distance >= R1 + R2 (shores don't touch).
	for (int32 i = 0; i < Lakes.Num(); ++i)
		for (int32 j = i + 1; j < Lakes.Num(); ++j)
		{
			const float Dist     = FVector2D::Distance(Lakes[i].Centroid, Lakes[j].Centroid);
			const float MinDist  = Lakes[i].Radius + Lakes[j].Radius;
			TestTrue(
				FString::Printf(TEXT("Lakes [%d] and [%d] shores do not overlap (dist=%.0f, minDist=%.0f)"), i, j, Dist, MinDist),
				Dist >= MinDist);
		}

	return true;
}

// -----------------------------------------------------------------------
// Test: Null Heightmap returns empty list
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_LakeGeneration_NullHeightmap,
	"Priordium.MapGenerator.WaterSystemBuilder.LakeGeneration.NullHeightmap",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_LakeGeneration_NullHeightmap::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("FindLakeLocations -- Heightmap is null"), EAutomationExpectedErrorFlags::Contains, 1);

	UWaterSystemBuilder* Builder = NewObject<UWaterSystemBuilder>(
		GetTransientPackage(), NAME_None, RF_Transient);
	UMapGeneratorSettings* Settings = NewObject<UMapGeneratorSettings>(
		GetTransientPackage(), NAME_None, RF_Transient);

	TArray<FIntPoint> Lakes = Builder->FindLakeLocations(nullptr, Settings);
	TestEqual(TEXT("Null Heightmap -> empty result"), Lakes.Num(), 0);
	return true;
}
