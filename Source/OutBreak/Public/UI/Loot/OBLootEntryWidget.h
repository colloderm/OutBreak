// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "OBLootEntryWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UOBLootWindow;

/**
왜 존재하는가?
 - 컨테이너 목록의 한 줄. 아이콘/이름/수량 + 가져가기 버튼.
무엇을 저장하는가?
 - 태그와 수량만. 아이템 스펙 포인터는 들지 않는다(표 Reimport 시 무효화된다).
 */
UCLASS()
class OUTBREAK_API UOBLootEntryWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetEntry(UOBLootWindow* InOwner, const FGameplayTag& InItemTag, int32 InCount);

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
	UPROPERTY()
	TObjectPtr<UOBLootWindow> Owner;

	FGameplayTag ItemTag;
	int32 Count = 0;
	
};
