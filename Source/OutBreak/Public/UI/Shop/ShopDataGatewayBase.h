// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UI/Shop/ShopWidgetCommands.h"
#include "UI/Shop/ShopWidgetViewData.h"
#include "ShopDataGatewayBase.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShopViewStateReceivedDelegate, FShopViewState, ViewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FShopCategoryItemsReceivedDelegate, FName, CategoryId, const TArray<FShopItemSummaryViewData>&, Items);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FShopItemDetailReceivedDelegate, FName, ItemId, FShopItemDetailViewData, ItemDetail);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShopTransactionCompletedDelegate, FShopTransactionResult, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShopGatewayErrorDelegate, FShopErrorViewData, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShopBusyStateChangedDelegate, bool, bBusy);

/**
 * 상점 UI와 실제 게임 데이터 공급 계층 사이의 추상 Gateway다.
 * 이 클래스는 데이터베이스, 서버, 인벤토리, DataTable을 직접 구현하지 않고 파생 클래스가 연결할 요청/응답 계약만 제공한다.
 */
UCLASS(Abstract, Blueprintable)
class OUTBREAK_API UShopDataGatewayBase : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 상점 최초 진입 시 전체 화면 스냅샷을 요청한다.
	 * 외부 시스템은 응답이 준비되면 OnShopViewStateReceived를 Broadcast한다.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Shop|Gateway")
	void RequestInitialShopState(const FShopInitializationData& InitializationData);

	/**
	 * 탭/카테고리/정렬 조건에 맞는 중앙 아이템 목록을 요청한다.
	 * 비동기 응답은 OnCategoryItemsReceived로 전달한다.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Shop|Gateway")
	void RequestCategoryItems(const FShopCategoryRequest& Request);

	/**
	 * 선택 아이템의 상세 정보를 요청한다.
	 * 응답이 늦게 도착할 수 있으므로 Request.ItemId를 기준으로 ShopWindow가 현재 선택과 비교한다.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Shop|Gateway")
	void RequestItemDetail(const FShopItemSelectionRequest& Request);

	/**
	 * 구매 요청을 외부 거래 시스템으로 제출한다.
	 * 실제 화폐 차감이나 인벤토리 추가는 이 클래스가 아니라 파생 Gateway나 게임 시스템에서 처리한다.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Shop|Gateway")
	void SubmitPurchase(const FShopPurchaseRequest& Request);

	/**
	 * 교환 요청을 외부 거래 시스템으로 제출한다.
	 * UI는 요청 Command만 만들고 성공/실패 결과는 OnTransactionCompleted로 받는다.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Shop|Gateway")
	void SubmitBarter(const FShopBarterRequest& Request);

	/**
	 * 현재 상점 재고 새로고침을 요청한다.
	 * 서버나 데이터 저장소가 필요한 경우 파생 Gateway에서만 접근한다.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Shop|Gateway")
	void RequestStockRefresh(const FShopStockRefreshRequest& Request);

	/**
	 * 상점이 닫히거나 Gateway가 교체될 때 아직 처리 중인 요청을 취소한다.
	 * 파생 구현은 비동기 콜백이 제거된 Widget으로 들어오지 않도록 여기서 정리한다.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Shop|Gateway")
	void CancelPendingRequests();

	/**
	 * Gateway 수명 종료 시 외부 구독이나 캐시를 해제한다.
	 * ShopWindow의 NativeDestruct나 SetShopDataGateway 교체 경로에서 호출할 수 있다.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Shop|Gateway")
	void ShutdownGateway();

	UPROPERTY(BlueprintAssignable, Category = "Shop|Gateway")
	FShopViewStateReceivedDelegate OnShopViewStateReceived;

	UPROPERTY(BlueprintAssignable, Category = "Shop|Gateway")
	FShopCategoryItemsReceivedDelegate OnCategoryItemsReceived;

	UPROPERTY(BlueprintAssignable, Category = "Shop|Gateway")
	FShopItemDetailReceivedDelegate OnItemDetailReceived;

	UPROPERTY(BlueprintAssignable, Category = "Shop|Gateway")
	FShopTransactionCompletedDelegate OnTransactionCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Shop|Gateway")
	FShopGatewayErrorDelegate OnGatewayError;

	UPROPERTY(BlueprintAssignable, Category = "Shop|Gateway")
	FShopBusyStateChangedDelegate OnBusyStateChanged;
};
