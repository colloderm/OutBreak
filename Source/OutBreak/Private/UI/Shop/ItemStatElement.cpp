// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Shop/ItemStatElement.h"

#include "Components/TextBlock.h"

void UItemStatElement::NativeConstruct()
{
	Super::NativeConstruct();
}

void UItemStatElement::SetStatData(const FShopItemStatViewData& InData)
{
	if (TXT_StatLabel)
	{
		TXT_StatLabel->SetText(InData.DisplayName);
	}

	if (TXT_StatValue)
	{
		TXT_StatValue->SetText(InData.DisplayValue);
	}
}

void UItemStatElement::ClearStatData()
{
	if (TXT_StatLabel)
	{
		TXT_StatLabel->SetText(FText::GetEmpty());
	}

	if (TXT_StatValue)
	{
		TXT_StatValue->SetText(FText::GetEmpty());
	}
}
