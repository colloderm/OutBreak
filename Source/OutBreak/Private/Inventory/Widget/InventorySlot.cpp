// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/Widget/InventorySlot.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"

#include "Inventory/Components/PlayerInventoryComponent.h"
#include "Inventory/Subsystem/ItemDataSubsystem.h"
#include "Inventory/Widget/InventoryDragDropOperation.h"
#include "Item/Data/OBItemDefinition.h"

void UInventorySlot::Update()
{
	const bool bAssignedQuickSlot =
		SlotHandle.Location == EInventoryItemLocation::QuickSlot &&
		InventoryData.ItemDefinition;
	if ((!bAssignedQuickSlot && InventoryData.ItemStack <= 0) ||
		(!InventoryData.ItemDefinition && InventoryData.ItemName.IsNone()))
	{
		ClearSlot();
		return;
	}

	if (InventoryData.ItemDefinition)
	{
		SetSlotMetaData(
			InventoryData.ItemDefinition->Icon,
			InventoryData.ItemStack);
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

void UInventorySlot::SetSlotContext(
	UPlayerInventoryComponent* InInventory,
	const FInventoryItemHandle& InHandle,
	const FInventoryData& Data)
{
	InventoryComponent = InInventory;
	SlotHandle = InHandle;
	SetSlotData(Data);
}

void UInventorySlot::SetQuickSlotContext(
	UPlayerInventoryComponent* InInventory,
	const int32 QuickSlotIndex)
{
	InventoryComponent = InInventory;
	SlotHandle = InInventory
		? InInventory->MakeQuickSlotHandle(QuickSlotIndex)
		: FInventoryItemHandle();

	FInventoryData DisplayData;
	if (!InInventory ||
		!InInventory->GetItemFromHandle(SlotHandle, DisplayData))
	{
		DisplayData = FInventoryData();
	}
	SetSlotData(DisplayData);
}

FReply UInventorySlot::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton &&
		InventoryComponent && SlotHandle.HasItem())
	{
		return UWidgetBlueprintLibrary::DetectDragIfPressed(
			InMouseEvent,
			this,
			EKeys::LeftMouseButton).NativeReply;
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UInventorySlot::NativeOnDragDetected(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent,
	UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);
	if (!InventoryComponent || !SlotHandle.HasItem())
	{
		return;
	}

	UInventoryDragDropOperation* Operation =
		Cast<UInventoryDragDropOperation>(
			UWidgetBlueprintLibrary::CreateDragDropOperation(
				UInventoryDragDropOperation::StaticClass()));
	if (!Operation)
	{
		return;
	}
	Operation->Inventory = InventoryComponent;
	Operation->SourceHandle = SlotHandle;
	OutOperation = Operation;
}

bool UInventorySlot::NativeOnDrop(
	const FGeometry& InGeometry,
	const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	const UInventoryDragDropOperation* Operation =
		Cast<UInventoryDragDropOperation>(InOperation);
	if (!Operation || !Operation->Inventory ||
		Operation->Inventory != InventoryComponent ||
		SlotHandle.Location == EInventoryItemLocation::None)
	{
		return Super::NativeOnDrop(
			InGeometry,
			InDragDropEvent,
			InOperation);
	}

	Operation->Inventory->MoveItem(Operation->SourceHandle, SlotHandle);
	return true;
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
