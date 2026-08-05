// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/Data/WorldItem.h"

#include "Net/UnrealNetwork.h"
#include "Player/Controller/OBPlayerController.h"
#include "Item/OBItemRegistry.h"


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
	DOREPLIFETIME(AWorldItem, ItemInstance);
	DOREPLIFETIME(AWorldItem, ContainedInventory);
}


// Called when the game starts or when spawned
void AWorldItem::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && ItemInstance.ItemTag.IsValid() &&
		ItemInstance.ItemStack > 0 && !ItemInstance.InstanceId.IsValid())
	{
		ItemInstance.InstanceId = FGuid::NewGuid();
		ForceNetUpdate();
	}
}

void AWorldItem::Interact_Implementation(AOBPlayerController* PC)
{
	if (PC && HasItemInstance())
	{
		PC->Server_PickUpWorldItem(this);
	}
}

FText AWorldItem::GetInteractPromptText_Implementation() const
{
	FText ItemName;
	UTexture2D* Icon = nullptr;
	if (UOBItemRegistry::GetItemDisplay(ItemInstance.ItemTag, ItemName, Icon))
	{
		// "붕대 줍기". 바닥에 여러 개 떨어져 있을 때 뭘 줍는지 보여야 한다.
		return FText::Format(
			NSLOCTEXT("OBInventory", "PickUpNamed", "{0} 줍기"), ItemName);
	}
	return NSLOCTEXT("OBInventory", "PickUpWorldItem", "줍기");
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
	ForceNetUpdate();
}

// Called every frame
void AWorldItem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

