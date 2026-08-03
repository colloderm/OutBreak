// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/Widget/InventorySlot.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"

#include "Inventory/Components/PlayerInventoryComponent.h"
#include "Inventory/Widget/InventoryDragDropOperation.h"
#include "Item/Data/OBItemDefinition.h"
#include "Item/OBItemRegistry.h"

void UInventorySlot::Update()
{
	// 퀵슬롯은 수량이 0이어도 "등록된 종류"를 계속 보여준다.
	const bool bAssignedQuickSlot =
		SlotHandle.Location == EInventoryItemLocation::QuickSlot &&
		InventoryData.ItemTag.IsValid();
	if ((!bAssignedQuickSlot && InventoryData.ItemStack <= 0) ||
		!InventoryData.ItemTag.IsValid())
	{
		ClearSlot();
		return;
	}

	const FOBItemDefinitionRow* ItemRow = InventoryData.GetDefinition();
	if (!ItemRow)
	{
		// 표에서 행이 사라졌거나 태그 오타. 조용히 비면 원인을 못 찾는다.
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s::%s : DT_Items에 \"%s\" 행이 없다."),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__),
			*InventoryData.ItemTag.ToString());
		ClearSlot();
		return;
	}

	// 아이콘은 소프트 참조라 표가 로드돼도 자동으로 올라오지 않는다.
	SetSlotMetaData(ItemRow->Icon.LoadSynchronous(), InventoryData.ItemStack);
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
