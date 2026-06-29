// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Struct/FlowFieldAgentStorage.h"
#include "BudgetOverlordSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class OUTBREAK_API UBudgetOverlordSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
	
public:
	
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	
	
private:
	FFlowFieldAgentStorage AgentStorage;
};
