// Fill out your copyright notice in the Description page of Project Settings.


#include "FlowField/Components/HordeComponent.h"

#include "FlowField/HordeProxyHost.h"

UHordeComponent::UHordeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	
}

void UHordeComponent::InitializeHordeComponent(AHordeProxyHost* inOwnerProxyHost)
{
	checkf(
		IsValid(inOwnerProxyHost),
		TEXT("InitializeHordeComponent received an invalid ProxyHost."));
	
	OwnerProxyHost = inOwnerProxyHost;
}


// Called when the game starts
void UHordeComponent::BeginPlay()
{
	Super::BeginPlay();
	
	if (!OwnerProxyHost)
	{
		OwnerProxyHost = Cast<AHordeProxyHost>(GetOwner());
	}

	ensureMsgf(
		IsValid(OwnerProxyHost),
		TEXT("%s must be owned by AHordeProxyHost or initialized explicitly."),
		*GetName());
	
}


// Called every frame
void UHordeComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                             FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

