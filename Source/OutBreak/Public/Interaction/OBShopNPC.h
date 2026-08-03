// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OBDialogueNPC.h"
#include "UI/Shop/ShopWidgetTypes.h"
#include "OBShopNPC.generated.h"

class UOBWeaponCatalog;
class UShopWindow;

UCLASS()
class OUTBREAK_API AOBShopNPC : public AOBDialogueNPC
{
	GENERATED_BODY()

protected:
	virtual void HandleAction(EOBDialogueAction Action) override;

	// 상점 구매/닫기 델리게이트 바인딩 대상.
	UFUNCTION()
	void OnPurchaseRequested(FName ShopId, FName ItemId, FName ActionId, int32 Quantity);
	
	UFUNCTION()
	void OnSellRequested(FName ShopId, FName ItemId, FName ActionId, int32 Quantity);
	
	UFUNCTION()
	void HandleTabChanged(FName ShopId, EShopTab Tab);

	// 현재 탭 기준으로 목록을 다시 만든다. bResetSelection=true면 선택도 초기화.
	void RefreshShopView(bool bResetSelection);

	UFUNCTION()
	void OnShopClosed(FName ShopId);

	void OpenShop();
	void CloseShop();

protected:
	UPROPERTY(EditAnywhere, Category = "Shop")
	TObjectPtr<UOBWeaponCatalog> WeaponCatalog;

	UPROPERTY(EditAnywhere, Category = "Shop")
	TSubclassOf<UShopWindow> ShopWindowClass;

	UPROPERTY(Transient)
	TObjectPtr<UShopWindow> ActiveShop;
};
