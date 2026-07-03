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
	int32 GetIndexByActor(const AActor* Actor) const;
	
	
	UFUNCTION(BlueprintCallable)
	void RegisterAgent(
		FTransform inTransform,
		float inMoveSpeed = 300.f,
		float MaxHealth = 100.f,
		float HealthPercent = 1.f
	);
	
	void UnregisterAgent(int32 Index);

private:
	/* Status System에서 Damage 적용 병렬 처리를 위한 Index By Actor Cache */
	TMap<TWeakObjectPtr<AActor>, int32> IndexByActor;
	
	UPROPERTY(Transient)
	TObjectPtr<class UHordeMovementSubsystem> MovementSubsystem;
	
	UPROPERTY(Transient)
	TObjectPtr<class UHordeProxySubsystem> ProxySubsystem;
	
	UPROPERTY(Transient)
	TObjectPtr<class UHordeStatusSubsystem> StatusSubsystem;
	
	void InitializeViceroy(int32 Capacity);
};
