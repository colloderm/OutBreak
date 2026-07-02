// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UI/Shop/ShopWidgetViewData.h"
#include "ShopCategoryEntryData.generated.h"

/**
 * Category ListView에 전달되는 표시 전용 UObject다.
 * 카테고리 선택 요청은 이 객체의 CategoryId를 ShopWindow가 Command로 변환한다.
 */
UCLASS(BlueprintType)
class OUTBREAK_API UShopCategoryEntryData : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(const FShopCategoryViewData& InViewData, bool bInSelected);

	const FShopCategoryViewData& GetViewData() const { return ViewData; }

	UFUNCTION(BlueprintPure, Category = "Shop|EntryData")
	FShopCategoryViewData GetViewDataCopy() const { return ViewData; }

	UFUNCTION(BlueprintPure, Category = "Shop|EntryData")
	FName GetCategoryId() const { return ViewData.CategoryId; }

	UFUNCTION(BlueprintPure, Category = "Shop|EntryData")
	bool IsSelected() const { return bSelected; }

	void SetSelected(bool bInSelected) { bSelected = bInSelected; }

private:
	UPROPERTY(BlueprintReadOnly, Category = "Shop|EntryData", meta = (AllowPrivateAccess = "true"))
	FShopCategoryViewData ViewData;

	UPROPERTY(BlueprintReadOnly, Category = "Shop|EntryData", meta = (AllowPrivateAccess = "true"))
	bool bSelected = false;
};
