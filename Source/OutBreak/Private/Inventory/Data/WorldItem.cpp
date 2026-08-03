// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/Data/WorldItem.h"


// Sets default values
AWorldItem::AWorldItem()
{
	PrimaryActorTick.bCanEverTick = false;
}


// Called when the game starts or when spawned
void AWorldItem::BeginPlay()
{
	Super::BeginPlay();
	
}

void AWorldItem::PickUpCompleted()
{
	Destroy();
}

// Called every frame
void AWorldItem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

