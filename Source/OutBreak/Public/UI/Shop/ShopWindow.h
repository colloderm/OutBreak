// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Shop/ShopWidgetTypes.h"
#include "UI/Widgets/OBInteractionWidget.h"
#include "ShopWindow.generated.h"

class UButton;
class UWidgetSwitcher;
class UShopCategory;
class UShopItemInspector;
class UShopItemList;
class UShopkeeperPortrait;
class UUserCurrency;

UCLASS()
class OUTBREAK_API UShopWindow : public UOBInteractionWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Shop")
	void InitializeShop(const FShopWindowViewData& InData);

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void RefreshShop(const FShopWindowViewData& InData);

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void ClearShop();

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void UpdateCurrency(const FUserCurrencyViewData& InData);

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void UpdateItem(const FShopItemViewData& InData);

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void UpdateItems(const TArray<FShopItemViewData>& InItems);

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void UpdateCategories(const TArray<FShopCategoryViewData>& InCategories);

	UFUNCTION(BlueprintCallable, Category = "Shop")
	bool SelectCategory(FName CategoryId);

	UFUNCTION(BlueprintCallable, Category = "Shop")
	bool SelectItem(FName ItemId);

	UFUNCTION(BlueprintPure, Category = "Shop")
	FName GetSelectedCategoryId() const;

	UFUNCTION(BlueprintPure, Category = "Shop")
	FName GetSelectedItemId() const;

	const FShopWindowViewData& GetCurrentViewData() const;
	
	// 상단 탭 전환. 목록은 NPC가 다시 만들어 넣는다(창고/잔액이 실시간이므로).
	UFUNCTION(BlueprintCallable, Category = "Shop")
	void SetActiveTab(EShopTab NewTab);

	UFUNCTION(BlueprintPure, Category = "Shop")
	EShopTab GetActiveTab() const { return ActiveTab; }

	UPROPERTY(BlueprintAssignable, Category = "Shop")
	FShopWindowTabChangedSignature OnTabChanged;

	virtual void RequestClose() override;

	UPROPERTY(BlueprintAssignable, Category = "Shop")
	FShopWindowCategorySelectionChangedSignature OnCategorySelectionChanged;

	UPROPERTY(BlueprintAssignable, Category = "Shop")
	FShopWindowItemSelectionChangedSignature OnItemSelectionChanged;

	UPROPERTY(BlueprintAssignable, Category = "Shop")
	FShopWindowActionRequestedSignature OnPurchaseRequested;

	UPROPERTY(BlueprintAssignable, Category = "Shop")
	FShopWindowActionRequestedSignature OnExchangeRequested;

	UPROPERTY(BlueprintAssignable, Category = "Shop")
	FShopWindowActionRequestedSignature OnShopActionRequested;

	UPROPERTY(BlueprintAssignable, Category = "Shop")
	FShopWindowCloseRequestedSignature OnShopCloseRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	
	UFUNCTION() 
	void HandleBuyTabClicked();
	
	UFUNCTION() 
	void HandleSellTabClicked();
	
	UFUNCTION() 
	void HandleTradeTabClicked();
	
	UFUNCTION() 
	void HandleRequestsTabClicked();

	// 내용이 있는 탭만 누를 수 있다. 교환/의뢰는 아직 비어 있다.
	static bool IsTabAvailable(EShopTab Tab);
	
	// 탭 → 스위처 슬롯. Buy/Sell은 레이아웃이 같아 슬롯 0을 공유하고 데이터만 갈아끼운다
	// (BindWidget 위젯은 트리에 하나씩만 존재할 수 있어 본문을 복제할 수 없다).
	static int32 GetSwitcherIndexForTab(EShopTab Tab);
	
	void CycleTab(int32 Delta);
	void RefreshTabVisuals();
	
	// 선택된 탭 강조 등 시각 처리를 WBP에서 하고 싶을 때 구현. 안 만들어도 동작한다.
	UFUNCTION(BlueprintImplementableEvent, Category = "Shop")
	void OnTabVisualChanged(EShopTab NewActiveTab);
	
	void ApplySwitcherSlot();
	
	UFUNCTION()
	void HandleCategorySelected(FName CategoryId);

	UFUNCTION()
	void HandleItemSelected(FName ItemId);

	UFUNCTION()
	void HandleActionTriggered(FName ActionId);

	void BindChildDelegates();
	void UnbindChildDelegates();
	void ApplyViewData(const FShopWindowViewData& InData, bool bPreserveSelection);
	void RebuildIndexes();
	void RefreshCurrentCategoryItems();
	void RefreshInspector();
	bool SelectCategoryInternal(FName CategoryId, bool bBroadcast);
	bool SelectItemInternal(FName ItemId, bool bBroadcast);
	FName ResolveCategorySelection(FName PreferredCategoryId, FName PreferredItemId) const;
	FName ResolveItemSelection(FName CategoryId, FName PreferredItemId) const;
	const FShopCategoryViewData* FindCategory(FName CategoryId) const;
	const FShopItemViewData* FindItem(FName ItemId) const;
	const FShopItemViewData* FindDisplayedItem(FName ItemId) const;
	const FShopActionViewData* FindActionForSelectedItem(FName ActionId) const;
	bool IsCategorySelectable(FName CategoryId) const;
	bool IsItemSelectableInCategory(FName ItemId, FName CategoryId) const;
	TArray<FShopItemViewData> BuildItemsForCategory(FName CategoryId) const;

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShopkeeperPortrait> WBP_ShopkeeperPortrait;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUserCurrency> WBP_UserCurrency;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShopCategory> WBP_ShopCategory;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShopItemList> WBP_ShopItemList;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShopItemInspector> WBP_ItemInspector;
	
	// 상단 탭 버튼. WBP에서 이 이름 그대로 만들어야 붙는다(없으면 조용히 무시).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BTN_Buy;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BTN_Sell;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BTN_Trade;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BTN_Requests;

	// 탭별 본문 전환. 슬롯 0 = 구매/판매 공용, 1 = 교환, 2 = 의뢰.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> WS_ShopBody;
	
	// 탭 선택 강조 배경. WBP에 같은 이름으로 있으면 자동 토글된다(없으면 무시).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> BG_TabBuySelected;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> BG_TabSellSelected;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> BG_TabTradeSelected;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> BG_TabRequestsSelected;

	EShopTab ActiveTab = EShopTab::Buy;

	UPROPERTY(Transient)
	FShopWindowViewData CurrentViewData;

	UPROPERTY(Transient)
	TArray<FShopItemViewData> DisplayedItems;

	TMap<FName, int32> CategoryIndexById;
	TMap<FName, int32> ItemIndexById;

	FName CurrentSelectedCategoryId = NAME_None;
	FName CurrentSelectedItemId = NAME_None;
};
