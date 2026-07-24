// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OBHomeHubWidget.generated.h"

class UButton;
class UWidgetSwitcher;
class UWidget;

UCLASS()
class OUTBREAK_API UOBHomeHubWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;

	// UIOnly에서 게임 InputAction은 안 오므로, 위젯 키 이벤트로 ESC를 처리.
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION() 
	void OnExpeditionClicked();
	
	UFUNCTION() 
	void OnLoadoutClicked();
	
	UFUNCTION() 
	void OnBoardClicked();
	
	UFUNCTION() 
	void OnCloseClicked();

	void ShowPanel(UWidget* Panel);
	
	UFUNCTION()
	void OnPartyClicked();
	
protected:
	// 좌측 메뉴 버튼(상시 표시).
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BtnExpedition;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BtnLoadout;
	
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BtnBoard;
	
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BtnClose;
	
	UPROPERTY(meta = (BindWidgetOptional)) 
	TObjectPtr<UButton> BtnParty;

	// 우측 콘텐츠.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> ContentSwitcher;

	// 스위처에 들어갈 패널들(디자이너에서 스위처 슬롯으로 배치).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ExpeditionPanel;   // M4 지역선택(지금은 스텁 텍스트)
	
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LoadoutPanel;      // 기존 WBP_Lobby 임베드
	
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> BoardPanel;        // 껍데기
	
	UPROPERTY(meta = (BindWidgetOptional)) 
	TObjectPtr<UWidget> PartyPanel;
};
