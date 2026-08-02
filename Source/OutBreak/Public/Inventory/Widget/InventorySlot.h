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

UCLASS()
class OUTBREAK_API UInventorySlot : public UUserWidget
{
	GENERATED_BODY()
	
public:
	auto Update() -> void;
	
	void SetSlotMetaData(UTexture2D* Image, int Stack);
	
protected:
	void SetSlotData(FInventoryData* Data);
	
private:
	FInventoryData* InventoryData;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> ItemImage;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ItemStack;
	
	
};
