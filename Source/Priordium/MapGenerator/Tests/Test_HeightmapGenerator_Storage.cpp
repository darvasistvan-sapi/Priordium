// Copyright Priordium. All Rights Reserved.
//
// Test_HeightmapGenerator_Storage.cpp
// Tests the data storage and query functions of UHeightmapGenerator.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "UHeightmapGenerator.h"

// -----------------------------------------------------------------------
// Test: Component can be instantiated
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_HeightmapGenerator_Instantiable,
	"Priordium.MapGenerator.HeightmapGenerator.Storage.Instantiable",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_HeightmapGenerator_Instantiable::RunTest(const FString& Parameters)
{
	UHeightmapGenerator* Gen = NewObject<UHeightmapGenerator>();
	TestNotNull(TEXT("UHeightmapGenerator can be instantiated"), Gen);
	return true;
}

// -----------------------------------------------------------------------
// Test: IsInitialized() is false before initialization
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_HeightmapGenerator_NotInitializedByDefault,
	"Priordium.MapGenerator.HeightmapGenerator.Storage.NotInitializedByDefault",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_HeightmapGenerator_NotInitializedByDefault::RunTest(const FString& Parameters)
{
	UHeightmapGenerator* Gen = NewObject<UHeightmapGenerator>();
	if (!TestNotNull(TEXT("Gen is not null"), Gen)) return false;

	TestFalse(TEXT("IsInitialized() is false before initialization"), Gen->IsInitialized());
	TestEqual(TEXT("HeightmapData is empty before initialization"), Gen->GetHeightmapData().Num(), 0);
	return true;
}

// -----------------------------------------------------------------------
// Test: Array size is correct after InitializeHeightmap
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_HeightmapGenerator_InitSize,
	"Priordium.MapGenerator.HeightmapGenerator.Storage.InitSize",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_HeightmapGenerator_InitSize::RunTest(const FString& Parameters)
{
	UHeightmapGenerator* Gen = NewObject<UHeightmapGenerator>();
	if (!TestNotNull(TEXT("Gen is not null"), Gen)) return false;

	Gen->InitializeHeightmap(128, 64);

	TestTrue(TEXT("IsInitialized() is true after InitializeHeightmap"), Gen->IsInitialized());
	TestEqual(TEXT("HeightmapData size is 128*64 = 8192"), Gen->GetHeightmapData().Num(), 128 * 64);
	TestEqual(TEXT("Resolution.X = 128"), Gen->GetResolution().X, 128);
	TestEqual(TEXT("Resolution.Y = 64"),  Gen->GetResolution().Y, 64);
	return true;
}

// -----------------------------------------------------------------------
// Test: All values are 0.5 after InitializeHeightmap
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_HeightmapGenerator_DefaultHeight,
	"Priordium.MapGenerator.HeightmapGenerator.Storage.DefaultHeight",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_HeightmapGenerator_DefaultHeight::RunTest(const FString& Parameters)
{
	UHeightmapGenerator* Gen = NewObject<UHeightmapGenerator>();
	if (!TestNotNull(TEXT("Gen is not null"), Gen)) return false;

	Gen->InitializeHeightmap(16, 16);

	const TArray<float>& Data = Gen->GetHeightmapData();
	bool bAllHalf = true;
	for (int32 i = 0; i < Data.Num(); ++i)
	{
		if (!FMath::IsNearlyEqual(Data[i], 0.5f, 1e-6f))
		{
			AddError(FString::Printf(TEXT("Data[%d] = %.6f, not 0.5"), i, Data[i]));
			bAllHalf = false;
			break;
		}
	}
	TestTrue(TEXT("All cell values are 0.5 after initialization"), bAllHalf);
	return true;
}

// -----------------------------------------------------------------------
// Test: GetHeightAt returns correct value for valid coordinates
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_HeightmapGenerator_GetHeightAt_Valid,
	"Priordium.MapGenerator.HeightmapGenerator.Storage.GetHeightAt_Valid",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_HeightmapGenerator_GetHeightAt_Valid::RunTest(const FString& Parameters)
{
	UHeightmapGenerator* Gen = NewObject<UHeightmapGenerator>();
	if (!TestNotNull(TEXT("Gen is not null"), Gen)) return false;

	Gen->InitializeHeightmap(10, 10);

	// Default values
	TestEqual(TEXT("GetHeightAt(0,0) = 0.5"),   Gen->GetHeightAt(0, 0),   0.5f);
	TestEqual(TEXT("GetHeightAt(9,9) = 0.5"),   Gen->GetHeightAt(9, 9),   0.5f);
	TestEqual(TEXT("GetHeightAt(5,3) = 0.5"),   Gen->GetHeightAt(5, 3),   0.5f);
	return true;
}

// -----------------------------------------------------------------------
// Test: GetHeightAt returns 0.0f for invalid coordinates
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_HeightmapGenerator_GetHeightAt_OutOfBounds,
	"Priordium.MapGenerator.HeightmapGenerator.Storage.GetHeightAt_OutOfBounds",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_HeightmapGenerator_GetHeightAt_OutOfBounds::RunTest(const FString& Parameters)
{
	UHeightmapGenerator* Gen = NewObject<UHeightmapGenerator>();
	if (!TestNotNull(TEXT("Gen is not null"), Gen)) return false;

	Gen->InitializeHeightmap(10, 10);

	TestEqual(TEXT("GetHeightAt(-1, 0) = 0.0"),  Gen->GetHeightAt(-1,  0),  0.0f);
	TestEqual(TEXT("GetHeightAt(0, -1) = 0.0"),  Gen->GetHeightAt( 0, -1),  0.0f);
	TestEqual(TEXT("GetHeightAt(10, 0) = 0.0"),  Gen->GetHeightAt(10,  0),  0.0f);
	TestEqual(TEXT("GetHeightAt(0, 10) = 0.0"),  Gen->GetHeightAt( 0, 10),  0.0f);
	TestEqual(TEXT("GetHeightAt(99,99) = 0.0"),  Gen->GetHeightAt(99, 99),  0.0f);
	return true;
}

// -----------------------------------------------------------------------
// Test: GetHeightAt returns 0.0f on uninitialized component
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_HeightmapGenerator_GetHeightAt_Uninitialized,
	"Priordium.MapGenerator.HeightmapGenerator.Storage.GetHeightAt_Uninitialized",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_HeightmapGenerator_GetHeightAt_Uninitialized::RunTest(const FString& Parameters)
{
	UHeightmapGenerator* Gen = NewObject<UHeightmapGenerator>();
	if (!TestNotNull(TEXT("Gen is not null"), Gen)) return false;

	// Without initialization every coordinate is out-of-bounds (Resolution = 0,0)
	TestEqual(TEXT("GetHeightAt(0,0) = 0.0 without initialization"), Gen->GetHeightAt(0, 0), 0.0f);
	return true;
}

// -----------------------------------------------------------------------
// Test: Index calculation is correct (Y * Width + X)
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_HeightmapGenerator_IndexCalculation,
	"Priordium.MapGenerator.HeightmapGenerator.Storage.IndexCalculation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_HeightmapGenerator_IndexCalculation::RunTest(const FString& Parameters)
{
	UHeightmapGenerator* Gen = NewObject<UHeightmapGenerator>();
	if (!TestNotNull(TEXT("Gen is not null"), Gen)) return false;

	const int32 W = 5, H = 5;
	Gen->InitializeHeightmap(W, H);

	// Direct array access: verify index calculation
	// GetHeightAt(X=2, Y=3) -> Index = 3*5 + 2 = 17
	const TArray<float>& Data = Gen->GetHeightmapData();

	// Array is const reference, check indexing implicitly:
	// Both coordinates have the same default value, indexing is implicit.
	TestEqual(TEXT("GetHeightAt(2,3) matches value at same array index"),
		Gen->GetHeightAt(2, 3), Data[3 * W + 2]);

	TestEqual(TEXT("GetHeightAt(0,0) = Data[0]"), Gen->GetHeightAt(0, 0), Data[0]);
	TestEqual(TEXT("GetHeightAt(4,4) = Data[24]"), Gen->GetHeightAt(4, 4), Data[24]);
	return true;
}

