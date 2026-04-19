// Copyright Priordium. All Rights Reserved.
//
// Test_HeightmapGenerator_Lifecycle.cpp
// Tests the lifecycle management and re-initialization of UHeightmapGenerator.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include "UHeightmapGenerator.h"

// -----------------------------------------------------------------------
// Test: Component can be attached to an Actor
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_HeightmapGenerator_AttachableToActor,
	"Priordium.MapGenerator.HeightmapGenerator.Lifecycle.AttachableToActor",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_HeightmapGenerator_AttachableToActor::RunTest(const FString& Parameters)
{
	UHeightmapGenerator* Gen = NewObject<UHeightmapGenerator>();
	TestNotNull(TEXT("UHeightmapGenerator NewObject succeeded"), Gen);

	TestTrue(TEXT("UHeightmapGenerator is a UActorComponent descendant"),
		Gen->IsA(UActorComponent::StaticClass()));
	return true;
}

// -----------------------------------------------------------------------
// Test: Multiple initializations do not crash, and data is updated
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_HeightmapGenerator_MultipleInit,
	"Priordium.MapGenerator.HeightmapGenerator.Lifecycle.MultipleInit",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_HeightmapGenerator_MultipleInit::RunTest(const FString& Parameters)
{
	UHeightmapGenerator* Gen = NewObject<UHeightmapGenerator>();
	if (!TestNotNull(TEXT("Gen is not null"), Gen)) return false;

	Gen->InitializeHeightmap(32, 32);
	TestEqual(TEXT("First init: HeightmapData size 32*32"), Gen->GetHeightmapData().Num(), 32 * 32);
	TestEqual(TEXT("First init: Resolution.X = 32"), Gen->GetResolution().X, 32);

	Gen->InitializeHeightmap(64, 128);
	TestEqual(TEXT("Second init: HeightmapData size 64*128"), Gen->GetHeightmapData().Num(), 64 * 128);
	TestEqual(TEXT("Second init: Resolution.X = 64"),  Gen->GetResolution().X, 64);
	TestEqual(TEXT("Second init: Resolution.Y = 128"), Gen->GetResolution().Y, 128);

	Gen->InitializeHeightmap(8, 8);
	TestEqual(TEXT("Third init: HeightmapData size 8*8"), Gen->GetHeightmapData().Num(), 8 * 8);

	return true;
}

// -----------------------------------------------------------------------
// Test: Initialization with invalid size does not crash
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_HeightmapGenerator_InvalidInitSize,
	"Priordium.MapGenerator.HeightmapGenerator.Lifecycle.InvalidInitSize",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_HeightmapGenerator_InvalidInitSize::RunTest(const FString& Parameters)
{
	UHeightmapGenerator* Gen = NewObject<UHeightmapGenerator>();
	if (!TestNotNull(TEXT("Gen is not null"), Gen)) return false;

	Gen->InitializeHeightmap(0, 0);
	TestFalse(TEXT("IsInitialized() false after zero size init"), Gen->IsInitialized());

	Gen->InitializeHeightmap(-1, 100);
	TestFalse(TEXT("IsInitialized() false after negative size init"), Gen->IsInitialized());

	Gen->InitializeHeightmap(4, 4);
	TestTrue(TEXT("IsInitialized() true after valid init"), Gen->IsInitialized());
	TestEqual(TEXT("Size is 4*4 after valid init"), Gen->GetHeightmapData().Num(), 4 * 4);

	return true;
}

// -----------------------------------------------------------------------
// Test: Both square and non-square resolutions work
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_HeightmapGenerator_NonSquareResolution,
	"Priordium.MapGenerator.HeightmapGenerator.Lifecycle.NonSquareResolution",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_HeightmapGenerator_NonSquareResolution::RunTest(const FString& Parameters)
{
	UHeightmapGenerator* Gen = NewObject<UHeightmapGenerator>();
	if (!TestNotNull(TEXT("Gen is not null"), Gen)) return false;

	Gen->InitializeHeightmap(513, 257);
	TestEqual(TEXT("513x257 size correct"), Gen->GetHeightmapData().Num(), 513 * 257);
	TestEqual(TEXT("Resolution.X = 513"), Gen->GetResolution().X, 513);
	TestEqual(TEXT("Resolution.Y = 257"), Gen->GetResolution().Y, 257);

	TestEqual(TEXT("GetHeightAt(512, 256) = 0.5"), Gen->GetHeightAt(512, 256), 0.5f);
	TestEqual(TEXT("GetHeightAt(513, 0) = 0.0"),   Gen->GetHeightAt(513, 0),   0.0f);

	return true;
}

