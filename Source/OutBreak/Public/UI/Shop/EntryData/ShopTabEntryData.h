// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UI/Shop/ShopWidgetViewData.h"
#include "ShopTabEntryData.generated.h"

/**
 * Tab ListView에 전달되는 표시 전용 UObject다.
 * 재사용되는 Entry Widget이 이전 선택 상태를 남기지 않도록 선택 여부를 데이터와 함께 전달한다.
 */
UCLASS(BlueprintType)
class OUTBREAK_API UShopTabEntryData : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(const FShopTabViewData& InViewData, bool bInSelected);

	const FShopTabViewData& GetViewData() const { return ViewData; }

	UFUNCTION(BlueprintPure, Category = "Shop|EntryData")
	FShopTabViewData GetViewDataCopy() const { return ViewData; }

	UFUNCTION(BlueprintPure, Category = "Shop|EntryData")
	FName GetTabId() const { return ViewData.TabId; }

	UFUNCTION(BlueprintPure, Category = "Shop|EntryData")
	bool IsSelected() const { return bSelected; }

	void SetSelected(bool bInSelected) { bSelected = bInSelected; }

private:
	UPROPERTY(BlueprintReadOnly, Category = "Shop|EntryData", meta = (AllowPrivateAccess = "true"))
	FShopTabViewData ViewData;

	UPROPERTY(BlueprintReadOnly, Category = "Shop|EntryData", meta = (AllowPrivateAccess = "true"))
	bool bSelected = false;
};
