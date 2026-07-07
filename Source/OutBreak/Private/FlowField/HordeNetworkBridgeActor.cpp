


#include "FlowField/HordeNetworkBridgeActor.h"

#include "FlowField/Subsystem/HordeNetworkSubsystem.h"


// Sets default values
AHordeNetworkBridgeActor::AHordeNetworkBridgeActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	
	bReplicates = true;
	
	// 이 Birdge의 Owner인 Client에게만 복제
	bOnlyRelevantToOwner = true;
	
	// Bridge는 이동하지 않음
	SetReplicateMovement(false);
	
	UWorld* World = GetWorld();
	
	if (!World)
	{
		return;
	}
	
	HordeNetworkSubsystem = World->GetSubsystem<UHordeNetworkSubsystem>();
	check(HordeNetworkSubsystem)
}

void AHordeNetworkBridgeActor::ClienntReceivePayloads_Implementation(const HordeNetworkFormat& Payload)
{
	check(HordeNetworkSubsystem)
	
	HordeNetworkSubsystem->ReceivePayloads(Payload);
}

// Called when the game starts or when spawned
void AHordeNetworkBridgeActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AHordeNetworkBridgeActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

