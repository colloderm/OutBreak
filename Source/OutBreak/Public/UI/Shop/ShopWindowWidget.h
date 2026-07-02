// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Shop/ShopWidgetCommands.h"
#include "UI/Shop/ShopWidgetViewData.h"
#include "ShopWindowWidget.generated.h"

class UShopActionSectionWidget;
class UShopCategorySectionWidget;
class UShopDataGatewayBase;
class UShopFooterSectionWidget;
class UShopHeaderSectionWidget;
class UShopItemDetailSectionWidget;
class UShopItemListSectionWidget;
class UShopTabSectionWidget;
class UShopViewModel;
class UTextBlock;
class UWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShopWindowTabRequestedDelegate, FShopTabRequest, Request);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShopWindowCategoryRequestedDelegate, FShopCategoryRequest, Request);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShopWindowItemSelectionRequestedDelegate, FShopItemSelectionRequest, Request);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShopWindowSortRequestedDelegate, FShopSortRequest, Request);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShopWindowPurchaseRequestedDelegate, FShopPurchaseRequest, Request);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShopWindowBarterRequestedDelegate, FShopBarterRequest, Request);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShopWindowStockRefreshRequestedDelegate, FShopStockRefreshRequest, Request);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShopWindowQuantityChangedDelegate, int32, Quantity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FShopWindowSimpleRequestedDelegate);

/**
 * 상점 화면 전체를 조정하는 중앙 Widget이다.
 * 외부 데이터는 Entry Point 함수로 들어오고, 사용자 입력은 Command Delegate 또는 Gateway 호출로 밖으로 나간다.
 */
UCLASS()
class OUTBREAK_API UShopWindowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** ShopWindow가 구독할 ViewModel을 설정한다. 교체 시 기존 ViewModel Delegate는 먼저 해제한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Window")
	void SetShopViewModel(UShopViewModel* InViewModel);

	/** ShopWindow가 사용할 데이터 Gateway를 설정한다. 교체 시 기존 Gateway Delegate는 먼저 해제한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Window")
	void SetShopDataGateway(UShopDataGatewayBase* InGateway);

	/** 상점 최초 진입 데이터를 저장하고 Gateway가 있으면 초기 화면 상태를 요청한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Window")
	void InitializeShop(const FShopInitializationData& InitializationData);

	/** 상점 Widget을 표시 상태로 전환한다. Widget 생성은 외부 코드나 WBP가 담당한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Window")
	void OpenShop();

	/** 전체 상점 화면 스냅샷을 각 Section에 적용한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Window")
	void ApplyShopViewState(const FShopViewState& ViewState);

	UFUNCTION(BlueprintCallable, Category = "Shop|Window")
	void ApplyVendorData(const FShopVendorViewData& VendorData);

	UFUNCTION(BlueprintCallable, Category = "Shop|Window")
	void ApplyWalletData(const FShopWalletViewData& WalletData);

	UFUNCTION(BlueprintCallable, Category = "Shop|Window")
	void ApplyTabs(const TArray<FShopTabViewData>& Tabs);

	UFUNCTION(BlueprintCallable, Category = "Shop|Window")
	void ApplyCategories(const TArray<FShopCategoryViewData>& Categories);

	UFUNCTION(BlueprintCallable, Category = "Shop|Window")
	void ApplyVisibleItems(const TArray<FShopItemSummaryViewData>& Items);

	UFUNCTION(BlueprintCallable, Category = "Shop|Window")
	void ApplySelectedItem(const FShopItemDetailViewData& ItemDetail);

	UFUNCTION(BlueprintCallable, Category = "Shop|Window")
	void ApplyTransactionResult(const FShopTransactionResult& Result);

	UFUNCTION(BlueprintCallable, Category = "Shop|Window")
	void ApplyError(const FShopErrorViewData& ErrorData);

	UFUNCTION(BlueprintCallable, Category = "Shop|Window")
	void SetBusy(bool bBusy);

	UFUNCTION(BlueprintCallable, Category = "Shop|Window")
	void SetInteractionEnabled(bool bEnabled);

	/** 상점 상태를 비우고 각 Section의 Clear 경로가 실행되도록 한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Window")
	void ResetShop();

	/** 상점을 닫고 Gateway의 미완료 요청 취소 진입점을 호출한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Window")
	void CloseShop();

	UPROPERTY(BlueprintAssignable, Category = "Shop|Window")
	FShopWindowTabRequestedDelegate OnTabRequested;

	UPROPERTY(BlueprintAssignable, Category = "Shop|Window")
	FShopWindowCategoryRequestedDelegate OnCategoryRequested;

	UPROPERTY(BlueprintAssignable, Category = "Shop|Window")
	FShopWindowItemSelectionRequestedDelegate OnItemSelectionRequested;

	UPROPERTY(BlueprintAssignable, Category = "Shop|Window")
	FShopWindowSortRequestedDelegate OnSortRequested;

	UPROPERTY(BlueprintAssignable, Category = "Shop|Window")
	FShopWindowPurchaseRequestedDelegate OnPurchaseRequested;

	UPROPERTY(BlueprintAssignable, Category = "Shop|Window")
	FShopWindowBarterRequestedDelegate OnBarterRequested;

	UPROPERTY(BlueprintAssignable, Category = "Shop|Window")
	FShopWindowStockRefreshRequestedDelegate OnStockRefreshRequested;

	UPROPERTY(BlueprintAssignable, Category = "Shop|Window")
	FShopWindowQuantityChangedDelegate OnQuantityChanged;

	UPROPERTY(BlueprintAssignable, Category = "Shop|Window")
	FShopWindowSimpleRequestedDelegate OnCompareRequested;

	UPROPERTY(BlueprintAssignable, Category = "Shop|Window")
	FShopWindowSimpleRequestedDelegate OnCloseRequested;

	UPROPERTY(BlueprintAssignable, Category = "Shop|Window")
	FShopWindowSimpleRequestedDelegate OnBackRequested;

	UPROPERTY(BlueprintAssignable, Category = "Shop|Window")
	FShopWindowSimpleRequestedDelegate OnHelpRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/**
	 * 상점 상단 정보 Section이다.
	 * WBP_ShopWindow에서 Component 이름을 HeaderSection으로 맞춰야 한다.
	 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShopHeaderSectionWidget> HeaderSection;

	/** 탭 목록 Section이다. WBP Component 이름은 TabSection이어야 한다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShopTabSectionWidget> TabSection;

	/** 좌측 카테고리 Section이다. WBP Component 이름은 CategorySection이어야 한다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShopCategorySectionWidget> CategorySection;

	/** 중앙 아이템 목록 Section이다. WBP Component 이름은 ItemListSection이어야 한다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShopItemListSectionWidget> ItemListSection;

	/** 우측 아이템 상세 Section이다. WBP Component 이름은 ItemDetailSection이어야 한다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShopItemDetailSectionWidget> ItemDetailSection;

	/** 하단 Footer Section이다. WBP Component 이름은 FooterSection이어야 한다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShopFooterSectionWidget> FooterSection;

	/** 로딩 중 상태를 표시하는 선택 Overlay다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LoadingOverlay;

	/** 오류 상태를 표시하는 선택 Overlay다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ErrorOverlay;

	/** 오류 메시지를 표시하는 선택 TextBlock이다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ErrorMessageText;

private:
	void BindSectionDelegates();
	void UnbindSectionDelegates();
	void BindGatewayDelegates();
	void UnbindGatewayDelegates();
	void BindViewModelDelegates();
	void UnbindViewModelDelegates();
	void ApplyStateToSections(const FShopViewState& ViewState);
	UShopActionSectionWidget* ResolveActionSection() const;

	UFUNCTION()
	void HandleTabRequested(FName TabId);

	UFUNCTION()
	void HandleCategoryRequested(FName CategoryId);

	UFUNCTION()
	void HandleItemSelectionRequested(FName ItemId);

	UFUNCTION()
	void HandleSortRequested(FName SortId);

	UFUNCTION()
	void HandlePurchaseRequested(FShopPurchaseRequest Request);

	UFUNCTION()
	void HandleBarterRequested(FShopBarterRequest Request);

	UFUNCTION()
	void HandleQuantityChanged(int32 Quantity);

	UFUNCTION()
	void HandleStockRefreshRequested();

	UFUNCTION()
	void HandleCompareRequested();

	UFUNCTION()
	void HandleCloseRequested();

	UFUNCTION()
	void HandleBackRequested();

	UFUNCTION()
	void HandleHelpRequested();

	UFUNCTION()
	void HandleGatewayViewStateReceived(FShopViewState ViewState);

	UFUNCTION()
	void HandleGatewayCategoryItemsReceived(FName CategoryId, const TArray<FShopItemSummaryViewData>& Items);

	UFUNCTION()
	void HandleGatewayItemDetailReceived(FName ItemId, FShopItemDetailViewData ItemDetail);

	UFUNCTION()
	void HandleGatewayTransactionCompleted(FShopTransactionResult Result);

	UFUNCTION()
	void HandleGatewayError(FShopErrorViewData ErrorData);

	UFUNCTION()
	void HandleGatewayBusyStateChanged(bool bBusy);

	UFUNCTION()
	void HandleViewModelStateChanged(FShopViewState ViewState);

	UPROPERTY()
	TObjectPtr<UShopViewModel> ShopViewModel;

	UPROPERTY()
	TObjectPtr<UShopDataGatewayBase> ShopDataGateway;

	UPROPERTY()
	FShopInitializationData LastInitializationData;

	UPROPERTY()
	FShopViewState CurrentViewState;

	bool bSectionDelegatesBound = false;
	bool bApplyingViewModelState = false;
};
