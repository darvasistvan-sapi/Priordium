// Copyright Priordium. All Rights Reserved.
//
// AHeightmapTestActor.cpp

#include "AHeightmapTestActor.h"
#include "UHeightmapGenerator.h"
#include "UMapGeneratorSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogHeightmapTest, Log, All);

AHeightmapTestActor::AHeightmapTestActor()
{
	PrimaryActorTick.bCanEverTick = false;
	HeightmapComponent = CreateDefaultSubobject<UHeightmapGenerator>(TEXT("HeightmapGenerator"));
}

void AHeightmapTestActor::BeginPlay()
{
	Super::BeginPlay();

	if (TestSettings)
	{
		const bool bSuccess = HeightmapComponent->Generate(TestSettings);

		const int32 Res     = TestSettings->Resolution;
		const int32 CenterX = Res / 2;
		const int32 CenterY = Res / 2;
		const float Height  = HeightmapComponent->GetHeightAt(CenterX, CenterY);

		if (bSuccess)
		{
			UE_LOG(LogHeightmapTest, Display,
				TEXT("Generation Success | Resolution: %dx%d | Seed: %d | Center Height: %.4f"),
				Res, Res, TestSettings->Seed, Height);

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green,
					FString::Printf(TEXT("Generation Success | Height: %.2f (Center %d,%d | Res %dx%d | Seed %d)"),
						Height, CenterX, CenterY, Res, Res, TestSettings->Seed));
			}
		}
		else
		{
			UE_LOG(LogHeightmapTest, Warning, TEXT("Generation FAILED -- check the Settings."));

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("Generation FAILED"));
			}
		}
	}
	else
	{
		HeightmapComponent->InitializeHeightmap(TestResolution, TestResolution);

		const int32 CenterX = TestResolution / 2;
		const int32 CenterY = TestResolution / 2;
		const float Height  = HeightmapComponent->GetHeightAt(CenterX, CenterY);

		UE_LOG(LogHeightmapTest, Display,
			TEXT("MVP2 fallback | Resolution: %dx%d | Center Height: %.2f"),
			TestResolution, TestResolution, Height);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Yellow,
				FString::Printf(TEXT("Height: %.2f  (Center %d,%d | Res %dx%d)"),
					Height, CenterX, CenterY, TestResolution, TestResolution));
		}
	}
}

