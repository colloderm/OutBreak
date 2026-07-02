// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "ShopTabEntryWidget.generated.h"

class UTextBlock;
class UWidget;

/**
 * 상점 탭 ListView의 단일 행 Widget이다.
 * Entry Data를 표시만 하며 탭 변경 요청은 ListView를 소유한 Tab Section이 처리한다.
 */
UCLASS()
class OUTBREAK_API UShopTabEntryWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	/** 재사용되는 ListView Entry에 이전 탭 이름이나 선택 표시가 남지 않도록 초기화한다. */
	void ResetEntry();

	/**
	 * 탭 이름을 표시하는 필수 TextBlock이다.
	 * WBP에서는 Component 이름을 TabLabelText로 정확히 맞춰야 한다.
	 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TabLabelText;

	/**
	 * 선택된 탭을 강조하는 선택 Component다.
	 * 선택 표시를 별도로 두지 않는 WBP에서는 생략할 수 있다.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SelectedIndicator;
};
