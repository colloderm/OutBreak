// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Shop/ShopWidgetCommands.h"
#include "UI/Shop/ShopWidgetViewData.h"
#include "ShopActionSectionWidget.generated.h"

class UButton;
class UProgressBar;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShopActionPurchaseRequestedDelegate, FShopPurchaseRequest, Request);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShopActionBarterRequestedDelegate, FShopBarterRequest, Request);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShopActionQuantityChangedDelegate, int32, Quantity);

/**
 * 구매/교환 버튼과 수량 입력을 담당하는 Action Section이다.
 * 화폐 차감, 인벤토리 추가, 서버 요청은 직접 처리하지 않고 Command Delegate로 ShopWindow에 전달한다.
 */
UCLASS()
class OUTBREAK_API UShopActionSectionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 선택 아이템의 구매 가능 상태와 수량 범위를 적용한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Action")
	void ApplyActionState(const FShopActionState& ActionState);

	/** 현재 Action Section이 대상으로 삼는 아이템 식별자를 설정한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Action")
	void SetSelectedItemId(FName ItemId);

	/** 구매 수량을 설정하고 버튼/텍스트 상태를 갱신한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Action")
	void SetQuantity(int32 InQuantity);

	/** 구매 가능한 최대 수량을 설정한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Action")
	void SetMaximumQuantity(int32 InMaximumQuantity);

	/** 구매 버튼 가능 여부를 설정한다. 실제 버튼 활성화는 상호작용 잠금 상태도 함께 고려한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Action")
	void SetBuyEnabled(bool bEnabled);

	/** 교환 버튼 가능 여부를 설정한다. BarterButton이 없는 WBP에서는 상태만 저장한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Action")
	void SetBarterEnabled(bool bEnabled);

	/** 선택 아이템이 사라질 때 수량과 버튼 상태를 안전하게 초기화한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Action")
	void ResetActionState();

	/** 상점 입력 잠금 상태에 맞춰 버튼 가능 여부를 갱신한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Action")
	void SetInteractionEnabled(bool bEnabled);

	UPROPERTY(BlueprintAssignable, Category = "Shop|Action")
	FShopActionPurchaseRequestedDelegate OnPurchaseRequested;

	UPROPERTY(BlueprintAssignable, Category = "Shop|Action")
	FShopActionBarterRequestedDelegate OnBarterRequested;

	UPROPERTY(BlueprintAssignable, Category = "Shop|Action")
	FShopActionQuantityChangedDelegate OnQuantityChanged;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	/**
	 * 구매 요청을 발생시키는 필수 Button이다.
	 * WBP Component 이름은 BuyButton이어야 한다.
	 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BuyButton;

	/** 교환 요청 버튼이다. 교환이 없는 상점 WBP에서는 생략할 수 있다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BarterButton;

	/** 수량 증가 버튼이다. 단일 구매만 지원하는 WBP에서는 생략할 수 있다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> IncreaseQuantityButton;

	/** 수량 감소 버튼이다. 단일 구매만 지원하는 WBP에서는 생략할 수 있다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> DecreaseQuantityButton;

	/** 현재 구매 수량을 표시하는 필수 TextBlock이다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> QuantityText;

	/** 최대 구매 수량을 표시하는 선택 TextBlock이다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MaximumQuantityText;

	/** 홀드 구매 같은 입력 진행률을 표시하는 선택 ProgressBar다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HoldProgressBar;

	/** 구매/교환 불가 사유를 표시하는 선택 TextBlock이다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> UnavailableReasonText;

private:
	UFUNCTION()
	void HandleBuyClicked();

	UFUNCTION()
	void HandleBarterClicked();

	UFUNCTION()
	void HandleIncreaseQuantityClicked();

	UFUNCTION()
	void HandleDecreaseQuantityClicked();

	void ApplyQuantity(int32 InQuantity, bool bBroadcastChange);
	void RefreshButtonState();
	void RefreshQuantityText();

	UPROPERTY()
	FName SelectedItemId;

	int32 Quantity = 1;
	int32 MaximumQuantity = 1;
	bool bBuyEnabled = false;
	bool bBarterEnabled = false;
	bool bHasStock = true;
	bool bInteractionEnabled = true;
};
