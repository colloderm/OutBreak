// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OBConsumableWidget.generated.h"

class UTextBlock;
class UOBInventoryComponent;

UCLASS()
class OUTBREAK_API UOBConsumableWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetInventory(UOBInventoryComponent* InInventory);
	
protected:
	virtual void NativeDestruct() override;
	void Refresh();
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BandageCountText;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> GrenadeCountText;
	
	TWeakObjectPtr<UOBInventoryComponent> Inventory;
	FDelegateHandle ChangedHandle;
};
