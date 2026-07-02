// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "ShopCurrencyEntryWidget.generated.h"

class UImage;
class UTextBlock;

/**
 * Header 지갑 ListView의 단일 화폐 Entry Widget이다.
 * 화폐 증감은 ViewData 재주입으로만 표시되며 인벤토리 시스템을 직접 참조하지 않는다.
 */
UCLASS()
class OUTBREAK_API UShopCurrencyEntryWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	/** ListView 재사용 시 이전 화폐 이름, 금액, 아이콘을 제거한다. */
	void ResetEntry();

	/** 화폐 이름을 표시하는 필수 TextBlock이다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CurrencyNameText;

	/** 화폐 보유량을 표시하는 필수 TextBlock이다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CurrencyAmountText;

	/** 화폐 아이콘을 표시하는 선택 Image다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> CurrencyIconImage;
};
