// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/Home/OBHomeHubWidget.h"

#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "Player/Controller/OBPlayerController.h"

void UOBHomeHubWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (BtnExpedition) 
		BtnExpedition->OnClicked.AddDynamic(this, &UOBHomeHubWidget::OnExpeditionClicked);
	
	if (BtnLoadout)    
		BtnLoadout->OnClicked.AddDynamic(this, &UOBHomeHubWidget::OnLoadoutClicked);
	
	if (BtnBoard)      
		BtnBoard->OnClicked.AddDynamic(this, &UOBHomeHubWidget::OnBoardClicked);
	
	if (BtnClose)      
		BtnClose->OnClicked.AddDynamic(this, &UOBHomeHubWidget::OnCloseClicked);
	
	if (BtnParty) 
		BtnParty->OnClicked.AddDynamic(this, &UOBHomeHubWidget::OnPartyClicked);

	// 처음엔 아무 패널도 안 보이게(버튼 클릭 전 빈 화면).
	if (ContentSwitcher)
		ContentSwitcher->SetVisibility(ESlateVisibility::Collapsed);

	// ESC 등 키 입력을 받기 위해 포커스 확보(위젯이 Focusable해야 유효 — PC에서 설정함).
	SetKeyboardFocus();
}

void UOBHomeHubWidget::ShowPanel(UWidget* Panel)
{
	if (!ContentSwitcher || !Panel) return;

	ContentSwitcher->SetVisibility(ESlateVisibility::Visible); // 첫 클릭 시 등장
	ContentSwitcher->SetActiveWidget(Panel);
}

void UOBHomeHubWidget::OnPartyClicked()
{
	ShowPanel(PartyPanel);
}

void UOBHomeHubWidget::OnExpeditionClicked()
{
	ShowPanel(ExpeditionPanel);
}

void UOBHomeHubWidget::OnLoadoutClicked()
{
	ShowPanel(LoadoutPanel);
}

void UOBHomeHubWidget::OnBoardClicked()
{
	ShowPanel(BoardPanel);
}

void UOBHomeHubWidget::OnCloseClicked()
{
	if (AOBPlayerController* PC = GetOwningPlayer<AOBPlayerController>())
		PC->CloseInteractionWidget();
}

FReply UOBHomeHubWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (AOBPlayerController* PC = GetOwningPlayer<AOBPlayerController>())
			PC->CloseInteractionWidget();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
