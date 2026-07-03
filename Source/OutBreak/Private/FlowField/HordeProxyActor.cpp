// Fill out your copyright notice in the Description page of Project Settings.


#include "FlowField/HordeProxyActor.h"

#include "Components/CapsuleComponent.h"


// Sets default values
AHordeProxyActor::AHordeProxyActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	Capsule = CreateDefaultSubobject<UCapsuleComponent>("Capsule");
}

// Called when the game starts or when spawned
void AHordeProxyActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AHordeProxyActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

