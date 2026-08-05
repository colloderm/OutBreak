// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "OBLootEntryWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;

// 클릭된 항목. 무엇을 할지는 소유 위젯이 정한다(가져가기 / 반입 / 반환).
DECLARE_DELEGATE_TwoParams(FOBOnLootEntryClicked, const FGameplayTag& /*ItemTag*/, int32 /*Count*/);

/**
왜 존재하는가?
 - 아이템 목록의 한 줄. 루팅 창 / 결과창 / 반입 선택이 전부 이걸 쓴다.
무엇을 저장하는가?
 - 태그와 수량만. 아이템 스펙 포인터는 들지 않는다(표 Reimport 시 무효화된다).
 */
UCLASS()
class OUTBREAK_API UOBLootEntryWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetEntry(const FGameplayTag& InItemTag, int32 InCount);
	
	// 바인딩하지 않으면 클릭해도 아무 일도 없다(결과창처럼 읽기 전용인 경우).
	FOBOnLootEntryClicked OnEntryClicked;

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleTakeClicked();

	// 위젯에 같은 이름이 있으면 자동 연결. 없어도 크래시하지 않는다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> IMG_Icon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TXT_Name;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TXT_Count;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BTN_Take;

private:
	FGameplayTag ItemTag;
	int32 Count = 0;
	
};
