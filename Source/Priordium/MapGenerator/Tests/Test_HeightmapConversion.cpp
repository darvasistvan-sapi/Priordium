// Copyright Priordium. All Rights Reserved.
//
// Test_HeightmapConversion.cpp
// Tests the heightmap conversion functionality of ULandscapeBuilder.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "ULandscapeBuilder.h"

// -----------------------------------------------------------------------
// Test: Float 0.0 -> uint16 0
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_HeightmapConversion_ZeroMapsToZero,
	"Priordium.MapGenerator.LandscapeBuilder.Conversion.ZeroMapsToZero",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_HeightmapConversion_ZeroMapsToZero::RunTest(const FString& Parameters)
{
	ULandscapeBuilder* Builder = NewObject<ULandscapeBuilder>();
	if (!TestNotNull(TEXT("Builder is not null"), Builder)) return false;

	TArray<float> Input = { 0.0f };
	TArray<uint16> Result = Builder->ConvertHeightmapToUint16(Input);

	TestEqual(TEXT("Result size is 1"), Result.Num(), 1);
	TestEqual(TEXT("0.0f -> 0"), Result[0], static_cast<uint16>(0));
	return true;
}

// -----------------------------------------------------------------------
// Test: Float 1.0 -> uint16 65535
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_HeightmapConversion_OneMapsToMax,
	"Priordium.MapGenerator.LandscapeBuilder.Conversion.OneMapsToMax",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_HeightmapConversion_OneMapsToMax::RunTest(const FString& Parameters)
{
	ULandscapeBuilder* Builder = NewObject<ULandscapeBuilder>();
	if (!TestNotNull(TEXT("Builder is not null"), Builder)) return false;

	TArray<float> Input = { 1.0f };
	TArray<uint16> Result = Builder->ConvertHeightmapToUint16(Input);

	TestEqual(TEXT("Result size is 1"), Result.Num(), 1);
	TestEqual(TEXT("1.0f -> 65535"), Result[0], static_cast<uint16>(65535));
	return true;
}

// -----------------------------------------------------------------------
// Test: Float 0.5 -> uint16 ~32767
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_HeightmapConversion_HalfMapsToMid,
	"Priordium.MapGenerator.LandscapeBuilder.Conversion.HalfMapsToMid",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_HeightmapConversion_HalfMapsToMid::RunTest(const FString& Parameters)
{
	ULandscapeBuilder* Builder = NewObject<ULandscapeBuilder>();
	if (!TestNotNull(TEXT("Builder is not null"), Builder)) return false;

	TArray<float> Input = { 0.5f };
	TArray<uint16> Result = Builder->ConvertHeightmapToUint16(Input);

	TestEqual(TEXT("Result size is 1"), Result.Num(), 1);
	// 0.5 * 65535 = 32767.5 -> 32767 (truncation)
	TestEqual(TEXT("0.5f -> 32767"), Result[0], static_cast<uint16>(32767));
	return true;
}

// -----------------------------------------------------------------------
// Test: Clamping -- values below 0 are clamped to 0
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_HeightmapConversion_ClampBelowZero,
	"Priordium.MapGenerator.LandscapeBuilder.Conversion.ClampBelowZero",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_HeightmapConversion_ClampBelowZero::RunTest(const FString& Parameters)
{
	ULandscapeBuilder* Builder = NewObject<ULandscapeBuilder>();
	if (!TestNotNull(TEXT("Builder is not null"), Builder)) return false;

	TArray<float> Input = { -0.5f, -1.0f, -100.0f };
	TArray<uint16> Result = Builder->ConvertHeightmapToUint16(Input);

	TestEqual(TEXT("Result size is 3"), Result.Num(), 3);
	TestEqual(TEXT("-0.5f -> 0 (clamp)"),   Result[0], static_cast<uint16>(0));
	TestEqual(TEXT("-1.0f -> 0 (clamp)"),   Result[1], static_cast<uint16>(0));
	TestEqual(TEXT("-100.0f -> 0 (clamp)"), Result[2], static_cast<uint16>(0));
	return true;
}

// -----------------------------------------------------------------------
// Test: Clamping -- values above 1 are clamped to 65535
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_HeightmapConversion_ClampAboveOne,
	"Priordium.MapGenerator.LandscapeBuilder.Conversion.ClampAboveOne",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_HeightmapConversion_ClampAboveOne::RunTest(const FString& Parameters)
{
	ULandscapeBuilder* Builder = NewObject<ULandscapeBuilder>();
	if (!TestNotNull(TEXT("Builder is not null"), Builder)) return false;

	TArray<float> Input = { 1.5f, 2.0f, 100.0f };
	TArray<uint16> Result = Builder->ConvertHeightmapToUint16(Input);

	TestEqual(TEXT("Result size is 3"), Result.Num(), 3);
	TestEqual(TEXT("1.5f -> 65535 (clamp)"),   Result[0], static_cast<uint16>(65535));
	TestEqual(TEXT("2.0f -> 65535 (clamp)"),   Result[1], static_cast<uint16>(65535));
	TestEqual(TEXT("100.0f -> 65535 (clamp)"), Result[2], static_cast<uint16>(65535));
	return true;
}

// -----------------------------------------------------------------------
// Test: Empty array handling
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_HeightmapConversion_EmptyInput,
	"Priordium.MapGenerator.LandscapeBuilder.Conversion.EmptyInput",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_HeightmapConversion_EmptyInput::RunTest(const FString& Parameters)
{
	ULandscapeBuilder* Builder = NewObject<ULandscapeBuilder>();
	if (!TestNotNull(TEXT("Builder is not null"), Builder)) return false;

	TArray<float> Input;
	TArray<uint16> Result = Builder->ConvertHeightmapToUint16(Input);

	TestEqual(TEXT("Empty input -> empty output"), Result.Num(), 0);
	return true;
}

// -----------------------------------------------------------------------
// Test: Linear mapping -- order of monotonically increasing values is preserved
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_HeightmapConversion_MonotonicMapping,
	"Priordium.MapGenerator.LandscapeBuilder.Conversion.MonotonicMapping",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_HeightmapConversion_MonotonicMapping::RunTest(const FString& Parameters)
{
	ULandscapeBuilder* Builder = NewObject<ULandscapeBuilder>();
	if (!TestNotNull(TEXT("Builder is not null"), Builder)) return false;

	TArray<float> Input = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
	TArray<uint16> Result = Builder->ConvertHeightmapToUint16(Input);

	TestEqual(TEXT("Result size is 5"), Result.Num(), 5);

	// Monotonically increasing order is preserved
	for (int32 i = 1; i < Result.Num(); ++i)
	{
		TestTrue(FString::Printf(TEXT("Result[%d] >= Result[%d]"), i, i - 1),
			Result[i] >= Result[i - 1]);
	}

	return true;
}

