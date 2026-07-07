#pragma once

#include "CoreMinimal.h"
#include <GameFramework/Actor.h>

#include "Struct/HordeSystemType.h"
#include "HordeNetworkBridgeActor.generated.h"

UCLASS()
class OUTBREAK_API AHordeNetworkBridgeActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AHordeNetworkBridgeActor();
	
	UFUNCTION(Client, Unreliable)
	void ClienntReceivePayloads(const HordeNetworkFormat& Payload);
	

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
private:
	UPROPERTY(Transient)
	TObjectPtr<UHordeNetworkSubsystem> HordeNetworkSubsystem;
};
