// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "OBLootEntryWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
struct FInventoryData;

// 클릭된 항목. 무엇을 할지는 소유 위젯이 정한다(가져가기 / 반입 / 반환).
DECLARE_DELEGATE_TwoParams(FOBOnLootEntryClicked, const FGameplayTag& /*ItemTag*/, int32 /*Count*/);
DECLARE_DELEGATE_TwoParams(FOBOnLootInstanceClicked, const FGuid& /*InstanceId*/, int32 /*Count*/);

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
	void SetItemInstance(const FInventoryData& InItemInstance);
	
	// 바인딩하지 않으면 클릭해도 아무 일도 없다(결과창처럼 읽기 전용인 경우).
	FOBOnLootEntryClicked OnEntryClicked;
	FOBOnLootInstanceClicked OnInstanceClicked;

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleTakeClicked();
	
	// 클릭 시점의 수정키로 수량을 정한다. Shift=절반, Ctrl=1개, 없으면 전부.
	int32 ResolveClickCount() const;

protected:
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
	FGuid InstanceId;
	int32 Count = 0;
	
};
