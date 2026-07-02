// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Shop/ShopViewModel.h"

void UShopViewModel::ApplyViewState(const FShopViewState& ViewState)
{
	Vendor = ViewState.Vendor;
	Wallet = ViewState.Wallet;
	Tabs = ViewState.Tabs;
	Categories = ViewState.Categories;
	VisibleItems = ViewState.VisibleItems;
	SelectedItem = ViewState.SelectedItem;
	bHasSelectedItem = ViewState.bHasSelectedItem;
	CurrentTabId = ViewState.CurrentTabId;
	CurrentCategoryId = ViewState.CurrentCategoryId;
	CurrentItemId = ViewState.CurrentItemId;
	CurrentSortId = ViewState.CurrentSortId;
	bBusy = ViewState.bBusy;
	bCanInteract = ViewState.bCanInteract;
	LastError = ViewState.Error;

	BroadcastViewStateChanged();
}

void UShopViewModel::Reset()
{
	Vendor = FShopVendorViewData();
	Wallet = FShopWalletViewData();
	Tabs.Reset();
	Categories.Reset();
	VisibleItems.Reset();
	SelectedItem = FShopItemDetailViewData();
	bHasSelectedItem = false;
	CurrentTabId = NAME_None;
	CurrentCategoryId = NAME_None;
	CurrentItemId = NAME_None;
	CurrentSortId = NAME_None;
	bBusy = false;
	bCanInteract = true;
	LastError = FShopErrorViewData();

	BroadcastViewStateChanged();
}

void UShopViewModel::SetVendor(const FShopVendorViewData& InVendor)
{
	Vendor = InVendor;
	BroadcastViewStateChanged();
}

void UShopViewModel::SetWallet(const FShopWalletViewData& InWallet)
{
	Wallet = InWallet;
	BroadcastViewStateChanged();
}

void UShopViewModel::SetTabs(const TArray<FShopTabViewData>& InTabs)
{
	Tabs = InTabs;
	BroadcastViewStateChanged();
}

void UShopViewModel::SetCategories(const TArray<FShopCategoryViewData>& InCategories)
{
	Categories = InCategories;
	BroadcastViewStateChanged();
}

void UShopViewModel::SetVisibleItems(const TArray<FShopItemSummaryViewData>& InVisibleItems)
{
	VisibleItems = InVisibleItems;
	BroadcastViewStateChanged();
}

void UShopViewModel::SetSelectedItem(const FShopItemDetailViewData& InSelectedItem, bool bInHasSelectedItem)
{
	SelectedItem = InSelectedItem;
	bHasSelectedItem = bInHasSelectedItem;
	CurrentItemId = bHasSelectedItem ? InSelectedItem.ItemId : NAME_None;
	BroadcastViewStateChanged();
}

void UShopViewModel::SetCurrentTab(FName InTabId)
{
	if (CurrentTabId != InTabId)
	{
		CurrentTabId = InTabId;
		BroadcastViewStateChanged();
	}
}

void UShopViewModel::SetCurrentCategory(FName InCategoryId)
{
	if (CurrentCategoryId != InCategoryId)
	{
		CurrentCategoryId = InCategoryId;
		BroadcastViewStateChanged();
	}
}

void UShopViewModel::SetCurrentItem(FName InItemId)
{
	if (CurrentItemId != InItemId)
	{
		CurrentItemId = InItemId;
		bHasSelectedItem = !InItemId.IsNone();
		BroadcastViewStateChanged();
	}
}

void UShopViewModel::SetCurrentSort(FName InSortId)
{
	if (CurrentSortId != InSortId)
	{
		CurrentSortId = InSortId;
		BroadcastViewStateChanged();
	}
}

void UShopViewModel::SetBusy(bool bInBusy)
{
	if (bBusy != bInBusy)
	{
		bBusy = bInBusy;
		BroadcastViewStateChanged();
	}
}

void UShopViewModel::SetInteractionEnabled(bool bInCanInteract)
{
	if (bCanInteract != bInCanInteract)
	{
		bCanInteract = bInCanInteract;
		BroadcastViewStateChanged();
	}
}

void UShopViewModel::SetError(const FShopErrorViewData& InError)
{
	LastError = InError;
	BroadcastViewStateChanged();
}

FShopViewState UShopViewModel::GetViewState() const
{
	FShopViewState ViewState;
	ViewState.Vendor = Vendor;
	ViewState.Wallet = Wallet;
	ViewState.Tabs = Tabs;
	ViewState.Categories = Categories;
	ViewState.VisibleItems = VisibleItems;
	ViewState.SelectedItem = SelectedItem;
	ViewState.bHasSelectedItem = bHasSelectedItem;
	ViewState.CurrentTabId = CurrentTabId;
	ViewState.CurrentCategoryId = CurrentCategoryId;
	ViewState.CurrentItemId = CurrentItemId;
	ViewState.CurrentSortId = CurrentSortId;
	ViewState.bBusy = bBusy;
	ViewState.bCanInteract = bCanInteract;
	ViewState.Error = LastError;
	return ViewState;
}

void UShopViewModel::BroadcastViewStateChanged()
{
	OnViewStateChanged.Broadcast(GetViewState());
}
