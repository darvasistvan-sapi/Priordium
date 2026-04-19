// Copyright Priordium. All Rights Reserved.
//
// UMapGenConsoleCommands.h
// Developer console commands for testing the MapGenerator module.
// SOLID: Single Responsibility -- exclusively handles debug console commands.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UMapGenConsoleCommands.generated.h"

/**
 * UMapGenConsoleCommands
 * Debug commands registered in the Unreal Engine console command system.
 * The single static instance of this class is created when the module is loaded.
 *
 * Available commands:
 *   MapGen.TestNoise [X] [Y] [Seed]  -- Writes a noise value to the Output Log
 */
UCLASS()
class PRIORDIUM_API UMapGenConsoleCommands : public UObject
{
	GENERATED_BODY()

public:

	/** Registers all MapGen.* console commands. Should be called on module load. */
	static void Register();
};


