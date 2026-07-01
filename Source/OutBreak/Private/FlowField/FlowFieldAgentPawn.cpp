// Copyright Epic Games, Inc. All Rights Reserved.

#include "FlowField/FlowFieldAgentPawn.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "FlowField/Subsystem/FlowFieldSubsystem.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Net/UnrealNetwork.h"



AFlowFieldAgentPawn::AFlowFieldAgentPawn()
{
	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComponent"));
	CapsuleComponent->InitCapsuleSize(34.0f, 72.0f);
	CapsuleComponent->SetCollisionProfileName(TEXT("Pawn"));
	CapsuleComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	CapsuleComponent->SetCanEverAffectNavigation(false);
	RootComponent = CapsuleComponent;
	
}


void AFlowFieldAgentPawn::BeginPlay()
{
	Super::BeginPlay();
	
}
