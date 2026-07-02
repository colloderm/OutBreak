// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Shop/Entries/ShopKeyHintEntryWidget.h"

#include "Components/TextBlock.h"
#include "UI/Shop/EntryData/ShopKeyHintEntryData.h"
#include "UI/Shop/ShopWidgetTypes.h"

void UShopKeyHintEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	ResetEntry();

	const UShopKeyHintEntryData* EntryData = Cast<UShopKeyHintEntryData>(ListItemObject);
	if (!EntryData)
	{
		UE_LOG(LogShopWidget, Warning, TEXT("ShopKeyHintEntryWidget received invalid entry data."));
		return;
	}

	if (ActionText)
	{
		ActionText->SetText(EntryData->GetActionText());
	}
	if (KeyText)
	{
		KeyText->SetText(EntryData->GetKeyText());
	}
}

void UShopKeyHintEntryWidget::ResetEntry()
{
	if (ActionText)
	{
		ActionText->SetText(FText::GetEmpty());
	}
	if (KeyText)
	{
		KeyText->SetText(FText::GetEmpty());
	}
}
