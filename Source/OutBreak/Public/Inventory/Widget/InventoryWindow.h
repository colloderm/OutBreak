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
	
	
	void SetInventoryArray(TArray<FInventoryData>& ArrayRef);
	void Update();
	
	
public:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UUniformGridPanel> InventorySlots;
	
private:
	UPROPERTY(meta = (AllowPrivateAccess = "true"))
	TArray<FInventoryData>& InventoryArray;
};
