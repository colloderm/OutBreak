// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Item/Data/OBItemTypes.h"
#include "OBExpeditionResultWidget.generated.h"

class UPanelWidget;
class UTextBlock;
class UOBLootEntryWidget;

/**
왜 존재하는가?
 - 세션 결과창에 "이번에 가져 나온 것"을 보여준다.
멀티플레이 역할?
 - 순수 표시. 창고 반영은 이미 Client RPC에서 끝났다.
 */
UCLASS()
class OUTBREAK_API UOBExpeditionResultWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// 컨트롤러가 결과창을 띄운 직후 밀어 넣는다.
	void SetHaul(const TArray<FOBItemStack>& InHaul);

protected:
	// 루팅 창의 한 줄 위젯을 그대로 재사용한다.
	UPROPERTY(EditDefaultsOnly, Category = "Result")
	TSubclassOf<UOBLootEntryWidget> EntryWidgetClass;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> Box_Haul;

	// 빈손으로 탈출했을 때만 보인다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TXT_HaulEmpty;
};
