// Copyright Priordium. All Rights Reserved.
//
// Test_BiomeManager_Structure.cpp
// Tests the basic structure and getter interface of UBiomeManager.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "UBiomeManager.h"

// -----------------------------------------------------------------------
// Test: UBiomeManager can be instantiated
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_BiomeManager_Instantiable,
	"Priordium.MapGenerator.BiomeManager.Structure.Instantiable",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_BiomeManager_Instantiable::RunTest(const FString& Parameters)
{
	UBiomeManager* Manager = NewObject<UBiomeManager>();
	TestNotNull(TEXT("UBiomeManager NewObject succeeded"), Manager);
	TestTrue(TEXT("UBiomeManager is a UActorComponent descendant"),
		Manager->IsA(UActorComponent::StaticClass()));
	return true;
}

// -----------------------------------------------------------------------
// Test: IsInitialized() returns false before initialization
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_BiomeManager_NotInitializedByDefault,
	"Priordium.MapGenerator.BiomeManager.Structure.NotInitializedByDefault",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_BiomeManager_NotInitializedByDefault::RunTest(const FString& Parameters)
{
	UBiomeManager* Manager = NewObject<UBiomeManager>();
	if (!TestNotNull(TEXT("Manager is not null"), Manager)) return false;

	TestFalse(TEXT("IsInitialized() is false before initialization"), Manager->IsInitialized());
	TestEqual(TEXT("BiomeMap is empty"), Manager->GetBiomeMap().Num(), 0);
	TestEqual(TEXT("Resolution (0,0)"), Manager->GetResolution(), FIntPoint(0, 0));
	return true;
}

// -----------------------------------------------------------------------
// Test: Correct size and default values after InitializeBiomeMap
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_BiomeManager_InitSize,
	"Priordium.MapGenerator.BiomeManager.Structure.InitSize",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_BiomeManager_InitSize::RunTest(const FString& Parameters)
{
	UBiomeManager* Manager = NewObject<UBiomeManager>();
	if (!TestNotNull(TEXT("Manager is not null"), Manager)) return false;

	Manager->InitializeBiomeMap(32, 16);

	TestTrue(TEXT("IsInitialized() is true"), Manager->IsInitialized());
	TestEqual(TEXT("BiomeMap size is 32*16"), Manager->GetBiomeMap().Num(), 32 * 16);
	TestEqual(TEXT("Resolution.X = 32"), Manager->GetResolution().X, 32);
	TestEqual(TEXT("Resolution.Y = 16"), Manager->GetResolution().Y, 16);
	return true;
}

// -----------------------------------------------------------------------
// Test: Default cells are initialized with Plains biome type
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_BiomeManager_DefaultBiomePlains,
	"Priordium.MapGenerator.BiomeManager.Structure.DefaultBiomePlains",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_BiomeManager_DefaultBiomePlains::RunTest(const FString& Parameters)
{
	UBiomeManager* Manager = NewObject<UBiomeManager>();
	if (!TestNotNull(TEXT("Manager is not null"), Manager)) return false;

	Manager->InitializeBiomeMap(8, 8);

	// Every cell should default to Plains
	bool bAllPlains = true;
	for (int32 Y = 0; Y < 8; ++Y)
	{
		for (int32 X = 0; X < 8; ++X)
		{
			if (Manager->GetBiomeTypeAt(X, Y) != EBiomeType::Plains)
			{
				bAllPlains = false;
				AddError(FString::Printf(TEXT("GetBiomeTypeAt(%d,%d) is not Plains"), X, Y));
			}
		}
	}
	TestTrue(TEXT("All cells default to Plains"), bAllPlains);
	return true;
}

// -----------------------------------------------------------------------
// Test: GetBiomeAt returns correct FBiomeCell for valid coordinates
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_BiomeManager_GetBiomeAt_Valid,
	"Priordium.MapGenerator.BiomeManager.Structure.GetBiomeAt_Valid",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_BiomeManager_GetBiomeAt_Valid::RunTest(const FString& Parameters)
{
	UBiomeManager* Manager = NewObject<UBiomeManager>();
	if (!TestNotNull(TEXT("Manager is not null"), Manager)) return false;

	Manager->InitializeBiomeMap(10, 10);

	FBiomeCell Cell = Manager->GetBiomeAt(5, 3);
	TestEqual(TEXT("GetBiomeAt(5,3) BiomeType = Plains"), Cell.BiomeType, EBiomeType::Plains);
	TestEqual(TEXT("GetBiomeAt(5,3) Height = 0.5"),       Cell.Height,    0.5f);
	return true;
}

// -----------------------------------------------------------------------
// Test: GetBiomeAt and GetBiomeTypeAt return default value for invalid coordinates
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_BiomeManager_GetBiomeAt_OutOfBounds,
	"Priordium.MapGenerator.BiomeManager.Structure.GetBiomeAt_OutOfBounds",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_BiomeManager_GetBiomeAt_OutOfBounds::RunTest(const FString& Parameters)
{
	UBiomeManager* Manager = NewObject<UBiomeManager>();
	if (!TestNotNull(TEXT("Manager is not null"), Manager)) return false;

	Manager->InitializeBiomeMap(10, 10);

	// GetBiomeTypeAt returns Plains for invalid coordinates
	TestEqual(TEXT("GetBiomeTypeAt(-1,0) = Plains"),  Manager->GetBiomeTypeAt(-1,  0), EBiomeType::Plains);
	TestEqual(TEXT("GetBiomeTypeAt(0,-1) = Plains"),  Manager->GetBiomeTypeAt( 0, -1), EBiomeType::Plains);
	TestEqual(TEXT("GetBiomeTypeAt(10,0) = Plains"),  Manager->GetBiomeTypeAt(10,  0), EBiomeType::Plains);
	TestEqual(TEXT("GetBiomeTypeAt(0,10) = Plains"),  Manager->GetBiomeTypeAt( 0, 10), EBiomeType::Plains);

	// GetBiomeAt returns default FBiomeCell for invalid coordinates
	FBiomeCell OutCell = Manager->GetBiomeAt(99, 99);
	TestEqual(TEXT("GetBiomeAt(99,99) = default Plains"), OutCell.BiomeType, EBiomeType::Plains);
	TestEqual(TEXT("GetBiomeAt(99,99) Height = 0.5"),     OutCell.Height,    0.5f);
	return true;
}

// -----------------------------------------------------------------------
// Test: Query without initialization does not crash
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_BiomeManager_GetBiomeAt_Uninitialized,
	"Priordium.MapGenerator.BiomeManager.Structure.GetBiomeAt_Uninitialized",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_BiomeManager_GetBiomeAt_Uninitialized::RunTest(const FString& Parameters)
{
	UBiomeManager* Manager = NewObject<UBiomeManager>();
	if (!TestNotNull(TEXT("Manager is not null"), Manager)) return false;

	// Without initialization
	FBiomeCell Cell    = Manager->GetBiomeAt(0, 0);
	EBiomeType BType   = Manager->GetBiomeTypeAt(0, 0);

	TestEqual(TEXT("Uninitialized GetBiomeAt BiomeType = Plains"), Cell.BiomeType, EBiomeType::Plains);
	TestEqual(TEXT("Uninitialized GetBiomeTypeAt = Plains"),       BType,          EBiomeType::Plains);
	return true;
}

// -----------------------------------------------------------------------
// Test: Multiple initializations do not crash, size is updated
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_BiomeManager_MultipleInit,
	"Priordium.MapGenerator.BiomeManager.Structure.MultipleInit",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_BiomeManager_MultipleInit::RunTest(const FString& Parameters)
{
	UBiomeManager* Manager = NewObject<UBiomeManager>();
	if (!TestNotNull(TEXT("Manager is not null"), Manager)) return false;

	Manager->InitializeBiomeMap(16, 16);
	TestEqual(TEXT("First init: 16*16"), Manager->GetBiomeMap().Num(), 256);

	Manager->InitializeBiomeMap(32, 8);
	TestEqual(TEXT("Second init: 32*8"), Manager->GetBiomeMap().Num(), 256);
	TestEqual(TEXT("Resolution.X = 32"), Manager->GetResolution().X, 32);
	TestEqual(TEXT("Resolution.Y = 8"),  Manager->GetResolution().Y, 8);

	return true;
}

// -----------------------------------------------------------------------
// Test: Invalid size initialization does not crash
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_BiomeManager_InvalidInitSize,
	"Priordium.MapGenerator.BiomeManager.Structure.InvalidInitSize",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_BiomeManager_InvalidInitSize::RunTest(const FString& Parameters)
{
	UBiomeManager* Manager = NewObject<UBiomeManager>();
	if (!TestNotNull(TEXT("Manager is not null"), Manager)) return false;

	Manager->InitializeBiomeMap(0, 0);
	TestFalse(TEXT("Zero size: IsInitialized() false"), Manager->IsInitialized());

	Manager->InitializeBiomeMap(-1, 10);
	TestFalse(TEXT("Negative size: IsInitialized() false"), Manager->IsInitialized());

	Manager->InitializeBiomeMap(4, 4);
	TestTrue(TEXT("IsInitialized() true after valid init"), Manager->IsInitialized());

	return true;
}

