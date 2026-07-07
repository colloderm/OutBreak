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
	virtual void Deinitialize() override;
		
public:
	
	
	FORCEINLINE void AddPayload(const FHordeNetworkFormat& Payload)
	{
		Payloads.Add(Payload);
	}
	
	void SendPayloads();
	
	void ReceivePayloads(const TArray<FHordeNetworkFormat>& InPayloads);
	
protected:
	virtual void ProcessSystem(const float DeltaSeconds) override;
	AHordeNetworkBridgeActor* RegisterConnection(APlayerController* PlayerController);
	virtual void BeginSystem() override;
	
	
private:
	TMap<TObjectPtr<APlayerController>,
		 TObjectPtr<AHordeNetworkBridgeActor>> Bridges;
	TArray<FHordeNetworkFormat> Payloads;
	
	friend class UBudgetOverlordSubsystem;
};
