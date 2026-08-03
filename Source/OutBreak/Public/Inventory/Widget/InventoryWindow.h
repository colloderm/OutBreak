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

UCLASS()
class OUTBREAK_API UInventoryWindow : public UUserWidget
{
	GENERATED_BODY()
	
	
public:
	void SetInventoryArray(const TArray<FInventoryData>& ArrayRef);
	void SetInventorySource(
		UPlayerInventoryComponent* InInventory,
		EInventoryItemLocation InLocation,
		const TArray<FInventoryData>& ArrayRef);
	void Update();

	virtual bool NativeOnDrop(
		const FGeometry& InGeometry,
		const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation) override;
	
public:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UUniformGridPanel> InventorySlots;
	
private:
	UPROPERTY(Transient)
	TArray<FInventoryData> InventoryArray;

	UPROPERTY(Transient)
	TObjectPtr<UPlayerInventoryComponent> InventoryComponent;

	UPROPERTY(Transient)
	EInventoryItemLocation InventoryLocation =
		EInventoryItemLocation::Backpack;
};
