// Copyright Priordium. All Rights Reserved.
//
// UClimateZoneManager.cpp
// Climate zone determination and query implementation.

#include "UClimateZoneManager.h"
#include "UMapGeneratorSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogClimateZone, Log, All);

// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------

UClimateZoneManager::UClimateZoneManager()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UClimateZoneManager::BeginPlay()
{
	Super::BeginPlay();
}

EClimateZone UClimateZoneManager::DetermineClimateZone(const FBiomeCell& Cell) const
{
	float Temperature = Cell.Temperature;

	// Altitude correction: colder at high elevation
	if (Cell.Height > 0.7f)
	{
		Temperature -= 0.2f;
	}

	if (Temperature > 0.75f && Cell.Moisture > 0.5f)
	{
		return EClimateZone::Tropical;
	}
	else if (Temperature > 0.4f)
	{
		return EClimateZone::Temperate;
	}
	else if (Temperature > 0.2f)
	{
		return EClimateZone::Continental;
	}
	else
	{
		return EClimateZone::Subarctic;
	}
}

bool UClimateZoneManager::CalculateClimateZones(const TArray<FBiomeCell>& BiomeMap, const UMapGeneratorSettings* Settings)
{
	if (BiomeMap.Num() == 0)
	{
		UE_LOG(LogClimateZone, Warning, TEXT("CalculateClimateZones -- BiomeMap is empty, cannot calculate climate zones."));
		return false;
	}

	if (!Settings)
	{
		UE_LOG(LogClimateZone, Warning, TEXT("CalculateClimateZones -- Settings null."));
		return false;
	}

	Resolution.X = Settings->Resolution;
	Resolution.Y = Settings->Resolution;

	ClimateZoneMap.SetNum(BiomeMap.Num());
	TemperatureMap.SetNum(BiomeMap.Num());

	for (int32 i = 0; i < BiomeMap.Num(); ++i)
	{
		ClimateZoneMap[i]  = DetermineClimateZone(BiomeMap[i]);
		TemperatureMap[i]  = BiomeMap[i].Temperature;
	}

	UE_LOG(LogClimateZone, Display,
		TEXT("CalculateClimateZones -- done. %d cells processed."), ClimateZoneMap.Num());

	return true;
}

float UClimateZoneManager::CalculateSeasonalTemperature(float BaseTemp, float TimeOfYear) const
{
	// Cosine interpolation: TimeOfYear=0 -> winter (t=1), TimeOfYear=0.5 -> summer (t=0)
	const float t = (FMath::Cos(TimeOfYear * 2.0f * PI) + 1.0f) * 0.5f;
	const float Modifier = FMath::Lerp(SeasonProfile.SummerTempModifier, SeasonProfile.WinterTempModifier, t);
	return FMath::Clamp(BaseTemp + Modifier, 0.0f, 1.0f);
}

float UClimateZoneManager::GetTemperatureAt(int32 Index, float TimeOfYear) const
{
	if (!TemperatureMap.IsValidIndex(Index))
	{
		UE_LOG(LogClimateZone, Warning,
			TEXT("GetTemperatureAt -- invalid index: %d (size: %d). Returning -1."),
			Index, TemperatureMap.Num());
		return -1.0f;
	}

	return CalculateSeasonalTemperature(TemperatureMap[Index], TimeOfYear);
}

EClimateZone UClimateZoneManager::GetClimateZoneAt(int32 Index) const
{
	if (!ClimateZoneMap.IsValidIndex(Index))
	{
		UE_LOG(LogClimateZone, Warning,
			TEXT("GetClimateZoneAt -- invalid index: %d (size: %d). Returning Temperate."),
			Index, ClimateZoneMap.Num());
		return EClimateZone::Temperate;
	}

	return ClimateZoneMap[Index];
}
