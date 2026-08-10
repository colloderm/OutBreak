// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/Widget/InventoryWindow.h"
#include "Blueprint/WidgetTree.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Inventory/Components/PlayerInventoryComponent.h"
#include "Inventory/Widget/EquipmentSlot.h"
#include "Inventory/Widget/InventoryDragDropOperation.h"
#include "Inventory/Widget/InventorySlot.h"
#include "Inventory/Data/InventorySystemSetting.h"
#include "Inventory/Widget/WeaponAttachmentBar.h"

void UInventoryWindow::SetInventorySource(
	UPlayerInventoryComponent* InInventory)
{
	InventoryComponent = InInventory;
	Update();
}

bool UInventoryWindow::NativeOnDrop(
	const FGeometry& InGeometry,
	const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	const UInventoryDragDropOperation* Operation =
		Cast<UInventoryDragDropOperation>(InOperation);
	if (!Operation || !Operation->Inventory ||
		Operation->Inventory != InventoryComponent)
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
	if (!IsValid(InventoryComponent))
	{
		return;
	}

	const TArray<FInventoryData> BackpackItems =
		InventoryComponent->GetBackpackItemsForUI();
	const TArray<FInventoryData> ContainerItems =
		InventoryComponent->GetContainerItemsForUI();
	const TArray<FQuickSlotData> QuickSlots =
		InventoryComponent->GetQuickSlotsForUI();

	UpdateItemGrid(
		InventoryBackPackSlots,
		EInventoryItemLocation::Backpack,
		BackpackItems);
	UpdateItemGrid(
		InventoryContainterSlots,
		EInventoryItemLocation::Container,
		ContainerItems);
	UpdateQuickSlotGrid(QuickSlots.Num());
	RefreshEquipmentSlots();
}

bool UInventoryWindow::SynchronizeGridSlots(
	UUniformGridPanel* Grid,
	const int32 DesiredSlotCount)
{
	if (!IsValid(Grid))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s::%s : Required inventory grid is invalid."),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__));
		return false;
	}

	const int32 SlotCount = FMath::Max(0, DesiredSlotCount);
	const UInventorySystemSetting* Settings =
		GetDefault<UInventorySystemSetting>();
	const int32 GridColumns = Settings
		? FMath::Max(1, Settings->InventoryGridColumns)
		: 1;
	int32 WidgetSlotCount = Grid->GetChildrenCount();

	/*
	 * 현재 패널에 들어 있는 자식들이 모두
	 * UInventorySlot인지 검증합니다.
	 */
	for (int32 Index = 0; Index < WidgetSlotCount; ++Index)
	{
		UInventorySlot* InventorySlot =
			Cast<UInventorySlot>(Grid->GetChildAt(Index));

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

			return false;
		}
	}

	/*
	 * 위젯 슬롯이 인벤토리 크기보다 많으면
	 * 뒤쪽 슬롯부터 실제 패널에서 제거합니다.
	 */
	while (WidgetSlotCount > SlotCount)
	{
		Grid->RemoveChildAt(WidgetSlotCount - 1);
		--WidgetSlotCount;
	}

	/*
	 * 위젯 슬롯이 부족하면 생성하여
	 * 실제 패널에 추가합니다.
	 */
	if (WidgetSlotCount < SlotCount)
	{
		if (!IsValid(Settings))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("%s::%s : Inventory system settings are invalid."),
				*GetClass()->GetName(),
				TEXT(__FUNCTION__));

			return false;
		}

		if (Settings->InventorySlotWidgetClass.IsNull())
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("%s::%s : InventorySlotWidgetClass is not configured."),
				*GetClass()->GetName(),
				TEXT(__FUNCTION__));

			return false;
		}

		TSubclassOf<UInventorySlot> SlotWidgetClass =
			Settings->InventorySlotWidgetClass.LoadSynchronous();
		if (!SlotWidgetClass)
		{
			return false;
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

			return false;
		}

		while (WidgetSlotCount < SlotCount)
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

				return false;
			}

			Grid->AddChildToUniformGrid(
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
	for (int32 Index = 0; Index < SlotCount; ++Index)
	{
		UInventorySlot* InventorySlot =
			CastChecked<UInventorySlot>(
				Grid->GetChildAt(Index));
		if (UUniformGridSlot* GridSlot =
			Cast<UUniformGridSlot>(InventorySlot->Slot))
		{
			GridSlot->SetRow(Index / GridColumns);
			GridSlot->SetColumn(Index % GridColumns);
			GridSlot->SetHorizontalAlignment(HAlign_Fill);
			GridSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}
	return true;
}

void UInventoryWindow::UpdateItemGrid(
	UUniformGridPanel* Grid,
	const EInventoryItemLocation Location,
	const TArray<FInventoryData>& Items)
{
	if (!SynchronizeGridSlots(Grid, Items.Num()))
	{
		return;
	}

	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		FInventoryItemHandle Handle;
		if (Location == EInventoryItemLocation::Backpack)
		{
			Handle = InventoryComponent->MakeBackpackHandle(Index);
		}
		else if (Location == EInventoryItemLocation::Container)
		{
			Handle = InventoryComponent->MakeContainerHandle(Index);
		}

		CastChecked<UInventorySlot>(Grid->GetChildAt(Index))->SetSlotContext(
			InventoryComponent,
			Handle,
			Items[Index]);
	}
}

void UInventoryWindow::UpdateQuickSlotGrid(const int32 QuickSlotCount)
{
	if (!SynchronizeGridSlots(InventoryQuickSlots, QuickSlotCount))
	{
		return;
	}

	for (int32 Index = 0; Index < QuickSlotCount; ++Index)
	{
		CastChecked<UInventorySlot>(
			InventoryQuickSlots->GetChildAt(Index))->SetQuickSlotContext(
				InventoryComponent,
				Index);
	}
}

void UInventoryWindow::RefreshEquipmentSlots()
{
	if (!WidgetTree) return;

	// Equipment slot instances may be arranged anywhere in the InventoryWindow
	// designer. Discovering them by class keeps the C++ binding independent of
	// their panel hierarchy and widget names.
	WidgetTree->ForEachWidget(
		[this](UWidget* Widget)
		{
			if (UEquipmentSlot* EquipmentSlot = Cast<UEquipmentSlot>(Widget))
			{
				EquipmentSlot->SetInventorySource(InventoryComponent);
			}
			else if (UWeaponAttachmentBar* AttachmentBar = Cast<UWeaponAttachmentBar>(Widget))
			{
				AttachmentBar->SetInventorySource(InventoryComponent);
			}
		});
}
