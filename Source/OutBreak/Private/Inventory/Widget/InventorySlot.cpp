// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/Widget/InventorySlot.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

#include "Inventory/Subsystem/ItemDataSubsystem.h"

void UInventorySlot::Update()
{
	if (InventoryData.ItemName.IsNone() || InventoryData.ItemStack <= 0)
	{
		ClearSlot();
		return;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	if (!IsValid(GameInstance))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s::%s : GameInstance is invalid."),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__));
		return;
	}
	
	const UItemDataSubsystem* ItemDataSubsystem =
		GameInstance->GetSubsystem<UItemDataSubsystem>();

	if (!IsValid(ItemDataSubsystem))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s::%s : ItemDataSubsystem is invalid."),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__));
		return;
	}

	const FItemMetaData* ItemMetaDataRow =
		ItemDataSubsystem->FindItemRow(
			InventoryData.ItemName,
			TEXT("UInventorySlot::Update"));

	if (ItemMetaDataRow == nullptr)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s::%s : Item row \"%s\" was not found."),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__),
			*InventoryData.ItemName.ToString());
		ClearSlot();
		return;
	}
	
	SetSlotMetaData(
		ItemMetaDataRow->ItemTexture,
		InventoryData.ItemStack);
}

void UInventorySlot::SetSlotData(const FInventoryData& Data)
{
	InventoryData = Data;
	
	Update();
}

void UInventorySlot::SetSlotMetaData(UTexture2D* Image, int Stack)
{
	if (!IsValid(ItemImage) || !IsValid(ItemStack))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s::%s : Bound slot widgets are invalid."),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__));
		return;
	}

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

void UInventorySlot::ClearSlot()
{
	if (IsValid(ItemImage))
	{
		ItemImage->SetBrushFromTexture(nullptr);
	}

	if (IsValid(ItemStack))
	{
		ItemStack->SetText(FText::GetEmpty());
	}
}
