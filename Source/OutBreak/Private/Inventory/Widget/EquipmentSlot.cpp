#include "Inventory/Widget/EquipmentSlot.h"

#include "Inventory/Components/PlayerInventoryComponent.h"
#include "Inventory/Widget/InventoryDragDropOperation.h"

void UEquipmentSlot::SetInventorySource(
	UPlayerInventoryComponent* InInventory)
{
	EquipmentInventory = InInventory;
	RefreshEquipmentSlot();
}

void UEquipmentSlot::SetSlotCategory(
	const EOBEquipmentSlot InSlotCategory)
{
	if (SlotCategory == InSlotCategory)
	{
		return;
	}

	SlotCategory = InSlotCategory;
	bReportedInvalidSlotCategory = false;
	OnEquipmentSlotCategoryChanged(SlotCategory);
	RefreshEquipmentSlot();
}

void UEquipmentSlot::RefreshEquipmentSlot()
{
	if (SlotCategory == EOBEquipmentSlot::None)
	{
		if (!IsDesignTime() && !bReportedInvalidSlotCategory)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("%s::%s : SlotCategory is None. Configure the "
					"equipment slot instance in the Widget Blueprint."),
				*GetClass()->GetName(),
				TEXT(__FUNCTION__));
			bReportedInvalidSlotCategory = true;
		}

		SetSlotContext(
			EquipmentInventory,
			FInventoryItemHandle(),
			FInventoryData());
		return;
	}

	FInventoryData DisplayData;
	FInventoryItemHandle EquipmentHandle;
	if (EquipmentInventory)
	{
		EquipmentHandle =
			EquipmentInventory->MakeEquipmentHandle(SlotCategory);
		if (!EquipmentInventory->GetEquippedItem(
			SlotCategory,
			DisplayData))
		{
			DisplayData = FInventoryData();
		}
	}

	SetSlotContext(
		EquipmentInventory,
		EquipmentHandle,
		DisplayData);
}

void UEquipmentSlot::NativePreConstruct()
{
	Super::NativePreConstruct();
	OnEquipmentSlotCategoryChanged(SlotCategory);
	RefreshEquipmentSlot();
}

void UEquipmentSlot::NativeOnDragEnter(
	const FGeometry& InGeometry,
	const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	Super::NativeOnDragEnter(
		InGeometry,
		InDragDropEvent,
		InOperation);
	OnEquipmentDragStateChanged(
		true,
		CanAcceptOperation(InOperation));
}

void UEquipmentSlot::NativeOnDragLeave(
	const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
	OnEquipmentDragStateChanged(false, false);
}

bool UEquipmentSlot::NativeOnDrop(
	const FGeometry& InGeometry,
	const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	const UInventoryDragDropOperation* Operation =
		Cast<UInventoryDragDropOperation>(InOperation);
	const bool bCanDrop = CanAcceptOperation(InOperation);
	OnEquipmentDragStateChanged(false, false);

	if (!bCanDrop)
	{
		if (Operation)
		{
			OnEquipmentDropRejected(Operation->SourceHandle);
		}
		return false;
	}

	return Super::NativeOnDrop(
		InGeometry,
		InDragDropEvent,
		InOperation);
}

bool UEquipmentSlot::CanAcceptOperation(
	UDragDropOperation* InOperation) const
{
	const UInventoryDragDropOperation* Operation =
		Cast<UInventoryDragDropOperation>(InOperation);
	return Operation &&
		EquipmentInventory &&
		Operation->Inventory == EquipmentInventory &&
		EquipmentInventory->CanEquipItemToSlot(
			Operation->SourceHandle,
			SlotCategory);
}
