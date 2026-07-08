// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "KeyBindableBtn.generated.h"


class UTextBlock;

/**
 * 
 */
UCLASS()
class OUTBREAK_API UKeyBindableBtn : public UUserWidget
{
	GENERATED_BODY()
	
	
public:
	/* 바인딩 시킨 키를 표시 해주는 텍스트 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_BindedKey;
	
	/* 이 버튼이 동작한 액션 정보 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_Action;
	
	
	virtual void NativeConstruct() override;
};
