#pragma once

#include "CoreMinimal.h"
#include "Equipment/Data/OBEquipmentTypes.h"
#include "Inventory/Widget/InventorySlot.h"
#include "EquipmentSlot.generated.h"

class UPlayerInventoryComponent;

/**
 * Inventory slot adapter for a single logical equipment position.
 *
 * The widget owns only presentation and the configured target position.
 * UPlayerInventoryComponent remains the authoritative owner of equipment data
 * and performs every move/equip validation on the server.
 */
UCLASS()
class OUTBREAK_API UEquipmentSlot : public UInventorySlot
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
	void SetInventorySource(UPlayerInventoryComponent* InInventory);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
	void SetSlotCategory(EOBEquipmentSlot InSlotCategory);

	UFUNCTION(BlueprintPure, Category = "Inventory|Equipment")
	EOBEquipmentSlot GetSlotCategory() const { return SlotCategory; }

	UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
	void RefreshEquipmentSlot();

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeOnDragEnter(
		const FGeometry& InGeometry,
		const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(
		const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(
		const FGeometry& InGeometry,
		const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation) override;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic,
		Category = "Inventory|Equipment",
		meta = (DisplayName = "On Equipment Slot Category Changed"))
	void OnEquipmentSlotCategoryChanged(EOBEquipmentSlot InSlotCategory);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic,
		Category = "Inventory|Equipment",
		meta = (DisplayName = "On Equipment Drag State Changed"))
	void OnEquipmentDragStateChanged(bool bIsDraggingOver, bool bCanDrop);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic,
		Category = "Inventory|Equipment",
		meta = (DisplayName = "On Equipment Drop Rejected"))
	void OnEquipmentDropRejected(const FInventoryItemHandle& SourceHandle);

private:
	bool CanAcceptOperation(UDragDropOperation* InOperation) const;
	
	// 이 슬롯에 장착된 무기의 InstanceId. 무기가 없거나 무기 슬롯이 아니면 무효.
	FGuid GetEquippedWeaponInstanceId() const;

	// 드롭된 것이 이 슬롯의 무기에 설치 가능한 부착물이면 true.
	bool CanInstallFromOperation(UDragDropOperation* InOperation) const;

	// Configure this per WBP_InventoryWindow widget instance.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Equipment",
		meta = (AllowPrivateAccess = "true", ExposeOnSpawn = "true"))
	EOBEquipmentSlot SlotCategory = EOBEquipmentSlot::None;

	UPROPERTY(Transient)
	TObjectPtr<UPlayerInventoryComponent> EquipmentInventory;

	bool bReportedInvalidSlotCategory = false;
};
