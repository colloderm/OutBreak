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
	ProxyRegisterResult Register(FTransform& Transform);
	void Unregister(int32 Index);
	virtual void ProcessSystem(const float DeltaSeconds) override;
	void CreateProxyHost();
	void ParallelProxy();
	HordeProxyStorage ProxyStorage;

	friend class UBudgetOverlordSubsystem;
private:
	UPROPERTY(Transient)
	TObjectPtr<class AHordeProxyHost> HordeProxy;
	
	UPROPERTY(Transient)
	TObjectPtr<class UHordeMovementSubsystem> MovementSubsystem;
	
private:
	UFUNCTION()
	void HandleCapsuleBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleCapsuleEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex);

	UFUNCTION()
	void HandleCapsuleHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse,
		const FHitResult& Hit);
};
