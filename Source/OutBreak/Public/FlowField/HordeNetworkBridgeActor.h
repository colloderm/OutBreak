#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Struct/HordeSystemType.h"
#include "HordeNetworkBridgeActor.generated.h"

class UHordeNetworkSubsystem;

UCLASS()
class OUTBREAK_API AHordeNetworkBridgeActor : public AActor
{
	GENERATED_BODY()

public:
	AHordeNetworkBridgeActor();
	
	UFUNCTION(Client, Reliable)
	void ClientReceiveLifecyclePayloads(
		const TArray<FHordeNetworkFormat>& InPayloads);

	UFUNCTION(Client, Unreliable)
	void ClientReceiveSnapshotPayloads(
		const TArray<FHordeNetworkFormat>& InPayloads);
	

protected:
	virtual void BeginPlay() override;
	
private:
	UPROPERTY(Transient)
	TObjectPtr<UHordeNetworkSubsystem> HordeNetworkSubsystem;
};
