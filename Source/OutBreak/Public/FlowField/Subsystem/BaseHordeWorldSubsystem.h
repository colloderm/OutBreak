// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FlowField/Struct//HordeSystemType.h"
#include "BaseHordeWorldSubsystem.generated.h"


/**
 * 
 */
UCLASS()
class OUTBREAK_API UBaseHordeWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	
	
protected:
	virtual void ProcessSystem(const float DeltaSeconds) {};
	UPROPERTY()
	TObjectPtr<class UFlowFieldSettings> Settings;
};
