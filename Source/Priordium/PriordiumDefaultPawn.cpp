// Copyright Priordium. All Rights Reserved.
#include "PriordiumDefaultPawn.h"
#include "GameFramework/FloatingPawnMovement.h"

APriordiumDefaultPawn::APriordiumDefaultPawn()
{
	// Default FloatingPawnMovement values: MaxSpeed=1200, Acceleration=4000, Deceleration=8000
	// Multiply by 10 for faster map inspection in PIE.
	if (UFloatingPawnMovement* Move = Cast<UFloatingPawnMovement>(GetMovementComponent()))
	{
		Move->MaxSpeed    = 12000.0f;
		Move->Acceleration = 40000.0f;
		Move->Deceleration = 80000.0f;
	}
}
