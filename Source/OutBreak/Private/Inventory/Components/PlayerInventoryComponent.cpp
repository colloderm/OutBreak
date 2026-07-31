// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/Components/PlayerInventoryComponent.h"


// Sets default values for this component's properties
UPlayerInventoryComponent::UPlayerInventoryComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
	
	UpdateInventory();
}

FInventoryQueryResult UPlayerInventoryComponent::QueryHasItem(FName QueryItemName)
{
	FInventoryQueryResult Result = FInventoryQueryResult();
	
	for (auto Element : InventoryBackPackArray)
	{
		if (Element.ItemName.IsEqual(QueryItemName))
		{
			Result.HasItem = true;
			Result.TotalStack += Element.ItemStack;
			Result.Indices.Add(InventoryBackPackArray.Find(Element));
		}
	}
	
	for (auto Element : InventoryContrainerArray)
	{
		if (Element.ItemName.IsEqual(QueryItemName))
		{
			Result.HasItem = true;
			Result.TotalStack += Element.ItemStack;
			Result.Indices.Add(InventoryContrainerArray.Find(Element));
		}
	}
	
	return Result;
}

void UPlayerInventoryComponent::SetInventoryBackPackSize(int newSize)
{
	InventoryBackPackSize = newSize;
	
	UpdateInventory();
}


// Called when the game starts
void UPlayerInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}

void UPlayerInventoryComponent::UpdateInventory()
{
	if (InventoryBackPackArray.Num() != InventoryBackPackSize)
	{
		InventoryBackPackArray.SetNum(InventoryBackPackSize);
	}
	
	
	if (InventoryContrainerArray.Num() != InventoryContainerSize)
	{
		InventoryContrainerArray.SetNum(InventoryContainerSize);
	}
	
	
	if (InventoryQuickSlotsArray.Num() != QuickSlotSize)
	{
		InventoryQuickSlotsArray.SetNum(QuickSlotSize);	
	}
	
	
	
	
}

void UPlayerInventoryComponent::UpdateInventoryWidget()
{
	
	
	// 위젯 데이터 재갱신 처리 요청.
	InventoryWidget;
}

void UPlayerInventoryComponent::LoadInventoryMetaData(void* LowData)
{
	// parshing lowdata 
	
	
	
	// parshed Data Insert;
	InventoryBackPackArray;
	InventoryContrainerArray;
	InventoryQuickSlotsArray;
	
	
}


// Called every frame
void UPlayerInventoryComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

