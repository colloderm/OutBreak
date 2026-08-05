// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Item/Data/OBItemTypes.h"
#include "UI/Widgets/OBInteractionWidget.h"
#include "OBCarrySelectWidget.generated.h"

class UPanelWidget;
class UTextBlock;
class UOBLootEntryWidget;
class UOBLoadoutSubsystem;

/**
왜 존재하는가?
 - 탐사에 들고 갈 물건을 창고에서 고른다.
무엇을 저장하는가?
 - 아무것도. 창고/반입 목록은 LoadoutSubsystem이 소유한다.
멀티플레이 역할?
 - 순수 로컬. 서버로는 입장 시 PlayerController가 push한다.
 */
UCLASS()
class OUTBREAK_API UOBCarrySelectWidget : public UOBInteractionWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	void Rebuild();
	void FillList(UPanelWidget* Box, const TArray<FOBItemStack>& Items, bool bFromStash);

	// 스택 통째로 옮긴다. 부분 수량 선택은 아직 없다.
	void HandleStashClicked(const FGameplayTag& ItemTag, int32 Count);
	void HandleCarryClicked(const FGameplayTag& ItemTag, int32 Count);

	UOBLoadoutSubsystem* GetLoadout() const;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Carry")
	TSubclassOf<UOBLootEntryWidget> EntryWidgetClass;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> Box_Stash;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> Box_Carry;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TXT_CarryEmpty;
	
};