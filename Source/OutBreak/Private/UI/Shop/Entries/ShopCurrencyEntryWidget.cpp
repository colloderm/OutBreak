// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Shop/Entries/ShopCurrencyEntryWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "UI/Shop/EntryData/ShopCurrencyEntryData.h"
#include "UI/Shop/ShopWidgetTypes.h"

void UShopCurrencyEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	ResetEntry();

	const UShopCurrencyEntryData* EntryData = Cast<UShopCurrencyEntryData>(ListItemObject);
	if (!EntryData)
	{
		UE_LOG(LogShopWidget, Warning, TEXT("ShopCurrencyEntryWidget received invalid entry data."));
		return;
	}

	const FShopCurrencyViewData& ViewData = EntryData->GetViewData();
	if (CurrencyNameText)
	{
		CurrencyNameText->SetText(ViewData.DisplayName);
	}
	if (CurrencyAmountText)
	{
		CurrencyAmountText->SetText(FText::AsNumber(ViewData.Amount));
	}
	if (CurrencyIconImage)
	{
		if (UTexture2D* LoadedTexture = ViewData.IconTexture.Get())
		{
			CurrencyIconImage->SetBrushFromTexture(LoadedTexture);
		}
		else
		{
			CurrencyIconImage->SetBrush(ViewData.IconBrush);
		}
	}
}

void UShopCurrencyEntryWidget::ResetEntry()
{
	if (CurrencyNameText)
	{
		CurrencyNameText->SetText(FText::GetEmpty());
	}
	if (CurrencyAmountText)
	{
		CurrencyAmountText->SetText(FText::GetEmpty());
	}
	if (CurrencyIconImage)
	{
		CurrencyIconImage->SetBrush(FSlateBrush());
	}
}
