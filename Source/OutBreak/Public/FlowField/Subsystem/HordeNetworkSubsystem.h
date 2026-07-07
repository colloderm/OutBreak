// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BaseHordeWorldSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "HordeNetworkSubsystem.generated.h"


class AHordeNetworkBridgeActor;
/**
 * 
 */
UCLASS()
class OUTBREAK_API UHordeNetworkSubsystem : public UBaseHordeWorldSubsystem
{
	GENERATED_BODY()
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	
	
	
	
public:
	FORCEINLINE void AddPayload(const HordeNetworkFormat Payload) { Payloads.Add(Payload); }
	
	
	void SendPayloads();
	
	void ReceivePayloads(const HordeNetworkFormat& Payload);
	
protected:
	AHordeNetworkBridgeActor* RegisterConnection(APlayerController* PlayerController);
	virtual void BeginSystem() override;
private:
	TMap<TObjectPtr<APlayerController>,
		 TObjectPtr<AHordeNetworkBridgeActor>> Bridges;
	TArray<HordeNetworkFormat> Payloads;
	
	friend class UBudgetOverlordSubsystem;
};
