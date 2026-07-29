// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/System/ModularSkeletalMeshActor.h"

#include "Components/SkeletalMeshComponent.h"


// Sets default values
AModularSkeletalMeshActor::AModularSkeletalMeshActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	Root = CreateDefaultSubobject<USceneComponent>(FName("Root"));
	SetRootComponent(Root);
	LeaderHead = CreateDefaultSubobject<USkeletalMeshComponent>(FName("LeaderHead"));
	LeaderHead->SetupAttachment(Root);
}

// Called when the game starts or when spawned
void AModularSkeletalMeshActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AModularSkeletalMeshActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

