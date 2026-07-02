// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Shop/ShopWidgetViewData.h"
#include "ShopFooterSectionWidget.generated.h"

class UButton;
class UListView;
class UShopKeyHintEntryData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FShopFooterRequestedDelegate);

/**
 * 상점 하단 Footer Section이다.
 * 닫기, 뒤로가기, 비교, 도움말 같은 전역 입력 요청을 ShopWindow로 올린다.
 */
UCLASS()
class OUTBREAK_API UShopFooterSectionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Footer 키 힌트 표시 데이터를 ListView Entry Data로 변환해 적용한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Footer")
	void ApplyKeyHints(const TArray<FShopKeyHintViewData>& KeyHints);

	/** 키 힌트 목록을 비운다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Footer")
	void ClearKeyHints();

	/** 상점 입력 잠금 상태에 맞춰 Footer 버튼 가능 여부를 변경한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|Footer")
	void SetInteractionEnabled(bool bEnabled);

	UPROPERTY(BlueprintAssignable, Category = "Shop|Footer")
	FShopFooterRequestedDelegate OnCloseRequested;

	UPROPERTY(BlueprintAssignable, Category = "Shop|Footer")
	FShopFooterRequestedDelegate OnBackRequested;

	UPROPERTY(BlueprintAssignable, Category = "Shop|Footer")
	FShopFooterRequestedDelegate OnCompareRequested;

	UPROPERTY(BlueprintAssignable, Category = "Shop|Footer")
	FShopFooterRequestedDelegate OnHelpRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	/** 상점을 닫는 필수 Button이다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CloseButton;

	/** 이전 화면으로 돌아가는 선택 Button이다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BackButton;

	/** 아이템 비교 요청을 발생시키는 선택 Button이다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CompareButton;

	/** 도움말 요청을 발생시키는 선택 Button이다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> HelpButton;

	/** 조작 힌트를 표시하는 선택 ListView다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UListView> KeyHintListView;

private:
	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleBackClicked();

	UFUNCTION()
	void HandleCompareClicked();

	UFUNCTION()
	void HandleHelpClicked();

	UPROPERTY()
	TArray<TObjectPtr<UShopKeyHintEntryData>> KeyHintEntryDataObjects;
};
