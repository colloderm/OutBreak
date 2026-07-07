// Fill out your copyright notice in the Description page of Project Settings.


#include "FlowField/Subsystem/HordeNetworkSubsystem.h"

#include "FlowField/HordeNetworkBridgeActor.h"
#include "Engine/World.h"
#include "FlowField/Subsystem/BudgetOverlordSubsystem.h"
#include "FlowField/Subsystem/FlowFieldSubsystem.h"
#include "FlowField/Subsystem/HordeStatusSubsystem.h"
#include "GameFramework/PlayerController.h"

void UHordeNetworkSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UHordeNetworkSubsystem::Deinitialize()
{
	Payloads.Reset();

	for (TPair<TObjectPtr<APlayerController>,
		 TObjectPtr<AHordeNetworkBridgeActor>>& Pair : Bridges)
	{
		if (AHordeNetworkBridgeActor* Bridge = Pair.Value.Get();
			IsValid(Bridge))
		{
			Bridge->Destroy();
		}
	}

	Bridges.Empty();
	Super::Deinitialize();
}

void UHordeNetworkSubsystem::BeginSystem()
{
	Super::BeginSystem();
}

void UHordeNetworkSubsystem::SendPayloads()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		Payloads.Reset();
		return;
	}

	if (World->GetNetMode() == NM_Client)
	{
		Payloads.Reset();
		return;
	}

	for (FConstPlayerControllerIterator It =
		     World->GetPlayerControllerIterator();
	     It;
	     ++It)
	{
		APlayerController* PlayerController = It->Get();

		if (IsValid(PlayerController))
		{
			RegisterConnection(PlayerController);
		}
	}

	for (auto It = Bridges.CreateIterator(); It; ++It)
	{
		APlayerController* PlayerController = It.Key().Get();
		AHordeNetworkBridgeActor* Bridge = It.Value().Get();

		if (!IsValid(PlayerController) || !IsValid(Bridge))
		{
			It.RemoveCurrent();
		}
	}

	if (!Payloads.IsEmpty())
	{
		for (const TPair<TObjectPtr<APlayerController>,
			     TObjectPtr<AHordeNetworkBridgeActor>>& Pair : Bridges)
		{
			AHordeNetworkBridgeActor* Bridge = Pair.Value.Get();

			if (IsValid(Bridge))
			{
				Bridge->ClientReceivePayloads(Payloads);
			}
		}
	}

	Payloads.Reset();
}

void UHordeNetworkSubsystem::ReceivePayloads(const TArray<FHordeNetworkFormat>& InPayloads)
{
	UWorld* World =
		GetWorld();

	if (!World)
	{
		return;
	}

	if (World->GetNetMode() != NM_Client)
	{
		if (IsFlowFieldNetworkDiagnosticsEnabled())
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT(
					"[HordeNetwork] World=%s WorldType=%d NetMode=%d "
					"PayloadCount=%d Function=%s IgnoredNonClientReceive=1"),
				*World->GetName(),
				static_cast<int32>(World->WorldType),
				static_cast<int32>(World->GetNetMode()),
				InPayloads.Num(),
				TEXT(__FUNCTION__));
		}

		return;
	}

	if (IsFlowFieldNetworkDiagnosticsEnabled())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"[HordeNetwork] World=%s WorldType=%d NetMode=%d "
				"PayloadCount=%d Function=%s"),
			*World->GetName(),
			static_cast<int32>(World->WorldType),
			static_cast<int32>(World->GetNetMode()),
			InPayloads.Num(),
			TEXT(__FUNCTION__));
	}

	for (const FHordeNetworkFormat& Payload : InPayloads)
	{
		BudgetOverlord->DispatchPayload(Payload);
	}
}

void UHordeNetworkSubsystem::ProcessSystem(const float DeltaSeconds)
{
	Super::ProcessSystem(DeltaSeconds);
	
	SendPayloads();
}

AHordeNetworkBridgeActor* UHordeNetworkSubsystem::RegisterConnection(APlayerController* PlayerController)
{
	UWorld* World = GetWorld();
	
	if (!World || !IsValid(PlayerController))
	{
		return nullptr;
	}
	
	if (World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	
	if (TObjectPtr<AHordeNetworkBridgeActor>* Existing = Bridges.Find(PlayerController))
	{
		if (IsValid(Existing->Get()))
		{
			return Existing->Get();
		}

		Bridges.Remove(PlayerController);
	}
	
	
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = PlayerController;
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
