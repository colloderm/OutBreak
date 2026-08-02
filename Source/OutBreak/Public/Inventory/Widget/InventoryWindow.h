// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryWindow.generated.h"

/**
 * 
 */
UCLASS()
class OUTBREAK_API UInventoryWindow : public UUserWidget
{
	GENERATED_BODY()
	
	
public:
	
	void Update();
	
	
public:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UUniformGridPanel> InventorySlots;
};
