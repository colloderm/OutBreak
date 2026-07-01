// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FlowField/Struct/HordeSystemType.h"
#include "BudgetOverlordSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class OUTBREAK_API UBudgetOverlordSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	
	UFUNCTION(BlueprintCallable)
	void RegisterAgent(
		FTransform inTransform,
		float inMoveSpeed = 300.0);
	
private:
	UPROPERTY(Transient)
	TObjectPtr<class UHordeMovementSubsystem> MovementSubsystem;
	
	UPROPERTY(Transient)
	TObjectPtr<class UHordeProxySubsystem> ProxySubsystem;
	
	void InitializeViceroy(int32 Capacity);
	HordeAgentType AgentStorage;
};
