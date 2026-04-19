// Copyright Priordium. All Rights Reserved.
#include "PriordiumSpectatorPawn.h"
#include "GameFramework/FloatingPawnMovement.h"

APriordiumSpectatorPawn::APriordiumSpectatorPawn()
{
	// Default PIE spectator speed is 600 cm/s -- multiply by 20
	if (UFloatingPawnMovement* Move = Cast<UFloatingPawnMovement>(GetMovementComponent()))
	{
		Move->MaxSpeed    = 12000.0f;
		Move->Acceleration = 8000.0f;
		Move->Deceleration = 8000.0f;
	}
}
