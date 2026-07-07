// Fill out your copyright notice in the Description page of Project Settings.


#include "FlowField/Subsystem/HordeNetworkSubsystem.h"

#include "FlowField/HordeNetworkBridgeActor.h"

void UHordeNetworkSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	
}

void UHordeNetworkSubsystem::BeginSystem()
{
	Super::BeginSystem();
	
	
	
}

void UHordeNetworkSubsystem::SendPayloads()
{
	
}

void UHordeNetworkSubsystem::ReceivePayloads(const HordeNetworkFormat& Payload)
{
	
}

AHordeNetworkBridgeActor* UHordeNetworkSubsystem::RegisterConnection(APlayerController* PlayerController)
{
	UWorld* World = GetWorld();
	
	if (!World || !PlayerController)
	{
		return nullptr;
	}
	
	if (!PlayerController->HasAuthority())
	{
		return nullptr;
	}
	
	if (TObjectPtr<AHordeNetworkBridgeActor>* Existing = Bridges.Find(PlayerController))
	{
		return *Existing;
	}
	
	
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	

	AHordeNetworkBridgeActor* Bridge = World->SpawnActor<AHordeNetworkBridgeActor>
	(
		AHordeNetworkBridgeActor::StaticClass(),
		FTransform::Identity,
		SpawnParameters
	);
	
	if (!Bridge)
	{
		return nullptr;
	}
	
	Bridges.Add(PlayerController, Bridge);
	
	return Bridge;
}
