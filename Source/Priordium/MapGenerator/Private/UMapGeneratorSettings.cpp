// Copyright Priordium. All Rights Reserved.
//
// UMapGeneratorSettings.cpp
// Validation helpers for map generation settings.

#include "UMapGeneratorSettings.h"

bool UMapGeneratorSettings::IsValidForGeneration() const
{
	return Validate(nullptr);
}

bool UMapGeneratorSettings::Validate(FString* OutError) const
{
	auto SetError = [OutError](const TCHAR* Msg)
	{
		if (OutError)
		{
			*OutError = Msg;
		}
	};

	if (MapSizeX <= 0 || MapSizeY <= 0)
	{
		SetError(TEXT("MapSizeX and MapSizeY must be > 0."));
		return false;
	}

	if (Resolution < 33)
	{
		SetError(TEXT("Resolution must be >= 33."));
		return false;
	}

	if (QuadSize <= 0)
	{
		SetError(TEXT("QuadSize must be > 0."));
		return false;
	}

	if (SeaLevel < 0.0f || SeaLevel > 1.0f)
	{
		SetError(TEXT("SeaLevel must be in [0.0, 1.0]."));
		return false;
	}

	return true;
}
