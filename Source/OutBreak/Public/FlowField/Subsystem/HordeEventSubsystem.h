// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FlowField/Subsystem/BaseHordeWorldSubsystem.h"
#include "HordeEventSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class OUTBREAK_API UHordeEventSubsystem : public UBaseHordeWorldSubsystem
{
	GENERATED_BODY()
	
	
	TArray<HordeDamageEvent> HordeDamageEvents;
	
	void AddDamageEvent(AActor* DamagedActor, const double Damage);
	
	
	virtual void ProcessSystem(const float DeltaSeconds) override;
	void ProcessEvent();
	void Parallel();
	
	UFUNCTION()
	void HandleBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);
	
};
