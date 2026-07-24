// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OBDialogueNPC.h"
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
