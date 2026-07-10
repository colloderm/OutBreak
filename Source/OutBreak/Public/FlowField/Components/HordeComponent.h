// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HordeComponent.generated.h"

class AHordeProxyHost;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class OUTBREAK_API UHordeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHordeComponent();
	
	void InitializeHordeComponent(AHordeProxyHost* inOwnerProxyHost);
	
	
	
	FORCEINLINE AHordeProxyHost* GetOwnerProxyHost()
	{
		if (OwnerProxyHost == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("%s::%s: (%s)Proxy Host is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__), *GetName());
			return nullptr;
		}
		return OwnerProxyHost;
	}

protected:
	// Called when the game starts
	virtual void BeginPlay() override;


public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<AHordeProxyHost> OwnerProxyHost;
};
