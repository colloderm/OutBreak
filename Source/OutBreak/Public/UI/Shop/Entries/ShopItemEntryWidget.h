// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "ShopItemEntryWidget.generated.h"

class UImage;
class UTextBlock;
class UWidget;

/**
 * 중앙 아이템 ListView의 단일 행 Widget이다.
 * 이 Widget은 상세 정보나 구매를 직접 요청하지 않고 표시 전용 Entry로 동작한다.
 */
UCLASS()
class OUTBREAK_API UShopItemEntryWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	/** ListView가 Entry를 재사용하기 전에 이전 아이템 텍스트, 이미지, 선택 표시를 제거한다. */
	void ResetEntry();

	/**
	 * 아이템 이름을 표시하는 필수 TextBlock이다.
	 * WBP Component 이름은 ItemNameText여야 한다.
	 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ItemNameText;

	/**
	 * 아이템 가격을 표시하는 필수 TextBlock이다.
	 * WBP Component 이름은 PriceText여야 한다.
	 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PriceText;

	/**
	 * 아이템 타입을 표시하는 선택 TextBlock이다.
	 * 리스트 밀도가 높은 WBP에서는 생략할 수 있다.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ItemTypeText;

	/**
	 * 희귀도 문구를 표시하는 선택 TextBlock이다.
	 * 색상이나 스타일은 WBP에서 조정한다.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RarityText;

	/**
	 * 보유 수량을 표시하는 선택 TextBlock이다.
	 * 보유 수량을 표시하지 않는 상점 디자인에서는 생략할 수 있다.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> OwnedQuantityText;

	/**
	 * 구매 불가 사유를 표시하는 선택 TextBlock이다.
	 * 빈 문구이면 자동으로 Collapsed 처리된다.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> UnavailableReasonText;

	/**
	 * 아이템 썸네일을 표시하는 선택 Image다.
	 * 이미지 없이 텍스트 리스트로 구성하는 WBP에서는 생략할 수 있다.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> ThumbnailImage;

	/**
	 * 선택된 아이템을 강조하는 선택 Component다.
	 * 선택 강조 방식이 다른 WBP에서는 생략할 수 있다.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SelectedIndicator;
};
