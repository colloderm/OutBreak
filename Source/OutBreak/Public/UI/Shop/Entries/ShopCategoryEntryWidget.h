// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "ShopCategoryEntryWidget.generated.h"

class UTextBlock;
class UWidget;

/**
 * 상점 카테고리 ListView의 단일 행 Widget이다.
 * 카테고리 클릭은 Category Section의 ListView 이벤트로 ShopWindow에 전달된다.
 */
UCLASS()
class OUTBREAK_API UShopCategoryEntryWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	/** ListView 재사용 시 이전 카테고리 표시와 선택 상태를 제거한다. */
	void ResetEntry();

	/**
	 * 카테고리 이름을 표시하는 필수 TextBlock이다.
	 * WBP Component 이름은 CategoryNameText여야 한다.
	 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CategoryNameText;

	/**
	 * 카테고리별 아이템 개수를 표시하는 선택 TextBlock이다.
	 * 수량 표시가 없는 디자인이면 WBP에서 생략할 수 있다.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ItemCountText;

	/**
	 * 현재 선택 카테고리를 강조하는 선택 Component다.
	 * 선택 강조를 다른 방식으로 처리하는 WBP에서는 생략할 수 있다.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SelectedIndicator;
};
