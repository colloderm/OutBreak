// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Shop/UserCurrency.h"

#include "Components/TextBlock.h"

void UUserCurrency::NativeConstruct()
{
	Super::NativeConstruct();
}

void UUserCurrency::SetCurrencyData(const FUserCurrencyViewData& InData)
{
	if (TXT_ScrapValue)
	{
		TXT_ScrapValue->SetText(FText::AsNumber(InData.Scrap));
	}
}

void UUserCurrency::ClearCurrencyData()
{
	if (TXT_ScrapValue)
	{
		TXT_ScrapValue->SetText(FText::AsNumber(0));
	}
}
