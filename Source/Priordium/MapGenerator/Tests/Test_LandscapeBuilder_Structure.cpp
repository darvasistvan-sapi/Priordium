// Copyright Priordium. All Rights Reserved.
//
// Test_LandscapeBuilder_Structure.cpp
// Tests the basic structure of the ULandscapeBuilder component.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"

#include "ULandscapeBuilder.h"

// -----------------------------------------------------------------------
// Test: ULandscapeBuilder component can be instantiated
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_LandscapeBuilder_Instantiable,
	"Priordium.MapGenerator.LandscapeBuilder.Structure.Instantiable",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_LandscapeBuilder_Instantiable::RunTest(const FString& Parameters)
{
	ULandscapeBuilder* Builder = NewObject<ULandscapeBuilder>();
	TestNotNull(TEXT("ULandscapeBuilder is not null"), Builder);
	return true;
}

// -----------------------------------------------------------------------
// Test: No generated Landscape by default
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_LandscapeBuilder_DefaultState,
	"Priordium.MapGenerator.LandscapeBuilder.Structure.DefaultState",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_LandscapeBuilder_DefaultState::RunTest(const FString& Parameters)
{
	ULandscapeBuilder* Builder = NewObject<ULandscapeBuilder>();
	if (!TestNotNull(TEXT("Builder is not null"), Builder)) return false;

	TestNull(TEXT("GetGeneratedLandscape() is null by default"), Builder->GetGeneratedLandscape());
	TestFalse(TEXT("IsLandscapeCreated() is false by default"), Builder->IsLandscapeCreated());
	return true;
}

// -----------------------------------------------------------------------
// Test: LayerInfoAssets TMap is accessible and modifiable
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_LandscapeBuilder_LayerInfoMapAccessible,
	"Priordium.MapGenerator.LandscapeBuilder.Structure.LayerInfoMapAccessible",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_LandscapeBuilder_LayerInfoMapAccessible::RunTest(const FString& Parameters)
{
	ULandscapeBuilder* Builder = NewObject<ULandscapeBuilder>();
	if (!TestNotNull(TEXT("Builder is not null"), Builder)) return false;

	// Empty by default
	TestEqual(TEXT("LayerInfoAssets is empty by default"), Builder->LayerInfoAssets.Num(), 0);

	// Element can be added (null value is acceptable for structure testing)
	Builder->LayerInfoAssets.Add(EBiomeType::Ocean, nullptr);
	TestEqual(TEXT("LayerInfoAssets.Num() == 1 after add"), Builder->LayerInfoAssets.Num(), 1);
	TestTrue(TEXT("Ocean key found"), Builder->LayerInfoAssets.Contains(EBiomeType::Ocean));

	return true;
}

// -----------------------------------------------------------------------
// Test: ULandscapeBuilder can be attached to an Actor
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_LandscapeBuilder_AttachableToActor,
	"Priordium.MapGenerator.LandscapeBuilder.Structure.AttachableToActor",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_LandscapeBuilder_AttachableToActor::RunTest(const FString& Parameters)
{
	AActor* TestActor = NewObject<AActor>();
	if (!TestNotNull(TEXT("TestActor is not null"), TestActor)) return false;

	ULandscapeBuilder* Builder = NewObject<ULandscapeBuilder>(TestActor);
	if (!TestNotNull(TEXT("Builder is not null"), Builder)) return false;

	TestTrue(TEXT("Builder outer is the TestActor"), Builder->GetOuter() == TestActor);
	return true;
}

// -----------------------------------------------------------------------
// Test: UClass reflection is accessible
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_LandscapeBuilder_ClassReflection,
	"Priordium.MapGenerator.LandscapeBuilder.Structure.ClassReflection",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_LandscapeBuilder_ClassReflection::RunTest(const FString& Parameters)
{
	const UClass* BuilderClass = ULandscapeBuilder::StaticClass();
	TestNotNull(TEXT("ULandscapeBuilder::StaticClass() is not null"), BuilderClass);
	TestTrue(TEXT("ULandscapeBuilder is a descendant of UActorComponent"),
		BuilderClass->IsChildOf(UActorComponent::StaticClass()));
	return true;
}

