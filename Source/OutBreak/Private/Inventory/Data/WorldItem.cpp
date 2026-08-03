// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/Data/WorldItem.h"

#include "Net/UnrealNetwork.h"


// Sets default values
AWorldItem::AWorldItem()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void AWorldItem::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWorldItem, ItemData);
	DOREPLIFETIME(AWorldItem, ItemInstance);
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

void AWorldItem::InitializeDroppedItem(
	const FInventoryData& InItemInstance,
	const TArray<FInventoryData>& InContainedInventory)
{
	if (!HasAuthority())
	{
		return;
	}

	ItemInstance = InItemInstance;
	ContainedInventory = InContainedInventory;
	ItemData.ItemDefinition = InItemInstance.ItemDefinition;
	ItemData.ItemName = InItemInstance.ItemName;
	ItemData.ItemStack = InItemInstance.ItemStack;
	ForceNetUpdate();
}

// Called every frame
void AWorldItem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

