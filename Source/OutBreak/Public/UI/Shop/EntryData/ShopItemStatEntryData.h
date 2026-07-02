// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UI/Shop/ShopWidgetViewData.h"
#include "ShopItemStatEntryData.generated.h"

/**
 * 상세 스탯 ListView에 전달되는 표시 전용 UObject다.
 * Entry Widget 재사용 시 이전 Delta 표시가 남지 않도록 ResetEntry와 함께 사용한다.
 */
UCLASS(BlueprintType)
class OUTBREAK_API UShopItemStatEntryData : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(const FShopItemStatViewData& InViewData);

	const FShopItemStatViewData& GetViewData() const { return ViewData; }

	UFUNCTION(BlueprintPure, Category = "Shop|EntryData")
	FShopItemStatViewData GetViewDataCopy() const { return ViewData; }

private:
	UPROPERTY(BlueprintReadOnly, Category = "Shop|EntryData", meta = (AllowPrivateAccess = "true"))
	FShopItemStatViewData ViewData;
};
