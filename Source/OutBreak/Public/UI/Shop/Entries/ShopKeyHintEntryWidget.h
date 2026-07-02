// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "ShopKeyHintEntryWidget.generated.h"

class UTextBlock;

/**
 * Footer 조작 힌트 ListView의 단일 Entry Widget이다.
 * 입력 장치별 표시는 외부 시스템이 텍스트로 변환해 Entry Data에 주입한다.
 */
UCLASS()
class OUTBREAK_API UShopKeyHintEntryWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	/** 재사용 Entry에서 이전 액션 이름과 키 표시를 제거한다. */
	void ResetEntry();

	/** 수행할 액션 이름을 표시하는 필수 TextBlock이다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ActionText;

	/** 액션에 대응하는 키나 버튼 이름을 표시하는 필수 TextBlock이다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> KeyText;
};
