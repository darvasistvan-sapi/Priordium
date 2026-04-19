// Copyright Priordium. All Rights Reserved.
//
// Test_LandscapePlugin.cpp
// Tests that the Landscape plugin is accessible and ALandscapeProxy can be instantiated.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "LandscapeProxy.h"
#include "LandscapeLayerInfoObject.h"

// -----------------------------------------------------------------------
// Test: Landscape module is loaded
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_LandscapePlugin_ModuleLoaded,
	"Priordium.MapGenerator.LandscapeBuilder.Plugin.ModuleLoaded",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_LandscapePlugin_ModuleLoaded::RunTest(const FString& Parameters)
{
	const bool bLoaded = FModuleManager::Get().IsModuleLoaded("Landscape");
	TestTrue(TEXT("Landscape module is loaded"), bLoaded);
	return true;
}

// -----------------------------------------------------------------------
// Test: ALandscapeProxy UClass is accessible via reflection
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_LandscapePlugin_ProxyClassExists,
	"Priordium.MapGenerator.LandscapeBuilder.Plugin.ProxyClassExists",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_LandscapePlugin_ProxyClassExists::RunTest(const FString& Parameters)
{
	const UClass* ProxyClass = ALandscapeProxy::StaticClass();
	TestNotNull(TEXT("ALandscapeProxy::StaticClass() is not null"), ProxyClass);
	TestTrue(TEXT("ALandscapeProxy is a descendant of AActor"),
		ProxyClass->IsChildOf(AActor::StaticClass()));
	return true;
}

// -----------------------------------------------------------------------
// Test: ULandscapeLayerInfoObject UClass is accessible
// -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTest_LandscapePlugin_LayerInfoClassExists,
	"Priordium.MapGenerator.LandscapeBuilder.Plugin.LayerInfoClassExists",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTest_LandscapePlugin_LayerInfoClassExists::RunTest(const FString& Parameters)
{
	const UClass* LayerInfoClass = ULandscapeLayerInfoObject::StaticClass();
	TestNotNull(TEXT("ULandscapeLayerInfoObject::StaticClass() is not null"), LayerInfoClass);
	return true;
}

