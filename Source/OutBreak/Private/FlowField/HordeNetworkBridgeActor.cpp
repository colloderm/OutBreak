#include "FlowField/HordeNetworkBridgeActor.h"

#include "Engine/World.h"
#include "FlowField/Subsystem/HordeNetworkSubsystem.h"

AHordeNetworkBridgeActor::AHordeNetworkBridgeActor()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	bOnlyRelevantToOwner = true;

	SetReplicateMovement(false);
}

void AHordeNetworkBridgeActor::ClientReceivePayloads_Implementation(
	const TArray<FHordeNetworkFormat>& InPayloads)
{
	if (!HordeNetworkSubsystem)
	{
		if (UWorld* World = GetWorld())
		{
			HordeNetworkSubsystem =
				World->GetSubsystem<UHordeNetworkSubsystem>();
		}
	}

	if (!ensure(HordeNetworkSubsystem))
	{
		return;
	}

	HordeNetworkSubsystem->ReceivePayloads(InPayloads);
}

void AHordeNetworkBridgeActor::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		HordeNetworkSubsystem =
			World->GetSubsystem<UHordeNetworkSubsystem>();
	}

	ensureAlwaysMsgf(
		HordeNetworkSubsystem != nullptr,
		TEXT("HordeNetworkSubsystem could not be found."));
}
