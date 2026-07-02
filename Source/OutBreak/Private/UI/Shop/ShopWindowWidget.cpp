// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Shop/ShopWindowWidget.h"

#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "UI/Shop/Sections/ShopActionSectionWidget.h"
#include "UI/Shop/Sections/ShopCategorySectionWidget.h"
#include "UI/Shop/Sections/ShopFooterSectionWidget.h"
#include "UI/Shop/Sections/ShopHeaderSectionWidget.h"
#include "UI/Shop/Sections/ShopItemDetailSectionWidget.h"
#include "UI/Shop/Sections/ShopItemListSectionWidget.h"
#include "UI/Shop/Sections/ShopTabSectionWidget.h"
#include "UI/Shop/ShopDataGatewayBase.h"
#include "UI/Shop/ShopViewModel.h"

void UShopWindowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindSectionDelegates();
	BindGatewayDelegates();
	BindViewModelDelegates();
	ApplyStateToSections(CurrentViewState);
}

void UShopWindowWidget::NativeDestruct()
{
	UnbindSectionDelegates();
	UnbindGatewayDelegates();
	UnbindViewModelDelegates();

	if (ShopDataGateway)
	{
		ShopDataGateway->CancelPendingRequests();
	}

	Super::NativeDestruct();
}

void UShopWindowWidget::SetShopViewModel(UShopViewModel* InViewModel)
{
	if (ShopViewModel == InViewModel)
	{
		return;
	}

	UnbindViewModelDelegates();
	ShopViewModel = InViewModel;
	BindViewModelDelegates();

	if (ShopViewModel)
	{
		ApplyShopViewState(ShopViewModel->GetViewState());
	}
}

void UShopWindowWidget::SetShopDataGateway(UShopDataGatewayBase* InGateway)
{
	if (ShopDataGateway == InGateway)
	{
		return;
	}

	if (ShopDataGateway)
	{
		ShopDataGateway->CancelPendingRequests();
	}

	UnbindGatewayDelegates();
	ShopDataGateway = InGateway;
	BindGatewayDelegates();
}

void UShopWindowWidget::InitializeShop(const FShopInitializationData& InitializationData)
{
	LastInitializationData = InitializationData;
	CurrentViewState.Vendor.VendorId = InitializationData.VendorId;
	CurrentViewState.CurrentTabId = InitializationData.InitialTabId;
	CurrentViewState.CurrentCategoryId = InitializationData.InitialCategoryId;

	if (ShopDataGateway)
	{
		SetBusy(true);
		ShopDataGateway->RequestInitialShopState(InitializationData);
	}
	else
	{
		ApplyShopViewState(CurrentViewState);
	}
}

void UShopWindowWidget::OpenShop()
{
	if (!IsInViewport())
	{
		AddToViewport();
	}

	SetVisibility(ESlateVisibility::Visible);
	SetInteractionEnabled(CurrentViewState.bCanInteract);
}

void UShopWindowWidget::ApplyShopViewState(const FShopViewState& ViewState)
{
	CurrentViewState = ViewState;

	if (ShopViewModel && !bApplyingViewModelState)
	{
		bApplyingViewModelState = true;
		ShopViewModel->ApplyViewState(ViewState);
		bApplyingViewModelState = false;
	}

	ApplyStateToSections(CurrentViewState);
}

void UShopWindowWidget::ApplyVendorData(const FShopVendorViewData& VendorData)
{
	CurrentViewState.Vendor = VendorData;
	ApplyShopViewState(CurrentViewState);
}

void UShopWindowWidget::ApplyWalletData(const FShopWalletViewData& WalletData)
{
	CurrentViewState.Wallet = WalletData;
	ApplyShopViewState(CurrentViewState);
}

void UShopWindowWidget::ApplyTabs(const TArray<FShopTabViewData>& Tabs)
{
	CurrentViewState.Tabs = Tabs;
	ApplyShopViewState(CurrentViewState);
}

void UShopWindowWidget::ApplyCategories(const TArray<FShopCategoryViewData>& Categories)
{
	CurrentViewState.Categories = Categories;
	ApplyShopViewState(CurrentViewState);
}

void UShopWindowWidget::ApplyVisibleItems(const TArray<FShopItemSummaryViewData>& Items)
{
	CurrentViewState.VisibleItems = Items;
	ApplyShopViewState(CurrentViewState);
}

void UShopWindowWidget::ApplySelectedItem(const FShopItemDetailViewData& ItemDetail)
{
	CurrentViewState.SelectedItem = ItemDetail;
	CurrentViewState.CurrentItemId = ItemDetail.ItemId;
	CurrentViewState.bHasSelectedItem = !ItemDetail.ItemId.IsNone();
	ApplyShopViewState(CurrentViewState);
}

void UShopWindowWidget::ApplyTransactionResult(const FShopTransactionResult& Result)
{
	SetBusy(false);

	if (!Result.bSuccess)
	{
		ApplyError(Result.Error);
		return;
	}

	CurrentViewState.Wallet = Result.UpdatedWallet;
	if (Result.UpdatedVisibleItems.Num() > 0)
	{
		CurrentViewState.VisibleItems = Result.UpdatedVisibleItems;
	}
	if (Result.bHasUpdatedItemDetail)
	{
		CurrentViewState.SelectedItem = Result.UpdatedItemDetail;
		CurrentViewState.CurrentItemId = Result.UpdatedItemDetail.ItemId;
		CurrentViewState.bHasSelectedItem = !Result.UpdatedItemDetail.ItemId.IsNone();
	}

	CurrentViewState.Error = FShopErrorViewData();
	ApplyShopViewState(CurrentViewState);
}

void UShopWindowWidget::ApplyError(const FShopErrorViewData& ErrorData)
{
	CurrentViewState.Error = ErrorData;
	CurrentViewState.bBusy = false;
	ApplyShopViewState(CurrentViewState);
}

void UShopWindowWidget::SetBusy(bool bBusy)
{
	CurrentViewState.bBusy = bBusy;
	ApplyShopViewState(CurrentViewState);
}

void UShopWindowWidget::SetInteractionEnabled(bool bEnabled)
{
	CurrentViewState.bCanInteract = bEnabled;
	ApplyShopViewState(CurrentViewState);
}

void UShopWindowWidget::ResetShop()
{
	CurrentViewState = FShopViewState();
	ApplyShopViewState(CurrentViewState);
}

void UShopWindowWidget::CloseShop()
{
	if (ShopDataGateway)
	{
		ShopDataGateway->CancelPendingRequests();
	}

	SetVisibility(ESlateVisibility::Collapsed);
}

void UShopWindowWidget::BindSectionDelegates()
{
	if (bSectionDelegatesBound)
	{
		return;
	}

	if (TabSection)
	{
		TabSection->OnTabRequested.AddUniqueDynamic(this, &UShopWindowWidget::HandleTabRequested);
	}
	if (CategorySection)
	{
		CategorySection->OnCategoryRequested.AddUniqueDynamic(this, &UShopWindowWidget::HandleCategoryRequested);
		CategorySection->OnStockRefreshRequested.AddUniqueDynamic(this, &UShopWindowWidget::HandleStockRefreshRequested);
	}
	if (ItemListSection)
	{
		ItemListSection->OnItemSelectionRequested.AddUniqueDynamic(this, &UShopWindowWidget::HandleItemSelectionRequested);
		ItemListSection->OnSortRequested.AddUniqueDynamic(this, &UShopWindowWidget::HandleSortRequested);
	}
	if (UShopActionSectionWidget* ActionSection = ResolveActionSection())
	{
		ActionSection->OnPurchaseRequested.AddUniqueDynamic(this, &UShopWindowWidget::HandlePurchaseRequested);
		ActionSection->OnBarterRequested.AddUniqueDynamic(this, &UShopWindowWidget::HandleBarterRequested);
		ActionSection->OnQuantityChanged.AddUniqueDynamic(this, &UShopWindowWidget::HandleQuantityChanged);
	}
	if (FooterSection)
	{
		FooterSection->OnCloseRequested.AddUniqueDynamic(this, &UShopWindowWidget::HandleCloseRequested);
		FooterSection->OnBackRequested.AddUniqueDynamic(this, &UShopWindowWidget::HandleBackRequested);
		FooterSection->OnCompareRequested.AddUniqueDynamic(this, &UShopWindowWidget::HandleCompareRequested);
		FooterSection->OnHelpRequested.AddUniqueDynamic(this, &UShopWindowWidget::HandleHelpRequested);
	}

	bSectionDelegatesBound = true;
}

void UShopWindowWidget::UnbindSectionDelegates()
{
	if (!bSectionDelegatesBound)
	{
		return;
	}

	if (TabSection)
	{
		TabSection->OnTabRequested.RemoveDynamic(this, &UShopWindowWidget::HandleTabRequested);
	}
	if (CategorySection)
	{
		CategorySection->OnCategoryRequested.RemoveDynamic(this, &UShopWindowWidget::HandleCategoryRequested);
		CategorySection->OnStockRefreshRequested.RemoveDynamic(this, &UShopWindowWidget::HandleStockRefreshRequested);
	}
	if (ItemListSection)
	{
		ItemListSection->OnItemSelectionRequested.RemoveDynamic(this, &UShopWindowWidget::HandleItemSelectionRequested);
		ItemListSection->OnSortRequested.RemoveDynamic(this, &UShopWindowWidget::HandleSortRequested);
	}
	if (UShopActionSectionWidget* ActionSection = ResolveActionSection())
	{
		ActionSection->OnPurchaseRequested.RemoveDynamic(this, &UShopWindowWidget::HandlePurchaseRequested);
		ActionSection->OnBarterRequested.RemoveDynamic(this, &UShopWindowWidget::HandleBarterRequested);
		ActionSection->OnQuantityChanged.RemoveDynamic(this, &UShopWindowWidget::HandleQuantityChanged);
	}
	if (FooterSection)
	{
		FooterSection->OnCloseRequested.RemoveDynamic(this, &UShopWindowWidget::HandleCloseRequested);
		FooterSection->OnBackRequested.RemoveDynamic(this, &UShopWindowWidget::HandleBackRequested);
		FooterSection->OnCompareRequested.RemoveDynamic(this, &UShopWindowWidget::HandleCompareRequested);
		FooterSection->OnHelpRequested.RemoveDynamic(this, &UShopWindowWidget::HandleHelpRequested);
	}

	bSectionDelegatesBound = false;
}

void UShopWindowWidget::BindGatewayDelegates()
{
	if (!ShopDataGateway)
	{
		return;
	}

	ShopDataGateway->OnShopViewStateReceived.AddUniqueDynamic(this, &UShopWindowWidget::HandleGatewayViewStateReceived);
	ShopDataGateway->OnCategoryItemsReceived.AddUniqueDynamic(this, &UShopWindowWidget::HandleGatewayCategoryItemsReceived);
	ShopDataGateway->OnItemDetailReceived.AddUniqueDynamic(this, &UShopWindowWidget::HandleGatewayItemDetailReceived);
	ShopDataGateway->OnTransactionCompleted.AddUniqueDynamic(this, &UShopWindowWidget::HandleGatewayTransactionCompleted);
	ShopDataGateway->OnGatewayError.AddUniqueDynamic(this, &UShopWindowWidget::HandleGatewayError);
	ShopDataGateway->OnBusyStateChanged.AddUniqueDynamic(this, &UShopWindowWidget::HandleGatewayBusyStateChanged);
}

void UShopWindowWidget::UnbindGatewayDelegates()
{
	if (!ShopDataGateway)
	{
		return;
	}

	ShopDataGateway->OnShopViewStateReceived.RemoveDynamic(this, &UShopWindowWidget::HandleGatewayViewStateReceived);
	ShopDataGateway->OnCategoryItemsReceived.RemoveDynamic(this, &UShopWindowWidget::HandleGatewayCategoryItemsReceived);
	ShopDataGateway->OnItemDetailReceived.RemoveDynamic(this, &UShopWindowWidget::HandleGatewayItemDetailReceived);
	ShopDataGateway->OnTransactionCompleted.RemoveDynamic(this, &UShopWindowWidget::HandleGatewayTransactionCompleted);
	ShopDataGateway->OnGatewayError.RemoveDynamic(this, &UShopWindowWidget::HandleGatewayError);
	ShopDataGateway->OnBusyStateChanged.RemoveDynamic(this, &UShopWindowWidget::HandleGatewayBusyStateChanged);
}

void UShopWindowWidget::BindViewModelDelegates()
{
	if (ShopViewModel)
	{
		ShopViewModel->OnViewStateChanged.AddUniqueDynamic(this, &UShopWindowWidget::HandleViewModelStateChanged);
	}
}

void UShopWindowWidget::UnbindViewModelDelegates()
{
	if (ShopViewModel)
	{
		ShopViewModel->OnViewStateChanged.RemoveDynamic(this, &UShopWindowWidget::HandleViewModelStateChanged);
	}
}

void UShopWindowWidget::ApplyStateToSections(const FShopViewState& ViewState)
{
	if (HeaderSection)
	{
		HeaderSection->ApplyVendorData(ViewState.Vendor);
		HeaderSection->ApplyWalletData(ViewState.Wallet);
		HeaderSection->SetInteractionEnabled(ViewState.bCanInteract);
	}
	if (TabSection)
	{
		TabSection->SetSelectedTab(ViewState.CurrentTabId);
		TabSection->ApplyTabs(ViewState.Tabs);
		TabSection->SetInteractionEnabled(ViewState.bCanInteract);
	}
	if (CategorySection)
	{
		CategorySection->SetSelectedCategory(ViewState.CurrentCategoryId);
		CategorySection->ApplyCategories(ViewState.Categories);
		CategorySection->SetInteractionEnabled(ViewState.bCanInteract);
	}
	if (ItemListSection)
	{
		ItemListSection->SetCurrentSort(ViewState.CurrentSortId);
		ItemListSection->SetSelectedItem(ViewState.CurrentItemId);
		ItemListSection->ApplyItems(ViewState.VisibleItems);
		ItemListSection->SetLoadingState(ViewState.bBusy);
		ItemListSection->SetInteractionEnabled(ViewState.bCanInteract);
	}
	if (ItemDetailSection)
	{
		if (ViewState.bHasSelectedItem)
		{
			ItemDetailSection->ApplyItemDetail(ViewState.SelectedItem);
		}
		else
		{
			ItemDetailSection->ClearItemDetail();
		}
		ItemDetailSection->SetInteractionEnabled(ViewState.bCanInteract);
	}
	if (FooterSection)
	{
		FooterSection->SetInteractionEnabled(ViewState.bCanInteract);
	}
	if (LoadingOverlay)
	{
		LoadingOverlay->SetVisibility(ViewState.bBusy ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	const bool bHasError = !ViewState.Error.Message.IsEmpty();
	if (ErrorOverlay)
	{
		ErrorOverlay->SetVisibility(bHasError ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (ErrorMessageText)
	{
		ErrorMessageText->SetText(ViewState.Error.Message);
	}
}

UShopActionSectionWidget* UShopWindowWidget::ResolveActionSection() const
{
	return ItemDetailSection ? ItemDetailSection->GetActionSection() : nullptr;
}

void UShopWindowWidget::HandleTabRequested(FName TabId)
{
	CurrentViewState.CurrentTabId = TabId;
	CurrentViewState.CurrentCategoryId = NAME_None;
	CurrentViewState.CurrentItemId = NAME_None;
	CurrentViewState.bHasSelectedItem = false;

	FShopTabRequest Request;
	Request.VendorId = CurrentViewState.Vendor.VendorId;
	Request.TabId = TabId;
	OnTabRequested.Broadcast(Request);

	ApplyShopViewState(CurrentViewState);
}

void UShopWindowWidget::HandleCategoryRequested(FName CategoryId)
{
	CurrentViewState.CurrentCategoryId = CategoryId;
	CurrentViewState.CurrentItemId = NAME_None;
	CurrentViewState.bHasSelectedItem = false;

	FShopCategoryRequest Request;
	Request.VendorId = CurrentViewState.Vendor.VendorId;
	Request.TabId = CurrentViewState.CurrentTabId;
	Request.CategoryId = CategoryId;
	Request.SortId = CurrentViewState.CurrentSortId;
	OnCategoryRequested.Broadcast(Request);

	if (ShopDataGateway)
	{
		SetBusy(true);
		ShopDataGateway->RequestCategoryItems(Request);
	}
	else
	{
		ApplyShopViewState(CurrentViewState);
	}
}

void UShopWindowWidget::HandleItemSelectionRequested(FName ItemId)
{
	CurrentViewState.CurrentItemId = ItemId;
	CurrentViewState.bHasSelectedItem = false;

	FShopItemSelectionRequest Request;
	Request.VendorId = CurrentViewState.Vendor.VendorId;
	Request.CategoryId = CurrentViewState.CurrentCategoryId;
	Request.ItemId = ItemId;
	OnItemSelectionRequested.Broadcast(Request);

	if (ShopDataGateway)
	{
		SetBusy(true);
		ShopDataGateway->RequestItemDetail(Request);
	}
	else
	{
		ApplyShopViewState(CurrentViewState);
	}
}

void UShopWindowWidget::HandleSortRequested(FName SortId)
{
	CurrentViewState.CurrentSortId = SortId;

	FShopSortRequest Request;
	Request.VendorId = CurrentViewState.Vendor.VendorId;
	Request.CategoryId = CurrentViewState.CurrentCategoryId;
	Request.SortId = SortId;
	OnSortRequested.Broadcast(Request);

	ApplyShopViewState(CurrentViewState);
}

void UShopWindowWidget::HandlePurchaseRequested(FShopPurchaseRequest Request)
{
	Request.VendorId = CurrentViewState.Vendor.VendorId;
	Request.CurrencyId = CurrentViewState.SelectedItem.CurrencyId;
	OnPurchaseRequested.Broadcast(Request);

	if (ShopDataGateway)
	{
		SetBusy(true);
		ShopDataGateway->SubmitPurchase(Request);
	}
}

void UShopWindowWidget::HandleBarterRequested(FShopBarterRequest Request)
{
	Request.VendorId = CurrentViewState.Vendor.VendorId;
	OnBarterRequested.Broadcast(Request);

	if (ShopDataGateway)
	{
		SetBusy(true);
		ShopDataGateway->SubmitBarter(Request);
	}
}

void UShopWindowWidget::HandleQuantityChanged(int32 Quantity)
{
	CurrentViewState.SelectedItem.ActionState.Quantity = Quantity;
	OnQuantityChanged.Broadcast(Quantity);
}

void UShopWindowWidget::HandleStockRefreshRequested()
{
	FShopStockRefreshRequest Request;
	Request.VendorId = CurrentViewState.Vendor.VendorId;
	Request.CategoryId = CurrentViewState.CurrentCategoryId;
	OnStockRefreshRequested.Broadcast(Request);

	if (ShopDataGateway)
	{
		SetBusy(true);
		ShopDataGateway->RequestStockRefresh(Request);
	}
}

void UShopWindowWidget::HandleCompareRequested()
{
	OnCompareRequested.Broadcast();
}

void UShopWindowWidget::HandleCloseRequested()
{
	OnCloseRequested.Broadcast();
	CloseShop();
}

void UShopWindowWidget::HandleBackRequested()
{
	OnBackRequested.Broadcast();
}

void UShopWindowWidget::HandleHelpRequested()
{
	OnHelpRequested.Broadcast();
}

void UShopWindowWidget::HandleGatewayViewStateReceived(FShopViewState ViewState)
{
	SetBusy(false);
	ViewState.bBusy = false;
	ApplyShopViewState(ViewState);
}

void UShopWindowWidget::HandleGatewayCategoryItemsReceived(FName CategoryId, const TArray<FShopItemSummaryViewData>& Items)
{
	if (!CurrentViewState.CurrentCategoryId.IsNone() && CategoryId != CurrentViewState.CurrentCategoryId)
	{
		return;
	}

	CurrentViewState.VisibleItems = Items;
	CurrentViewState.CurrentItemId = NAME_None;
	CurrentViewState.bHasSelectedItem = false;
	CurrentViewState.bBusy = false;
	ApplyShopViewState(CurrentViewState);
}

void UShopWindowWidget::HandleGatewayItemDetailReceived(FName ItemId, FShopItemDetailViewData ItemDetail)
{
	if (!CurrentViewState.CurrentItemId.IsNone() && ItemId != CurrentViewState.CurrentItemId)
	{
		return;
	}

	CurrentViewState.SelectedItem = ItemDetail;
	CurrentViewState.CurrentItemId = ItemDetail.ItemId;
	CurrentViewState.bHasSelectedItem = !ItemDetail.ItemId.IsNone();
	CurrentViewState.bBusy = false;
	ApplyShopViewState(CurrentViewState);
}

void UShopWindowWidget::HandleGatewayTransactionCompleted(FShopTransactionResult Result)
{
	ApplyTransactionResult(Result);
}

void UShopWindowWidget::HandleGatewayError(FShopErrorViewData ErrorData)
{
	ApplyError(ErrorData);
}

void UShopWindowWidget::HandleGatewayBusyStateChanged(bool bBusy)
{
	SetBusy(bBusy);
}

void UShopWindowWidget::HandleViewModelStateChanged(FShopViewState ViewState)
{
	if (bApplyingViewModelState)
	{
		return;
	}

	bApplyingViewModelState = true;
	ApplyShopViewState(ViewState);
	bApplyingViewModelState = false;
}
