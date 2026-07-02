// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Shop/ShopWidgetViewData.h"
#include "ShopCategorySectionWidget.generated.h"

class UButton;
class UListView;
class UTextBlock;
class UShopCategoryEntryData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShopCategorySectionRequestedDelegate, FName, CategoryId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FShopStockRefreshSectionRequestedDelegate);

/**
 * 상점 좌측 카테고리 Section이다.
 * 카테고리 선택과 재고 새로고침 요청을 ShopWindow로 올리고, 아이템 목록은 직접 갱신하지 않는다.
 */
UCLASS()
class OUTBREAK_API UShopCategorySectionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 카테고리 표시 데이터를 ListView Entry Data로 변환해 적용한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Categories")
	void ApplyCategories(const TArray<FShopCategoryViewData>& Categories);

	/** 현재 선택 카테고리 강조 상태를 갱신한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Categories")
	void SetSelectedCategory(FName CategoryId);

	/** 다음 재고 갱신 시간 또는 새로고침 상태 문구를 표시한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Categories")
	void SetStockRefreshText(const FText& RefreshText);

	/** 카테고리 없음 안내 표시를 켜거나 끈다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Categories")
	void SetEmptyState(bool bIsEmpty);

	/** 카테고리 목록과 선택 표시를 초기화한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Categories")
	void ClearCategories();

	/** 상점 입력 잠금 상태에 맞춰 카테고리 영역 입력 가능 여부를 변경한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Categories")
	void SetInteractionEnabled(bool bEnabled);

	UPROPERTY(BlueprintAssignable, Category = "Shop|Categories")
	FShopCategorySectionRequestedDelegate OnCategoryRequested;

	UPROPERTY(BlueprintAssignable, Category = "Shop|Categories")
	FShopStockRefreshSectionRequestedDelegate OnStockRefreshRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	/** 카테고리 Entry를 표시하는 필수 ListView다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UListView> CategoryListView;

	/** 재고 갱신 상태 문구를 표시하는 필수 TextBlock이다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> StockRefreshText;

	/** 재고 새로고침 요청 버튼이다. 자동 새로고침만 쓰는 WBP에서는 생략할 수 있다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> RefreshButton;

	/** 카테고리가 없을 때 표시하는 선택 TextBlock이다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EmptyCategoryText;

private:
	UFUNCTION()
	void HandleRefreshClicked();

	void HandleCategoryItemClicked(UObject* Item);
	void RebuildSelectionState();

	UPROPERTY()
	TArray<TObjectPtr<UShopCategoryEntryData>> CategoryEntryDataObjects;

	UPROPERTY()
	FName SelectedCategoryId;
};
