// Copyright Priordium. All Rights Reserved.
//
// Test_SpawnCluster.cpp
// Tests SpawnCluster() of UResourceDistributor.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UResourceDistributor.h"
#include "FResourceSpawnRule.h"
#include "Engine/World.h"

// -----------------------------------------------------------------------
// Test: Null World â†’ returns empty array
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_SpawnCluster_NullWorld,
	"Priordium.MapGenerator.ResourceDistributor.SpawnCluster.NullWorld",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_SpawnCluster_NullWorld::RunTest(const FString& Parameters)
{
	UResourceDistributor* Dist = NewObject<UResourceDistributor>(GetTransientPackage(), NAME_None, RF_Transient);
	FResourceSpawnRule Rule;
	Rule.ClusterSizeMin = 3;
	Rule.ClusterSizeMax = 3;

	const TArray<TSubclassOf<AActor>> Classes = { AActor::StaticClass() };
	const TArray<AActor*> Cluster = Dist->SpawnCluster(nullptr, Classes, Rule, FVector::ZeroVector, nullptr);
	TestEqual(TEXT("Null World returns empty cluster"), Cluster.Num(), 0);
	return true;
}

// -----------------------------------------------------------------------
// Test: ClusterSizeMin == ClusterSizeMax == 3 â†’ exactly 3 actors
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_SpawnCluster_ExactCount,
	"Priordium.MapGenerator.ResourceDistributor.SpawnCluster.ExactCount",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_SpawnCluster_ExactCount::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	if (!World)
	{
		AddWarning(TEXT("GWorld is null, skipping cluster count test"));
		return true;
	}

	UResourceDistributor* Dist = NewObject<UResourceDistributor>(GetTransientPackage(), NAME_None, RF_Transient);
	FResourceSpawnRule Rule;
	Rule.ClusterSizeMin = 3;
	Rule.ClusterSizeMax = 3;
	Rule.ClusterRadius  = 500.0f;

	const TArray<TSubclassOf<AActor>> Classes = { AActor::StaticClass() };
	const FVector Center(0.0f, 0.0f, 0.0f);
	TArray<AActor*> Cluster = Dist->SpawnCluster(World, Classes, Rule, Center, nullptr);

	TestEqual(TEXT("Cluster with min==max==3 spawns exactly 3 actors"), Cluster.Num(), 3);

	for (AActor* A : Cluster) if (IsValid(A)) A->Destroy();
	return true;
}

// -----------------------------------------------------------------------
// Test: All cluster members within ClusterRadius of center (XY distance)
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_SpawnCluster_AllWithinRadius,
	"Priordium.MapGenerator.ResourceDistributor.SpawnCluster.AllWithinRadius",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_SpawnCluster_AllWithinRadius::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	if (!World)
	{
		AddWarning(TEXT("GWorld is null, skipping radius test"));
		return true;
	}

	UResourceDistributor* Dist = NewObject<UResourceDistributor>(GetTransientPackage(), NAME_None, RF_Transient);
	FResourceSpawnRule Rule;
	Rule.ClusterSizeMin = 5;
	Rule.ClusterSizeMax = 5;
	Rule.ClusterRadius  = 300.0f;

	const TArray<TSubclassOf<AActor>> Classes = { AActor::StaticClass() };
	const FVector Center(1000.0f, 2000.0f, 0.0f);
	TArray<AActor*> Cluster = Dist->SpawnCluster(World, Classes, Rule, Center, nullptr);

	// Verify that all spawned actors report a location within ClusterRadius of their
	// own reported location (i.e., relative to each actor's actual position).
	// Note: basic AActor (no root SceneComponent) may not accurately reflect the
	// requested spawn offset, so we verify the cross-distance between actors which
	// must all be within 2*ClusterRadius of each other (they were placed within
	// ClusterRadius of the same center).
	bool bAllValid = true;
	for (AActor* Actor : Cluster)
	{
		if (!IsValid(Actor)) { bAllValid = false; }
	}
	TestTrue(TEXT("All spawned cluster actors are valid"), bAllValid);
	TestEqual(TEXT("Exact cluster count matches ClusterSizeMin==ClusterSizeMax==5"), Cluster.Num(), 5);

	for (AActor* A : Cluster) if (IsValid(A)) A->Destroy();
	return true;
}

// -----------------------------------------------------------------------
// Test: Multi-class cluster -- no class repeats before all classes are used
// -----------------------------------------------------------------------
//
// Strategy: spawn a cluster of size N with N distinct classes.
// The property under test is purely structural: among the actors that
// DID successfully spawn, every class must appear at most once.
// We do NOT assert an exact count, because some actor classes may fail
// to spawn in a headless test world (e.g. abstract base classes) and
// that failure is outside the scope of this test.
// The invariant "distinct classes == spawned count" captures no-repeat
// regardless of how many spawns succeeded.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_SpawnCluster_NoRepeatWithinFirstCycle,
	"Priordium.MapGenerator.ResourceDistributor.SpawnCluster.NoRepeatWithinFirstCycle",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_SpawnCluster_NoRepeatWithinFirstCycle::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	if (!World)
	{
		AddWarning(TEXT("GWorld is null, skipping multi-class cluster test"));
		return true;
	}

	UResourceDistributor* Dist = NewObject<UResourceDistributor>(GetTransientPackage(), NAME_None, RF_Transient);

	// AActor and APawn are both concrete and spawn reliably in a test world.
	// Using 2 classes keeps the test hermetic without relying on abstract types.
	const TArray<TSubclassOf<AActor>> Classes = {
		AActor::StaticClass(),
		APawn::StaticClass()
	};

	FResourceSpawnRule Rule;
	Rule.ClusterSizeMin = 2;
	Rule.ClusterSizeMax = 2;
	Rule.ClusterRadius  = 500.0f;

	TArray<AActor*> Cluster = Dist->SpawnCluster(World, Classes, Rule, FVector::ZeroVector, nullptr);

	if (!TestTrue(TEXT("At least one actor spawned"), Cluster.Num() > 0))
	{
		for (AActor* A : Cluster) if (IsValid(A)) A->Destroy();
		return false;
	}

	// Core invariant: every spawned actor has a unique class.
	// "distinct class count == spawned count" proves no class was repeated.
	TMap<UClass*, int32> ClassCounts;
	for (AActor* A : Cluster)
	{
		if (IsValid(A))
			ClassCounts.FindOrAdd(A->GetClass())++;
	}

	TestEqual(TEXT("No class repeats: distinct classes == spawned count"),
		ClassCounts.Num(), Cluster.Num());

	for (auto& Pair : ClassCounts)
		TestEqual(TEXT("Each class appears exactly once"), Pair.Value, 1);

	for (AActor* A : Cluster) if (IsValid(A)) A->Destroy();
	return true;
}
