// Copyright Priordium. All Rights Reserved.
//
// Test_SpawnResource.cpp
// Tests SpawnResource() and CalculateResourceCount() of UResourceDistributor.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UResourceDistributor.h"
#include "FResourceSpawnRule.h"
#include "Engine/World.h"

// -----------------------------------------------------------------------
// Test: Null World â†’ returns nullptr
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_SpawnResource_NullWorld,
	"Priordium.MapGenerator.ResourceDistributor.SpawnResource.NullWorld",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_SpawnResource_NullWorld::RunTest(const FString& Parameters)
{
	UResourceDistributor* Dist = NewObject<UResourceDistributor>(GetTransientPackage(), NAME_None, RF_Transient);

	AddExpectedError(TEXT("World is null"), EAutomationExpectedErrorFlags::Contains, 1);
	AActor* Result = Dist->SpawnResource(nullptr, AActor::StaticClass(), FGameplayTag(), FVector::ZeroVector);
	TestNull(TEXT("Null World returns nullptr"), Result);
	TestEqual(TEXT("SpawnedResources remains empty"), Dist->GetSpawnedResources().Num(), 0);
	return true;
}

// -----------------------------------------------------------------------
// Test: Null Class â†’ returns nullptr
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_SpawnResource_NullClass,
	"Priordium.MapGenerator.ResourceDistributor.SpawnResource.NullClass",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_SpawnResource_NullClass::RunTest(const FString& Parameters)
{
	UResourceDistributor* Dist = NewObject<UResourceDistributor>(GetTransientPackage(), NAME_None, RF_Transient);

	// Use GWorld if available; skip actual spawn
	UWorld* World = GWorld;
	if (!World)
	{
		AddWarning(TEXT("GWorld is null, testing null class guard only"));
		AddExpectedError(TEXT("World is null"), EAutomationExpectedErrorFlags::Contains, 1);
		AActor* Result = Dist->SpawnResource(nullptr, nullptr, FGameplayTag(), FVector::ZeroVector);
		TestNull(TEXT("Null World+Class returns nullptr"), Result);
		return true;
	}

	AddExpectedError(TEXT("LoadedClass is null"), EAutomationExpectedErrorFlags::Contains, 1);
	AActor* Result = Dist->SpawnResource(World, nullptr, FGameplayTag(), FVector::ZeroVector);
	TestNull(TEXT("Null Class returns nullptr"), Result);
	TestEqual(TEXT("SpawnedResources not incremented on failure"), Dist->GetSpawnedResources().Num(), 0);
	return true;
}

// -----------------------------------------------------------------------
// Test: Valid spawn â†’ SpawnedResources grows
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_SpawnResource_SpawnedResources_Grows,
	"Priordium.MapGenerator.ResourceDistributor.SpawnResource.SpawnedResources_Grows",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_SpawnResource_SpawnedResources_Grows::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	if (!World)
	{
		AddWarning(TEXT("GWorld is null, skipping actual spawn test"));
		return true;
	}

	UResourceDistributor* Dist = NewObject<UResourceDistributor>(GetTransientPackage(), NAME_None, RF_Transient);
	TestEqual(TEXT("SpawnedResources starts empty"), Dist->GetSpawnedResources().Num(), 0);

	AActor* Actor = Dist->SpawnResource(World, AActor::StaticClass(), FGameplayTag(), FVector::ZeroVector);

	if (TestNotNull(TEXT("SpawnResource returns non-null actor"), Actor))
	{
		TestEqual(TEXT("SpawnedResources has 1 element after spawn"), Dist->GetSpawnedResources().Num(), 1);
		Actor->Destroy();
	}
	return true;
}

// -----------------------------------------------------------------------
// Test: CalculateResourceCount -- density-based count
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_SpawnResource_CalculateResourceCount,
	"Priordium.MapGenerator.ResourceDistributor.SpawnResource.CalculateResourceCount",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_SpawnResource_CalculateResourceCount::RunTest(const FString& Parameters)
{
	UResourceDistributor* Dist = NewObject<UResourceDistributor>(GetTransientPackage(), NAME_None, RF_Transient);

	FResourceSpawnRule Rule;

	Rule.Density = 0.0f;
	TestEqual(TEXT("Zero area â†’ 0 resources"), Dist->CalculateResourceCount(Rule, 0), 0);

	Rule.Density = 1.0f;
	const int32 Count = Dist->CalculateResourceCount(Rule, 1000);
	TestTrue(TEXT("Density=1.0, area=1000 â†’ at least 1 resource"), Count >= 1);
	TestTrue(TEXT("Density=1.0, area=1000 â†’ count = round(1000*1.0*0.01) = 10"), Count == 10);

	Rule.Density = 0.5f;
	const int32 HalfCount = Dist->CalculateResourceCount(Rule, 1000);
	TestTrue(TEXT("Density=0.5 count is less than Density=1.0 count"), HalfCount <= Count);

	return true;
}
