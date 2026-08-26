// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OBConsumableWidget.generated.h"

class UImage;
class UPlayerInventoryComponent;
class UTextBlock;

UCLASS()
class OUTBREAK_API UOBConsumableWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetInventory(UPlayerInventoryComponent* InInventory);
	
protected:
	virtual void NativeConstruct() override;

	// 아이콘은 세션 중 바뀌지 않는다. 생성 시 한 번만 채운다.
	void ApplyIcons();
	
	virtual void NativeDestruct() override;
	void Refresh();

	// 구독은 NativeDestruct에서 끊긴다. 재진입 경로마다 다시 붙일 수 있게 분리했다.
	void BindInventoryChanged();
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> BandageIcon;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BandageCountText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> GrenadeIcon;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> GrenadeCountText;
	
	TWeakObjectPtr<UPlayerInventoryComponent> Inventory;
	FDelegateHandle ChangedHandle;
};
