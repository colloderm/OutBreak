// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FlowField/Subsystem/BaseHordeWorldSubsystem.h"
#include "HordeMovementSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class OUTBREAK_API UHordeMovementSubsystem : public UBaseHordeWorldSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	
	void InitializeStorage(int32 Capacity);
	
protected:
	void Register(FTransform& Transform, float MoveSpeed);
	virtual void ProcessSystem(const float DeltaSeconds) override;
	void Parallel(const float DeltaSeconds);
		
private:
	HordeMovementStorage MovementStorage;
	
	UPROPERTY(Transient)
	TObjectPtr<class UFlowFieldSubsystem> FlowFieldSubsystem;
	
	friend class UBudgetOverlordSubsystem;
	friend class UHordeProxySubsystem;
};
