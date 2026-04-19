// Copyright Priordium. All Rights Reserved.
//
// Test_ResourceQuery.cpp
// Tests GetResourcesInArea(), ClearResources() and IsInitialized()
// of UResourceDistributor.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UResourceDistributor.h"
#include "FResourceSpawnRule.h"
#include "Engine/World.h"

// -----------------------------------------------------------------------
// Test: IsInitialized() is false before any spawn
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ResourceQuery_IsInitialized_FalseInitially,
	"Priordium.MapGenerator.ResourceDistributor.ResourceQuery.IsInitialized_FalseInitially",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ResourceQuery_IsInitialized_FalseInitially::RunTest(const FString& Parameters)
{
	UResourceDistributor* Dist = NewObject<UResourceDistributor>(GetTransientPackage(), NAME_None, RF_Transient);
	TestFalse(TEXT("IsInitialized() is false before any spawn"), Dist->IsInitialized());
	return true;
}

// -----------------------------------------------------------------------
// Test: GetSpawnedResources() returns empty array initially
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ResourceQuery_SpawnedResources_EmptyInitially,
	"Priordium.MapGenerator.ResourceDistributor.ResourceQuery.SpawnedResources_EmptyInitially",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ResourceQuery_SpawnedResources_EmptyInitially::RunTest(const FString& Parameters)
{
	UResourceDistributor* Dist = NewObject<UResourceDistributor>(GetTransientPackage(), NAME_None, RF_Transient);
	TestEqual(TEXT("GetSpawnedResources() starts empty"), Dist->GetSpawnedResources().Num(), 0);
	return true;
}

// -----------------------------------------------------------------------
// Test: ClearResources() empties SpawnedResources
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ResourceQuery_ClearResources_Empties,
	"Priordium.MapGenerator.ResourceDistributor.ResourceQuery.ClearResources_Empties",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ResourceQuery_ClearResources_Empties::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	if (!World)
	{
		AddWarning(TEXT("GWorld is null, testing ClearResources on empty list"));
		UResourceDistributor* Dist = NewObject<UResourceDistributor>(GetTransientPackage(), NAME_None, RF_Transient);
		Dist->ClearResources();
		TestEqual(TEXT("ClearResources on empty list is safe"), Dist->GetSpawnedResources().Num(), 0);
		return true;
	}

	UResourceDistributor* Dist = NewObject<UResourceDistributor>(GetTransientPackage(), NAME_None, RF_Transient);

	// Spawn one actor to populate SpawnedResources
	AActor* Actor = Dist->SpawnResource(World, AActor::StaticClass(), FGameplayTag(), FVector::ZeroVector);
	if (TestNotNull(TEXT("Actor spawned for clear test"), Actor))
	{
		TestEqual(TEXT("SpawnedResources has 1 element before clear"), Dist->GetSpawnedResources().Num(), 1);
		Dist->ClearResources();
		TestEqual(TEXT("SpawnedResources is empty after ClearResources"), Dist->GetSpawnedResources().Num(), 0);
	}
	return true;
}

// -----------------------------------------------------------------------
// Test: GetResourcesInArea() returns empty when no resources
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ResourceQuery_GetResourcesInArea_EmptyWhenNone,
	"Priordium.MapGenerator.ResourceDistributor.ResourceQuery.GetResourcesInArea_EmptyWhenNone",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ResourceQuery_GetResourcesInArea_EmptyWhenNone::RunTest(const FString& Parameters)
{
	UResourceDistributor* Dist = NewObject<UResourceDistributor>(GetTransientPackage(), NAME_None, RF_Transient);

	const FBox LargeBox(FVector(-1e6f), FVector(1e6f));
	const TArray<AActor*> Result = Dist->GetResourcesInArea(LargeBox, FGameplayTag());

	TestEqual(TEXT("GetResourcesInArea returns empty when no resources exist"), Result.Num(), 0);
	return true;
}

// -----------------------------------------------------------------------
// Test: GetResourcesInArea() with valid spawn -- actor found in box
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_ResourceQuery_GetResourcesInArea_FindsActor,
	"Priordium.MapGenerator.ResourceDistributor.ResourceQuery.GetResourcesInArea_FindsActor",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_ResourceQuery_GetResourcesInArea_FindsActor::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	if (!World)
	{
		AddWarning(TEXT("GWorld is null, skipping GetResourcesInArea find test"));
		return true;
	}

	UResourceDistributor* Dist = NewObject<UResourceDistributor>(GetTransientPackage(), NAME_None, RF_Transient);

	const FVector SpawnLoc(100.0f, 200.0f, 0.0f);
	AActor* Actor = Dist->SpawnResource(World, AActor::StaticClass(), FGameplayTag(), SpawnLoc);

	if (TestNotNull(TEXT("Actor spawned for area query test"), Actor))
	{
		// Use the actor's actual location to build the search box.
		// Basic AActor (no root SceneComponent) may report GetActorLocation()
		// differently from the requested spawn position.
		const FVector ActorLoc = Actor->GetActorLocation();
		const FBox SearchBox(ActorLoc - FVector(50.0f), ActorLoc + FVector(50.0f));
		const TArray<AActor*> Found = Dist->GetResourcesInArea(SearchBox, FGameplayTag());

		TestEqual(TEXT("GetResourcesInArea finds the spawned actor in its actual box"), Found.Num(), 1);

		// A box that definitely does NOT contain the actor's actual location
		const FVector FarPoint(ActorLoc.X + 10000.0f, ActorLoc.Y + 10000.0f, ActorLoc.Z + 10000.0f);
		const FBox FarBox(FarPoint - FVector(50.0f), FarPoint + FVector(50.0f));
		const TArray<AActor*> NotFound = Dist->GetResourcesInArea(FarBox, FGameplayTag());
		TestEqual(TEXT("GetResourcesInArea returns empty for far box"), NotFound.Num(), 0);

		Dist->ClearResources();
	}
	return true;
}
