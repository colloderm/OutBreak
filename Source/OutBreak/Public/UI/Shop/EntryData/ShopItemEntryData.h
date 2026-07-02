// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UI/Shop/ShopWidgetViewData.h"
#include "ShopItemEntryData.generated.h"

/**
 * Item ListView에 전달되는 표시 전용 UObject다.
 * 실제 아이템 객체 대신 ItemId와 렌더링용 텍스트/브러시만 보관한다.
 */
UCLASS(BlueprintType)
class OUTBREAK_API UShopItemEntryData : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(const FShopItemSummaryViewData& InViewData, bool bInSelected);

	const FShopItemSummaryViewData& GetViewData() const { return ViewData; }

	UFUNCTION(BlueprintPure, Category = "Shop|EntryData")
	FShopItemSummaryViewData GetViewDataCopy() const { return ViewData; }

	UFUNCTION(BlueprintPure, Category = "Shop|EntryData")
	FName GetItemId() const { return ViewData.ItemId; }

	UFUNCTION(BlueprintPure, Category = "Shop|EntryData")
	bool IsSelected() const { return bSelected; }

	void SetSelected(bool bInSelected) { bSelected = bInSelected; }

private:
	UPROPERTY(BlueprintReadOnly, Category = "Shop|EntryData", meta = (AllowPrivateAccess = "true"))
	FShopItemSummaryViewData ViewData;

	UPROPERTY(BlueprintReadOnly, Category = "Shop|EntryData", meta = (AllowPrivateAccess = "true"))
	bool bSelected = false;
};
