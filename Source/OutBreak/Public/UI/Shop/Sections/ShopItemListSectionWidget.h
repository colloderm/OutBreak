// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Shop/ShopWidgetViewData.h"
#include "ShopItemListSectionWidget.generated.h"

class UButton;
class UListView;
class UTextBlock;
class UWidget;
class UShopItemEntryData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShopItemSelectionSectionRequestedDelegate, FName, ItemId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShopSortSectionRequestedDelegate, FName, SortId);

/**
 * 상점 중앙 아이템 목록 Section이다.
 * 목록 표시와 선택 요청 전달만 담당하고 상세 정보 조회나 구매 처리는 ShopWindow/Gateway로 넘긴다.
 */
UCLASS()
class OUTBREAK_API UShopItemListSectionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 아이템 요약 데이터를 ListView Entry Data로 변환해 적용한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Items")
	void ApplyItems(const TArray<FShopItemSummaryViewData>& Items);

	/** 현재 선택 아이템 강조 상태를 갱신한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Items")
	void SetSelectedItem(FName ItemId);

	/** 목록 상단의 아이템 개수 표시를 갱신한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Items")
	void SetItemCount(int32 ItemCount);

	/** 목록이 비어 있을 때 표시할 안내 상태를 변경한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Items")
	void SetEmptyState(bool bIsEmpty);

	/** Gateway 요청 중 로딩 표시를 켜거나 끈다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Items")
	void SetLoadingState(bool bIsLoading);

	/** 아이템 목록과 선택 표시를 초기화한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Items")
	void ClearItems();

	/** 현재 정렬 식별자를 저장한다. SortButton 클릭 시 이 값을 ShopWindow로 전달한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Items")
	void SetCurrentSort(FName SortId);

	/** 상점 입력 잠금 상태에 맞춰 목록 입력 가능 여부를 변경한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Items")
	void SetInteractionEnabled(bool bEnabled);

	UPROPERTY(BlueprintAssignable, Category = "Shop|Items")
	FShopItemSelectionSectionRequestedDelegate OnItemSelectionRequested;

	UPROPERTY(BlueprintAssignable, Category = "Shop|Items")
	FShopSortSectionRequestedDelegate OnSortRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	/** 아이템 Entry를 표시하는 필수 ListView다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UListView> ItemListView;

	/** 현재 표시 중인 아이템 개수를 보여주는 필수 TextBlock이다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ItemCountText;

	/** 아이템 목록이 비었을 때 표시하는 선택 TextBlock이다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EmptyStateText;

	/** 목록 로딩 중 표시하는 선택 Widget이다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LoadingIndicator;

	/** 정렬 요청 버튼이다. 별도 ComboBox나 정렬 Widget을 쓰는 WBP에서는 생략할 수 있다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> SortButton;

private:
	UFUNCTION()
	void HandleSortClicked();

	void HandleItemClicked(UObject* Item);
	void RebuildSelectionState();

	UPROPERTY()
	TArray<TObjectPtr<UShopItemEntryData>> ItemEntryDataObjects;

	UPROPERTY()
	FName SelectedItemId;

	UPROPERTY()
	FName CurrentSortId;
};
