// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/Widget/InventoryWindow.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Inventory/Components/PlayerInventoryComponent.h"
#include "Inventory/Widget/InventoryDragDropOperation.h"
#include "Inventory/Widget/InventorySlot.h"
#include "Inventory/Data/InventorySystemSetting.h"


void UInventoryWindow::SetInventoryArray(
	const TArray<FInventoryData>& ArrayRef)
{
	InventoryArray = ArrayRef;
	Update();
}

void UInventoryWindow::SetInventorySource(
	UPlayerInventoryComponent* InInventory,
	const EInventoryItemLocation InLocation,
	const TArray<FInventoryData>& ArrayRef)
{
	InventoryComponent = InInventory;
	InventoryLocation = InLocation;
	SetInventoryArray(ArrayRef);
}

bool UInventoryWindow::NativeOnDrop(
	const FGeometry& InGeometry,
	const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	const UInventoryDragDropOperation* Operation =
		Cast<UInventoryDragDropOperation>(InOperation);
	if (!Operation || !Operation->Inventory)
	{
		return Super::NativeOnDrop(
			InGeometry,
			InDragDropEvent,
			InOperation);
	}

	Operation->Inventory->DropItem(Operation->SourceHandle);
	return true;
}

void UInventoryWindow::Update()
{
	if (!IsValid(InventorySlots))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s::%s : InventorySlots is invalid."),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__));

		return;
	}

	const int32 InventorySize = InventoryArray.Num();
	const UInventorySystemSetting* Settings =
		GetDefault<UInventorySystemSetting>();
	const int32 GridColumns = Settings
		? FMath::Max(1, Settings->InventoryGridColumns)
		: 1;
	int32 WidgetSlotCount = InventorySlots->GetChildrenCount();

	/*
	 * 현재 패널에 들어 있는 자식들이 모두
	 * UInventorySlot인지 검증합니다.
	 */
	for (int32 Index = 0; Index < WidgetSlotCount; ++Index)
	{
		UInventorySlot* InventorySlot =
			Cast<UInventorySlot>(InventorySlots->GetChildAt(Index));

		if (!IsValid(InventorySlot))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT(
					"%s::%s : InventorySlots contains an invalid child "
					"at index %d. Only UInventorySlot is allowed."),
				*GetClass()->GetName(),
				TEXT(__FUNCTION__),
				Index);

			return;
		}
	}

	/*
	 * 위젯 슬롯이 인벤토리 크기보다 많으면
	 * 뒤쪽 슬롯부터 실제 패널에서 제거합니다.
	 */
	while (WidgetSlotCount > InventorySize)
	{
		InventorySlots->RemoveChildAt(WidgetSlotCount - 1);
		--WidgetSlotCount;
	}

	/*
	 * 위젯 슬롯이 부족하면 생성하여
	 * 실제 패널에 추가합니다.
	 */
	if (WidgetSlotCount < InventorySize)
	{
		if (!IsValid(Settings))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("%s::%s : Inventory system settings are invalid."),
				*GetClass()->GetName(),
				TEXT(__FUNCTION__));

			return;
		}

		if (Settings->InventorySlotWidgetClass.IsNull())
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("%s::%s : InventorySlotWidgetClass is not configured."),
				*GetClass()->GetName(),
				TEXT(__FUNCTION__));

			return;
		}

		TSubclassOf<UInventorySlot> SlotWidgetClass =
			Settings->InventorySlotWidgetClass.LoadSynchronous();
		if (!SlotWidgetClass)
		{
			return;
		}

		APlayerController* Controller = GetOwningPlayer();

		if (!IsValid(Controller))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("%s::%s : Owning player controller is invalid."),
				*GetClass()->GetName(),
				TEXT(__FUNCTION__));

			return;
		}

		while (WidgetSlotCount < InventorySize)
		{
			UInventorySlot* NewSlotWidget =
				CreateWidget<UInventorySlot>(
					Controller,
					SlotWidgetClass);

			if (!IsValid(NewSlotWidget))
			{
				UE_LOG(
					LogTemp,
					Error,
					TEXT("%s::%s : Failed to create inventory slot."),
					*GetClass()->GetName(),
					TEXT(__FUNCTION__));

				return;
			}

			InventorySlots->AddChildToUniformGrid(
				NewSlotWidget,
				WidgetSlotCount / GridColumns,
				WidgetSlotCount % GridColumns);
			++WidgetSlotCount;
		}
	}

	/*
	 * 개수 동기화가 끝난 뒤 전체 슬롯을 갱신합니다.
	 * 이렇게 해야 새로 생성된 슬롯도 같은 호출에서 갱신됩니다.
	 */
	for (int32 Index = 0; Index < InventorySize; ++Index)
	{
		UInventorySlot* InventorySlot =
			CastChecked<UInventorySlot>(
				InventorySlots->GetChildAt(Index));
		if (UUniformGridSlot* GridSlot =
			Cast<UUniformGridSlot>(InventorySlot->Slot))
		{
			GridSlot->SetRow(Index / GridColumns);
			GridSlot->SetColumn(Index % GridColumns);
		}

		FInventoryItemHandle Handle;
		if (InventoryComponent)
		{
			if (InventoryLocation == EInventoryItemLocation::Backpack)
			{
				Handle = InventoryComponent->MakeBackpackHandle(Index);
			}
		}
		InventorySlot->SetSlotContext(
			InventoryComponent,
			Handle,
			InventoryArray[Index]);
	}
}
