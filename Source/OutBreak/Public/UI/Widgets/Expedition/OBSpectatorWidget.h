// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OBSpectatorWidget.generated.h"

class UTextBlock;
class UButton;
/**
 * 
 */
UCLASS()
class OUTBREAK_API UOBSpectatorWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	
	UFUNCTION()
	void HandlePrevClicked();
	
	UFUNCTION()
	void HandleNextClicked();
	
	UFUNCTION()
	void HandleLeaveClicked();
	
	// 시점 전환은 서버 왕복이라 즉시 반영되지 않는다. 짧은 주기로 라벨만 폴링.
	void RefreshTargetLabel();
	
protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BTN_Prev;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BTN_Next;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BTN_Leave;      // "탐사 포기" — 결과창을 기다리지 않고 홈으로

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TXT_Target;  // "관전 중: <이름>"

private:
	FTimerHandle LabelRefreshTimer;
	
};
