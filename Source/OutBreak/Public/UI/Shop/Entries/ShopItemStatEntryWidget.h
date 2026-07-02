// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "ShopItemStatEntryWidget.generated.h"

class UTextBlock;

/**
 * 아이템 상세 스탯 ListView의 단일 행 Widget이다.
 * 수치 계산은 외부 시스템이 끝낸 뒤 표시 문자열만 이 Entry로 전달한다.
 */
UCLASS()
class OUTBREAK_API UShopItemStatEntryWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	/** 재사용 Entry에서 이전 스탯 이름과 증감 표시를 제거한다. */
	void ResetEntry();

	/** 스탯 이름을 표시하는 필수 TextBlock이다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> StatNameText;

	/** 스탯 값을 표시하는 필수 TextBlock이다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> StatValueText;

	/** 비교 증감값을 표시하는 선택 TextBlock이다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DeltaText;
};
