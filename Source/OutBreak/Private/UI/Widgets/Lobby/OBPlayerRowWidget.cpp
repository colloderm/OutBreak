// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/Lobby/OBPlayerRowWidget.h"

#include "Components/TextBlock.h"
#include "Components/Image.h"

void UOBPlayerRowWidget::Setup(const FString& PlayerName, bool bReady, bool bIsMe)
{
	if (NameText) 
		NameText->SetText(FText::FromString(PlayerName));
	
	if (ReadyCheck) 
		ReadyCheck->SetVisibility(bReady ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	// bIsMe 하이라이트는 BP 스타일로
}

void UOBPlayerRowWidget::SetEmpty()
{
	if (NameText) 
		NameText->SetText(FText::FromString(TEXT("대기 중…")));
	
	if (ReadyCheck) 
		ReadyCheck->SetVisibility(ESlateVisibility::Hidden);
	
	if (CrownImage) 
		CrownImage->SetVisibility(ESlateVisibility::Hidden);
}