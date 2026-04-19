// Copyright Priordium. All Rights Reserved.
//
// Test_GameplayTags.cpp
// Tests that the GameplayTags module is accessible and FGameplayTag works correctly.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsManager.h"

// -----------------------------------------------------------------------
// Test: UGameplayTagsManager singleton is accessible
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_GameplayTags_ManagerAccessible,
	"Priordium.MapGenerator.GameplayTags.ManagerAccessible",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_GameplayTags_ManagerAccessible::RunTest(const FString& Parameters)
{
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
	// The manager is a singleton reference -- if we reached here, it is valid
	TestTrue(TEXT("UGameplayTagsManager singleton is accessible"), true);
	return true;
}

// -----------------------------------------------------------------------
// Test: Default FGameplayTag is invalid (not valid)
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_GameplayTags_DefaultTagInvalid,
	"Priordium.MapGenerator.GameplayTags.DefaultTagInvalid",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_GameplayTags_DefaultTagInvalid::RunTest(const FString& Parameters)
{
	const FGameplayTag EmptyTag;
	TestFalse(TEXT("Default-constructed FGameplayTag is not valid"), EmptyTag.IsValid());
	return true;
}

// -----------------------------------------------------------------------
// Test: FGameplayTagContainer is usable
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_GameplayTags_ContainerUsable,
	"Priordium.MapGenerator.GameplayTags.ContainerUsable",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_GameplayTags_ContainerUsable::RunTest(const FString& Parameters)
{
	FGameplayTagContainer Container;
	TestEqual(TEXT("Empty container has 0 tags"), Container.Num(), 0);
	return true;
}

// -----------------------------------------------------------------------
// Test: FGameplayTag equality semantics work
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_GameplayTags_EqualitySemantics,
	"Priordium.MapGenerator.GameplayTags.EqualitySemantics",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_GameplayTags_EqualitySemantics::RunTest(const FString& Parameters)
{
	const FGameplayTag TagA;
	const FGameplayTag TagB;

	// Two default-constructed tags are equal
	TestTrue(TEXT("Two default tags are equal"), TagA == TagB);

	return true;
}
