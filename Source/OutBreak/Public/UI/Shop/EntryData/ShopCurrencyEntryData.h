// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UI/Shop/ShopWidgetViewData.h"
#include "ShopCurrencyEntryData.generated.h"

/**
 * Header의 화폐 ListView에 전달되는 표시 전용 UObject다.
 * 지갑 시스템 객체를 직접 참조하지 않아 UI 수명과 게임 데이터 수명을 분리한다.
 */
UCLASS(BlueprintType)
class OUTBREAK_API UShopCurrencyEntryData : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(const FShopCurrencyViewData& InViewData);

	const FShopCurrencyViewData& GetViewData() const { return ViewData; }

	UFUNCTION(BlueprintPure, Category = "Shop|EntryData")
	FShopCurrencyViewData GetViewDataCopy() const { return ViewData; }

private:
	UPROPERTY(BlueprintReadOnly, Category = "Shop|EntryData", meta = (AllowPrivateAccess = "true"))
	FShopCurrencyViewData ViewData;
};
