// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "FlowFieldAgentPawn.generated.h"

class UCapsuleComponent;
class UPawnMovementComponent;
class AActor;

/** A Flow Field Pawn that is assembled from movement and density components. */
UCLASS(Blueprintable)
class OUTBREAK_API AFlowFieldAgentPawn : public AActor
{
	GENERATED_BODY()

public:
	AFlowFieldAgentPawn();
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlowField|Components")
	TObjectPtr<UCapsuleComponent> CapsuleComponent;

protected:
	virtual void BeginPlay() override;
	

};
