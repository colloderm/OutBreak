// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FlowField/Subsystem/BaseHordeWorldSubsystem.h"
#include "HordeProxySubsystem.generated.h"


/**
 * 
 */
UCLASS()
class OUTBREAK_API UHordeProxySubsystem : public UBaseHordeWorldSubsystem
{
	GENERATED_BODY()
	
	
	
public:
	virtual auto Initialize(FSubsystemCollectionBase& Collection) -> void override;


	void InitializeStorage(int32 Capacity);
	
protected:
	void Register(FTransform& Transform);
	virtual void ProcessSystem(const float DeltaSeconds) override;
	void CreateProxyHost();
	void ParallelProxy();
	HordeProxyStorage ProxyEntity;
	
	friend class UBudgetOverlordSubsystem;
private:
	UPROPERTY(Transient)
	TObjectPtr<class AHordeProxyActor> HordeProxy;
	
	UPROPERTY(Transient)
	TObjectPtr<class UHordeMovementSubsystem> MovementSubsystem;
};
