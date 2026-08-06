// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Inventory/Data/InventoryData.h"
#include "InventoryWindow.generated.h"

/**
 * 
 */
class UPlayerInventoryComponent;
class UEquipmentSlot;
class UUniformGridPanel;

UCLASS()
class OUTBREAK_API UInventoryWindow : public UUserWidget
{
	GENERATED_BODY()
	
	
public:
	void SetInventorySource(UPlayerInventoryComponent* InInventory);
	void Update();

	virtual bool NativeOnDrop(
		const FGeometry& InGeometry,
		const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation) override;
	
public:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> InventoryBackPackSlots;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> InventoryQuickSlots;

	// The Widget Blueprint currently uses this spelling. Keep the C++ binding
	// exact so the user's edited asset does not need to be renamed or resaved.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> InventoryContainterSlots;
	
private:
	bool SynchronizeGridSlots(
		UUniformGridPanel* Grid,
		int32 DesiredSlotCount);
	void UpdateItemGrid(
		UUniformGridPanel* Grid,
		EInventoryItemLocation Location,
		const TArray<FInventoryData>& Items);
	void UpdateQuickSlotGrid(int32 QuickSlotCount);
	void RefreshEquipmentSlots();

	UPROPERTY(Transient)
	TObjectPtr<UPlayerInventoryComponent> InventoryComponent;
};
