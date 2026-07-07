// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FlowField/Subsystem/BaseHordeWorldSubsystem.h"
#include "HordeStatusSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class OUTBREAK_API UHordeStatusSubsystem : public UBaseHordeWorldSubsystem
{
	GENERATED_BODY()
	
	
	TArray<HordeDamageEvent> HordeDamageEvents;

	// 액터가 HordeDamageEvents의 몇 번째 원소인지 기록
	TMap<TWeakObjectPtr<AActor>, int32> DamageEventIndexMap;
	
public:
	void AddDamageEvent(AActor* DamagedActor, const double Damage);
	
protected:
	void InitializeStorage(int32 Capacity);
	/* Percent는 현재 MaxHealth 기반 현재 체력*/
	int32 Register(float MaxHealth, float Percent = 1.0);
	void Unregister(int32 Index);
	
	virtual void ProcessSystem(const float DeltaSeconds) override;
	void DeadCheck();
	void Parallel();
	
	friend class UBudgetOverlordSubsystem;
	
	HordeStatusStorage StatusStorage;
	
	UFUNCTION()
	void HandleBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);
	
};
