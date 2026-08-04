// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OBInteractPromptWidget.generated.h"

class UTextBlock;

/**
 왜 존재하는가?
 - 상호작용 액터 머리 위에 뜨는 안내 문구. 액터의 WidgetComponent가 띄운다.
 무엇을 저장하는가?
 - 아무것도. 액터가 문구를 밀어 넣는다.
   (월드 위젯은 소유 플레이어가 보장되지 않아서 위젯이 컨트롤러를 뒤지면 안 된다.)
 */
UCLASS()
class OUTBREAK_API UOBInteractPromptWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetPromptText(const FText& InText);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_Prompt;
};
