// Copyright Priordium. All Rights Reserved.

#include "Priordium.h"
#include "Modules/ModuleManager.h"
#include "UMapGenConsoleCommands.h"

class FPriordiumModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();

#if !UE_BUILD_SHIPPING
		// Register console commands only in non-shipping builds.
		UMapGenConsoleCommands::Register();
#endif
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FPriordiumModule, Priordium, "Priordium");

