// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "Inventory/Data/InventoryData.h"

#include "InventorySlot.generated.h"

/**
 *
 */
class UButton;
class UTextBlock;
class UImage;
class UPlayerInventoryComponent;

UCLASS()
class OUTBREAK_API UInventorySlot : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void Update();
	void SetSlotData(const FInventoryData& Data);

	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	void SetSlotContext(
		UPlayerInventoryComponent* InInventory,
		const FInventoryItemHandle& InHandle,
		const FInventoryData& Data);

	UFUNCTION(BlueprintCallable, Category = "Inventory|QuickSlot")
	void SetQuickSlotContext(
		UPlayerInventoryComponent* InInventory,
		int32 QuickSlotIndex);

	UFUNCTION(BlueprintPure, Category = "Inventory|DragDrop")
	FInventoryItemHandle GetSlotHandle() const { return SlotHandle; }
	
	void SetSlotMetaData(UTexture2D* Image, int Stack);

protected:
	virtual FReply NativeOnMouseButtonDown(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent,
		UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(
		const FGeometry& InGeometry,
		const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation) override;

private:
	void ClearSlot();

	UPROPERTY(Transient)
	FInventoryData InventoryData;

	UPROPERTY(Transient)
	TObjectPtr<UPlayerInventoryComponent> InventoryComponent;

	UPROPERTY(Transient)
	FInventoryItemHandle SlotHandle;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> ItemImage;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ItemStack;
	
	
};
