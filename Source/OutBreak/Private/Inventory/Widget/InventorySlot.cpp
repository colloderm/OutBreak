// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/Widget/InventorySlot.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

#include "Inventory/Subsystem/ItemDataSubsystem.h"

void UInventorySlot::Update()
{
	const UGameInstance* GameInstance = GetGameInstance();
	if (!IsValid(GameInstance))
	{
		return;
	}
	
	const UItemDataSubsystem* ItemDataSubsystem =
	GameInstance->GetSubsystem<UItemDataSubsystem>();
	
	if (InventoryData->ItemName.IsNone())
	{
		UE_LOG(LogTemp, Fatal, TEXT("%s::%s : Slot Fatal Error. Maybe, Inventory Data is None"), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	auto ItemMetaDataRow = ItemDataSubsystem->FindItemRow(InventoryData->ItemName);
	
	UTexture2D* RefTexture2D = ItemMetaDataRow->ItemTexture;
	
	SetSlotMetaData(RefTexture2D, InventoryData->ItemStack);
}

void UInventorySlot::SetSlotData(FInventoryData* Data)
{
	InventoryData = Data;
	
	Update();
}

void UInventorySlot::SetSlotMetaData(UTexture2D* Image, int Stack)
{
	ItemImage->SetBrushFromTexture(Image);
	

	if (Stack > 1)
	{
		ItemStack->SetText(FText::AsNumber(Stack));
	}
	else
	{
		ItemStack->SetText(FText::GetEmpty());
	}
}
