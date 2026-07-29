// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Shop/ShopWidgetTypes.h"
#include "ShopWindow.generated.h"

class UShopCategory;
class UShopItemInspector;
class UShopItemList;
class UShopkeeperPortrait;
class UUserCurrency;

UCLASS()
class OUTBREAK_API UShopWindow : public UUserWidget
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

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void RequestClose();

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

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

protected:
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

	UPROPERTY(Transient)
	FShopWindowViewData CurrentViewData;

	UPROPERTY(Transient)
	TArray<FShopItemViewData> DisplayedItems;

	TMap<FName, int32> CategoryIndexById;
	TMap<FName, int32> ItemIndexById;

	FName CurrentSelectedCategoryId = NAME_None;
	FName CurrentSelectedItemId = NAME_None;
};
