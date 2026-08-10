// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/Widget/InventorySlot.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"

#include "Inventory/Components/PlayerInventoryComponent.h"
#include "Inventory/Widget/InventoryDragDropOperation.h"
#include "Item/OBItemRegistry.h"
#include "UI/Tooltip/OBItemTooltipLibrary.h"

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

	// 이름/아이콘의 단일 조회 경로를 사용한다. 무기 행의 Icon이 비어 있으면
	// GetItemDisplay가 WeaponData의 WeaponIcon으로 fallback한다.
	FText DisplayName;
	UTexture2D* Icon = nullptr;
	if (!UOBItemRegistry::GetItemDisplay(
		InventoryData.ItemTag,
		DisplayName,
		Icon))
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

	SetSlotMetaData(Icon, InventoryData.ItemStack);
	SetToolTipText(UOBItemTooltipLibrary::BuildFallbackTooltipText(InventoryData));
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

void UInventorySlot::SetAttachmentContext(
	UPlayerInventoryComponent* InInventory,
	const FGuid& InWeaponInstanceId,
	const FGameplayTag& InAttachmentSlotTag,
	const FInventoryData& Data)
{
	InventoryComponent = InInventory;
	AttachmentWeaponInstanceId = InWeaponInstanceId;
	AttachmentSlotTag = InAttachmentSlotTag;

	// SlotHandle은 비워 둔다. Location이 None이라 드롭이 와도 MoveItem으로
	// 새지 않고 Super로 흘러간다.
	SlotHandle = FInventoryItemHandle();
	SetSlotData(Data);
}

FReply UInventorySlot::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	// 부착물 칸 우클릭 = 해제. 서버가 가방으로 되돌려준다.
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton &&
		InventoryComponent &&
		AttachmentSlotTag.IsValid() &&
		AttachmentWeaponInstanceId.IsValid() &&
		InventoryData.ItemTag.IsValid())
	{
		InventoryComponent->RemoveAttachment(
			AttachmentWeaponInstanceId,
			AttachmentSlotTag);
		return FReply::Handled();
	}

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
	ItemImage->SetVisibility(
		Image
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Hidden);

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
	SetToolTipText(FText::GetEmpty());
	if (IsValid(ItemImage))
	{
		ItemImage->SetBrushFromTexture(nullptr);
		ItemImage->SetVisibility(ESlateVisibility::Hidden);
	}

	if (IsValid(ItemStack))
	{
		ItemStack->SetText(FText::GetEmpty());
	}
}
