// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Shop/ShopWidgetViewData.h"
#include "ShopItemDetailSectionWidget.generated.h"

class UImage;
class UListView;
class UTextBlock;
class UWidget;
class UShopActionSectionWidget;
class UShopItemStatEntryData;

/**
 * 상점 우측 아이템 상세 Section이다.
 * 선택 아이템의 설명, 이미지, 가격, 스탯을 표시하며 구매 실행은 내부 Action Section Delegate로 ShopWindow에 올린다.
 */
UCLASS()
class OUTBREAK_API UShopItemDetailSectionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 선택 아이템 상세 정보를 화면에 적용하고 Empty 상태를 해제한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|ItemDetail")
	void ApplyItemDetail(const FShopItemDetailViewData& ItemDetail);

	/** 구매 가능 여부나 수량만 바뀐 경우 내부 Action Section에 상태를 적용한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|ItemDetail")
	void ApplyActionState(const FShopActionState& ActionState);

	/** 선택 아이템이 없을 때 이전 상세 정보가 남지 않도록 모든 표시값을 비운다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|ItemDetail")
	void ClearItemDetail();

	/** 선택 없음 안내 Widget 표시를 켜거나 끈다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|ItemDetail")
	void SetEmptyState(bool bIsEmpty);

	/** 상점 입력 잠금 상태에 맞춰 상세 영역과 Action Section의 입력 가능 여부를 변경한다. */
	UFUNCTION(BlueprintCallable, Category = "Shop|ItemDetail")
	void SetInteractionEnabled(bool bEnabled);

	/** ShopWindow가 Action Section Delegate를 중앙에서 바인딩하기 위한 읽기 전용 접근자다. */
	UFUNCTION(BlueprintPure, Category = "Shop|ItemDetail")
	UShopActionSectionWidget* GetActionSection() const { return ActionSection; }

protected:
	/** 선택 아이템 이름을 표시하는 필수 TextBlock이다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ItemNameText;

	/** 선택 아이템 타입을 표시하는 필수 TextBlock이다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ItemTypeText;

	/** 선택 아이템 희귀도를 표시하는 필수 TextBlock이다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> RarityText;

	/** 선택 아이템 설명을 표시하는 필수 TextBlock이다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DescriptionText;

	/** 선택 아이템 미리보기 이미지를 표시하는 필수 Image다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> ItemPreviewImage;

	/** 보유 수량을 표시하는 필수 TextBlock이다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> OwnedQuantityText;

	/** 가격 문구를 표시하는 필수 TextBlock이다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PriceText;

	/** 가격 화폐 아이콘을 표시하는 필수 Image다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> CurrencyIconImage;

	/** 스탯 Entry를 표시하는 필수 ListView다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UListView> StatListView;

	/** 구매/교환 입력을 담당하는 필수 Action Section이다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShopActionSectionWidget> ActionSection;

	/** 선택 아이템이 없을 때 표시하는 선택 Widget이다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> EmptyStateWidget;

private:
	UPROPERTY()
	TArray<TObjectPtr<UShopItemStatEntryData>> StatEntryDataObjects;
};
