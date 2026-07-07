// Fill out your copyright notice in the Description page of Project Settings.


#include "FlowField/HordeProxyActor.h"

#include "Components/CapsuleComponent.h"


// Sets default values
AHordeProxyActor::AHordeProxyActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneComponent =
		CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

	SetRootComponent(SceneComponent);

	Capsule =
		CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	
	Capsule->SetCapsuleHalfHeight(90.f);
	Capsule->SetCapsuleRadius(34.f);
	Capsule->SetupAttachment(SceneComponent);
	Capsule->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
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

