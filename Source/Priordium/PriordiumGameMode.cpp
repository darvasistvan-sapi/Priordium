// Copyright Priordium. All Rights Reserved.
#include "PriordiumGameMode.h"
#include "PriordiumDefaultPawn.h"
#include "PriordiumSpectatorPawn.h"

APriordiumGameMode::APriordiumGameMode()
{
	DefaultPawnClass = APriordiumDefaultPawn::StaticClass();
	SpectatorClass   = APriordiumSpectatorPawn::StaticClass();
}
