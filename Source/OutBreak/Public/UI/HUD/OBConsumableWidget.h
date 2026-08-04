// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OBConsumableWidget.generated.h"

class UPlayerInventoryComponent;
class UTextBlock;

UCLASS()
class OUTBREAK_API UOBConsumableWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetInventory(UPlayerInventoryComponent* InInventory);
	
protected:
	virtual void NativeDestruct() override;
	void Refresh();
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BandageCountText;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> GrenadeCountText;
	
	TWeakObjectPtr<UPlayerInventoryComponent> Inventory;
	FDelegateHandle ChangedHandle;
};
