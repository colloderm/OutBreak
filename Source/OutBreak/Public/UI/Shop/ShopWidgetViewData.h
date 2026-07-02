// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "ShopWidgetViewData.generated.h"

class UMaterialInterface;
class UTexture2D;

/**
 * 플레이어가 가진 단일 화폐 표시 데이터다.
 * 데이터베이스 Row나 인벤토리 객체를 직접 보관하지 않고 WBP 표시값만 가진다.
 */
USTRUCT(BlueprintType)
struct OUTBREAK_API FShopCurrencyViewData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Currency")
	FName CurrencyId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Currency")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Currency")
	int32 Amount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Currency")
	TSoftObjectPtr<UTexture2D> IconTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Currency")
	FSlateBrush IconBrush;
};

/**
 * Header Section이 표시할 상인 프로필과 평판 데이터다.
 * 실제 상인 Actor를 참조하지 않아 상점 UI가 게임 시스템과 느슨하게 연결된다.
 */
USTRUCT(BlueprintType)
struct OUTBREAK_API FShopVendorViewData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Vendor")
	FName VendorId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Vendor")
	FText VendorName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Vendor")
	FText VendorRole;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Vendor")
	FText VendorDescription;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Vendor")
	FText ReputationName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Vendor")
	float ReputationValue = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Vendor")
	float ReputationMaxValue = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Vendor")
	TSoftObjectPtr<UTexture2D> PortraitTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Vendor")
	FSlateBrush PortraitBrush;
};

/**
 * Header Section의 화폐 ListView에 전달되는 지갑 스냅샷이다.
 * 갱신은 명시적인 ApplyWalletData 호출로만 일어난다.
 */
USTRUCT(BlueprintType)
struct OUTBREAK_API FShopWalletViewData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Wallet")
	TArray<FShopCurrencyViewData> Currencies;
};

/**
 * Footer 조작 힌트에 표시할 액션 이름과 키 이름 데이터다.
 * 실제 입력 매핑이나 Enhanced Input 객체는 보관하지 않는다.
 */
USTRUCT(BlueprintType)
struct OUTBREAK_API FShopKeyHintViewData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|KeyHint")
	FText ActionText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|KeyHint")
	FText KeyText;
};

/**
 * 상점 상단 탭 하나를 표시하기 위한 데이터다.
 * 탭 선택 후 다른 Section을 직접 바꾸지 않고 ShopWindow가 중계한다.
 */
USTRUCT(BlueprintType)
struct OUTBREAK_API FShopTabViewData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Tab")
	FName TabId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Tab")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Tab")
	int32 SortOrder = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Tab")
	bool bEnabled = true;
};

/**
 * 좌측 Category Section에 표시되는 분류 데이터다.
 * 아이템 목록 요청은 이 데이터가 아니라 CategoryId 기반 Command로 수행된다.
 */
USTRUCT(BlueprintType)
struct OUTBREAK_API FShopCategoryViewData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Category")
	FName CategoryId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Category")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Category")
	int32 ItemCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Category")
	bool bEnabled = true;
};

/**
 * 상세 정보에 표시되는 단일 아이템 스탯 행 데이터다.
 * 수치 비교나 색상 결정은 게임 시스템에서 계산해 표시값으로 주입한다.
 */
USTRUCT(BlueprintType)
struct OUTBREAK_API FShopItemStatViewData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Item")
	FName StatId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Item")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Item")
	FText ValueText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Item")
	FText DeltaText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Item")
	bool bPositiveDelta = false;
};

/**
 * 중앙 Item List Section에 표시되는 아이템 요약 데이터다.
 * 실제 아이템 객체나 데이터 테이블 Row 포인터를 들고 있지 않는다.
 */
USTRUCT(BlueprintType)
struct OUTBREAK_API FShopItemSummaryViewData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Item")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Item")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Item")
	FText ItemTypeText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Item")
	FText RarityText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Item")
	FText PriceText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Item")
	FName CurrencyId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Item")
	int32 OwnedQuantity = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Item")
	bool bInStock = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Item")
	bool bCanPurchase = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Item")
	FText UnavailableReason;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Item")
	TSoftObjectPtr<UTexture2D> ThumbnailTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Item")
	FSlateBrush ThumbnailBrush;
};

/**
 * Action Section이 버튼 가능 여부와 수량 상태를 표시하기 위한 데이터다.
 * 구매 실행은 이 구조체가 아니라 FShopPurchaseRequest로 ShopWindow에 요청한다.
 */
USTRUCT(BlueprintType)
struct OUTBREAK_API FShopActionState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Action")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Action")
	int32 Quantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Action")
	int32 MaximumQuantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Action")
	bool bCanBuy = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Action")
	bool bCanBarter = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Action")
	bool bHasStock = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Action")
	FText UnavailableReason;
};

/**
 * 우측 Item Detail Section이 표시할 선택 아이템 상세 데이터다.
 * 선택이 사라지면 ShopWindow가 ClearItemDetail을 호출해 이전 데이터 잔상을 제거한다.
 */
USTRUCT(BlueprintType)
struct OUTBREAK_API FShopItemDetailViewData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Item")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Item")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Item")
	FText ItemTypeText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Item")
	FText RarityText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Item")
	FText DescriptionText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Item")
	FText OwnedQuantityText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Item")
	FText PriceText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Item")
	FName CurrencyId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Item")
	TSoftObjectPtr<UTexture2D> PreviewTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Item")
	FSlateBrush PreviewBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Item")
	TSoftObjectPtr<UTexture2D> CurrencyIconTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Item")
	FSlateBrush CurrencyIconBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Item")
	TArray<FShopItemStatViewData> Stats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Item")
	FShopActionState ActionState;
};

/**
 * Gateway나 게임 시스템에서 UI로 전달하는 오류 표시 데이터다.
 * 복구 가능 여부는 WBP가 Retry 버튼 등을 표시할지 판단하는 데 사용할 수 있다.
 */
USTRUCT(BlueprintType)
struct OUTBREAK_API FShopErrorViewData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Error")
	FName ErrorCode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Error")
	FText Message;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Error")
	bool bRecoverable = true;
};

/**
 * 거래 요청 완료 후 UI를 갱신하기 위한 결과 스냅샷이다.
 * 성공 여부와 갱신된 지갑/목록/상세 정보를 한 번에 전달할 수 있다.
 */
USTRUCT(BlueprintType)
struct OUTBREAK_API FShopTransactionResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Transaction")
	FGuid RequestId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Transaction")
	bool bSuccess = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Transaction")
	FText Message;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Transaction")
	FShopWalletViewData UpdatedWallet;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Transaction")
	TArray<FShopItemSummaryViewData> UpdatedVisibleItems;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Transaction")
	bool bHasUpdatedItemDetail = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Transaction")
	FShopItemDetailViewData UpdatedItemDetail;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Transaction")
	FShopErrorViewData Error;
};

/**
 * 상점 초기 진입 시 외부 게임 시스템이 UI에 넘기는 최소 식별자다.
 * 실제 초기 데이터는 Gateway나 ApplyShopViewState를 통해 주입한다.
 */
USTRUCT(BlueprintType)
struct OUTBREAK_API FShopInitializationData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Initialization")
	FName VendorId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Initialization")
	FName InitialTabId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Initialization")
	FName InitialCategoryId;
};

/**
 * ShopWindow가 한 번에 적용할 수 있는 전체 상점 화면 상태다.
 * 비동기 응답이 도착했을 때 이 스냅샷을 적용하면 각 Section이 같은 기준 상태를 공유한다.
 */
USTRUCT(BlueprintType)
struct OUTBREAK_API FShopViewState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|State")
	FShopVendorViewData Vendor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|State")
	FShopWalletViewData Wallet;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|State")
	TArray<FShopTabViewData> Tabs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|State")
	TArray<FShopCategoryViewData> Categories;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|State")
	TArray<FShopItemSummaryViewData> VisibleItems;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|State")
	bool bHasSelectedItem = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|State")
	FShopItemDetailViewData SelectedItem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|State")
	FName CurrentTabId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|State")
	FName CurrentCategoryId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|State")
	FName CurrentItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|State")
	FName CurrentSortId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|State")
	bool bBusy = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|State")
	bool bCanInteract = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|State")
	FShopErrorViewData Error;
};
