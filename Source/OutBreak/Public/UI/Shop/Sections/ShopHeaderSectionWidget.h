// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Shop/ShopWidgetViewData.h"
#include "ShopHeaderSectionWidget.generated.h"

class UImage;
class UListView;
class UProgressBar;
class UTextBlock;
class UShopCurrencyEntryData;

/**
 * 상점 화면 상단 Header Section이다.
 * 상인 프로필, 평판, 플레이어 지갑을 표시하며 구매나 데이터 요청은 수행하지 않는다.
 */
UCLASS()
class OUTBREAK_API UShopHeaderSectionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 외부 게임 데이터에서 변환된 상인 표시 정보를 Header에 적용한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Header")
	void ApplyVendorData(const FShopVendorViewData& VendorData);

	/** 플레이어 지갑 스냅샷을 Currency ListView Entry Data로 변환해 표시한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Header")
	void ApplyWalletData(const FShopWalletViewData& WalletData);

	/** 상인 정보가 없거나 상점이 닫힐 때 이전 Header 표시값을 제거한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Header")
	void ClearVendorData();

	/** 지갑 정보가 없을 때 Currency ListView를 비운다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Header")
	void ClearWalletData();

	/** 상점 전체 입력 가능 여부와 맞춰 Header의 상호작용 상태를 변경한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Header")
	void SetInteractionEnabled(bool bEnabled);

protected:
	/**
	 * 상인 초상화를 표시하는 필수 Image다.
	 * WBP Component 이름은 VendorPortraitImage여야 한다.
	 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> VendorPortraitImage;

	/** 상인 역할을 표시하는 필수 TextBlock이다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> VendorRoleText;

	/** 상인 이름을 표시하는 필수 TextBlock이다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> VendorNameText;

	/** 상인 설명을 표시하는 필수 TextBlock이다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> VendorDescriptionText;

	/** 평판 이름을 표시하는 필수 TextBlock이다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ReputationNameText;

	/** 평판 수치를 표시하는 필수 TextBlock이다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ReputationValueText;

	/** 평판 진행률을 표시하는 필수 ProgressBar다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> ReputationProgressBar;

	/** 보유 화폐를 표시하는 필수 ListView다. Entry Widget Class는 WBP에서 지정한다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UListView> CurrencyListView;

private:
	UPROPERTY()
	TArray<TObjectPtr<UShopCurrencyEntryData>> CurrencyEntryDataObjects;
};
