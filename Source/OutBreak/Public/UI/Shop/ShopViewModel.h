// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UI/Shop/ShopWidgetViewData.h"
#include "ShopViewModel.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShopViewModelStateChangedDelegate, FShopViewState, ViewState);

/**
 * 상점 UI의 명시적 상태 저장소다.
 * 외부 코드는 배열을 직접 수정하지 않고 Setter나 ApplyViewState를 통해 스냅샷을 교체한다.
 */
UCLASS(BlueprintType)
class OUTBREAK_API UShopViewModel : public UObject
{
	GENERATED_BODY()

public:
	/** 전체 화면 상태를 한 번에 교체하고 단일 변경 이벤트를 발생시킨다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|ViewModel")
	void ApplyViewState(const FShopViewState& ViewState);

	/** ViewModel을 빈 상점 상태로 되돌리고 각 Section이 Clear 함수를 호출할 수 있도록 변경 이벤트를 발생시킨다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|ViewModel")
	void Reset();

	UFUNCTION(BlueprintCallable, Category = "Shop|ViewModel")
	void SetVendor(const FShopVendorViewData& InVendor);

	UFUNCTION(BlueprintCallable, Category = "Shop|ViewModel")
	void SetWallet(const FShopWalletViewData& InWallet);

	UFUNCTION(BlueprintCallable, Category = "Shop|ViewModel")
	void SetTabs(const TArray<FShopTabViewData>& InTabs);

	UFUNCTION(BlueprintCallable, Category = "Shop|ViewModel")
	void SetCategories(const TArray<FShopCategoryViewData>& InCategories);

	UFUNCTION(BlueprintCallable, Category = "Shop|ViewModel")
	void SetVisibleItems(const TArray<FShopItemSummaryViewData>& InVisibleItems);

	UFUNCTION(BlueprintCallable, Category = "Shop|ViewModel")
	void SetSelectedItem(const FShopItemDetailViewData& InSelectedItem, bool bInHasSelectedItem);

	UFUNCTION(BlueprintCallable, Category = "Shop|ViewModel")
	void SetCurrentTab(FName InTabId);

	UFUNCTION(BlueprintCallable, Category = "Shop|ViewModel")
	void SetCurrentCategory(FName InCategoryId);

	UFUNCTION(BlueprintCallable, Category = "Shop|ViewModel")
	void SetCurrentItem(FName InItemId);

	UFUNCTION(BlueprintCallable, Category = "Shop|ViewModel")
	void SetCurrentSort(FName InSortId);

	UFUNCTION(BlueprintCallable, Category = "Shop|ViewModel")
	void SetBusy(bool bInBusy);

	UFUNCTION(BlueprintCallable, Category = "Shop|ViewModel")
	void SetInteractionEnabled(bool bInCanInteract);

	UFUNCTION(BlueprintCallable, Category = "Shop|ViewModel")
	void SetError(const FShopErrorViewData& InError);

	UFUNCTION(BlueprintPure, Category = "Shop|ViewModel")
	FShopViewState GetViewState() const;

	UFUNCTION(BlueprintPure, Category = "Shop|ViewModel")
	FName GetCurrentVendorId() const { return Vendor.VendorId; }

	UFUNCTION(BlueprintPure, Category = "Shop|ViewModel")
	FName GetCurrentTabId() const { return CurrentTabId; }

	UFUNCTION(BlueprintPure, Category = "Shop|ViewModel")
	FName GetCurrentCategoryId() const { return CurrentCategoryId; }

	UFUNCTION(BlueprintPure, Category = "Shop|ViewModel")
	FName GetCurrentItemId() const { return CurrentItemId; }

	UPROPERTY(BlueprintAssignable, Category = "Shop|ViewModel")
	FShopViewModelStateChangedDelegate OnViewStateChanged;

private:
	/** Setter 경로가 끝난 뒤 전체 스냅샷을 만들어 구독자에게 전달한다. */
	void BroadcastViewStateChanged();

	UPROPERTY()
	FShopVendorViewData Vendor;

	UPROPERTY()
	FShopWalletViewData Wallet;

	UPROPERTY()
	TArray<FShopTabViewData> Tabs;

	UPROPERTY()
	TArray<FShopCategoryViewData> Categories;

	UPROPERTY()
	TArray<FShopItemSummaryViewData> VisibleItems;

	UPROPERTY()
	FShopItemDetailViewData SelectedItem;

	UPROPERTY()
	bool bHasSelectedItem = false;

	UPROPERTY()
	FName CurrentTabId;

	UPROPERTY()
	FName CurrentCategoryId;

	UPROPERTY()
	FName CurrentItemId;

	UPROPERTY()
	FName CurrentSortId;

	UPROPERTY()
	bool bBusy = false;

	UPROPERTY()
	bool bCanInteract = true;

	UPROPERTY()
	FShopErrorViewData LastError;
};
