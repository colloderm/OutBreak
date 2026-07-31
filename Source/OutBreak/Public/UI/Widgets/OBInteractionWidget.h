// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"
#include "OBInteractionWidget.generated.h"

class UButton;

UCLASS()
class OUTBREAK_API UOBInteractionWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION()
	void HandleCloseClicked();

	// 커서·입력모드·이동잠금 원복은 컨트롤러가 한다. 위젯이 직접 RemoveFromParent 하지 말 것.
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	virtual void RequestClose();

protected:
	// 위젯에 같은 이름의 버튼이 있으면 자동 연결. 없어도 무방.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BTN_Close;

	// PIE에서 ESC가 플레이 중지와 겹치면 다른 키로 바꿀 것.
	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	FKey CloseKey = EKeys::Escape;
	
};
