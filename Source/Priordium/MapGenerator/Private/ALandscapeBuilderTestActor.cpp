// Copyright Priordium. All Rights Reserved.
//
// ALandscapeBuilderTestActor.cpp

#include "ALandscapeBuilderTestActor.h"
#include "ULandscapeBuilder.h"
#include "UHeightmapGenerator.h"
#include "UMapGeneratorSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogLandscapeBuilderTest, Log, All);

ALandscapeBuilderTestActor::ALandscapeBuilderTestActor()
{
	PrimaryActorTick.bCanEverTick = false;
	LandscapeBuilderComponent = CreateDefaultSubobject<ULandscapeBuilder>(TEXT("LandscapeBuilder"));
}

void ALandscapeBuilderTestActor::BeginPlay()
{
	Super::BeginPlay();

	if (!LandscapeBuilderComponent) return;

	const bool bLoaded = LandscapeBuilderComponent->LoadLayerInfoAssets();
	UE_LOG(LogLandscapeBuilderTest, Display,
		TEXT("LoadLandscapeBuilder -- Layer Info loaded: %s (%d / 7 biomes)"),
		bLoaded ? TEXT("SUCCESS") : TEXT("INCOMPLETE"),
		LandscapeBuilderComponent->LayerInfoAssets.Num());

	UMapGeneratorSettings* Settings = NewObject<UMapGeneratorSettings>();
	Settings->Resolution      = 65;
	Settings->QuadSize        = 100;
	Settings->Seed            = 42;
	Settings->bUseContinentMask          = true;
	Settings->HeightmapConfig.Octaves    = 4;
	Settings->HeightmapConfig.Frequency  = 0.05f;
	Settings->HeightmapConfig.Amplitude  = 1.0f;
	Settings->HeightmapConfig.Persistence = 0.5f;
	Settings->HeightmapConfig.Lacunarity = 2.0f;

	UHeightmapGenerator* HeightmapGen = NewObject<UHeightmapGenerator>(this);
	HeightmapGen->Generate(Settings);

	const bool bBuilt = LandscapeBuilderComponent->BuildLandscape(Settings, HeightmapGen);

	UE_LOG(LogLandscapeBuilderTest, Display,
		TEXT("BuildLandscape -- result: %s"),
		bBuilt ? TEXT("SUCCESS -- Landscape created!") : TEXT("FAILED"));

	if (GEngine)
		GEngine->AddOnScreenDebugMessage(-1, 15.0f,
			bBuilt ? FColor::Green : FColor::Red,
			FString::Printf(TEXT("LandscapeBuilder MVP4 | Build: %s | LayerInfo: %d/7"),
				bBuilt ? TEXT("OK") : TEXT("FAIL"),
				LandscapeBuilderComponent->LayerInfoAssets.Num()));
}

