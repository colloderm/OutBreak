// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/HUD/OBInteractPromptWidget.h"

#include "Components/TextBlock.h"

void UOBInteractPromptWidget::SetPromptText(const FText& InText)
{
	if (TXT_Prompt)
	{
		TXT_Prompt->SetText(InText);
	}
}