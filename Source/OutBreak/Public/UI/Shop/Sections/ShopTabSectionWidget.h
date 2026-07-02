// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Shop/ShopWidgetViewData.h"
#include "ShopTabSectionWidget.generated.h"

class UListView;
class UShopTabEntryData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShopTabSectionRequestedDelegate, FName, TabId);

/**
 * 상점 탭 목록 Section이다.
 * 탭 선택 요청만 ShopWindow에 전달하고 카테고리나 아이템 목록을 직접 갱신하지 않는다.
 */
UCLASS()
class OUTBREAK_API UShopTabSectionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 탭 표시 데이터를 ListView Entry Data로 변환해 적용한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Tabs")
	void ApplyTabs(const TArray<FShopTabViewData>& Tabs);

	/** 현재 선택된 탭 표시 상태를 갱신한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Tabs")
	void SetSelectedTab(FName TabId);

	/** 탭 목록을 비우고 이전 선택 상태를 제거한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Tabs")
	void ClearTabs();

	/** 상점 입력 잠금 상태에 맞춰 탭 입력 가능 여부를 변경한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Tabs")
	void SetInteractionEnabled(bool bEnabled);

	UPROPERTY(BlueprintAssignable, Category = "Shop|Tabs")
	FShopTabSectionRequestedDelegate OnTabRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	/**
	 * 탭 Entry를 표시하는 필수 ListView다.
	 * Entry Widget Class는 UShopTabEntryWidget 기반 WBP로 지정한다.
	 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UListView> TabListView;

private:
	void HandleTabItemClicked(UObject* Item);
	void RebuildSelectionState();

	UPROPERTY()
	TArray<TObjectPtr<UShopTabEntryData>> TabEntryDataObjects;

	UPROPERTY()
	FName SelectedTabId;
};
