// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Shop/Sections/ShopHeaderSectionWidget.h"

#include "Components/Image.h"
#include "Components/ListView.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "UI/Shop/EntryData/ShopCurrencyEntryData.h"

void UShopHeaderSectionWidget::ApplyVendorData(const FShopVendorViewData& VendorData)
{
	if (VendorPortraitImage)
	{
		if (UTexture2D* LoadedTexture = VendorData.PortraitTexture.Get())
		{
			VendorPortraitImage->SetBrushFromTexture(LoadedTexture);
		}
		else
		{
			VendorPortraitImage->SetBrush(VendorData.PortraitBrush);
		}
	}
	if (VendorRoleText)
	{
		VendorRoleText->SetText(VendorData.VendorRole);
	}
	if (VendorNameText)
	{
		VendorNameText->SetText(VendorData.VendorName);
	}
	if (VendorDescriptionText)
	{
		VendorDescriptionText->SetText(VendorData.VendorDescription);
	}
	if (ReputationNameText)
	{
		ReputationNameText->SetText(VendorData.ReputationName);
	}
	if (ReputationValueText)
	{
		ReputationValueText->SetText(FText::AsNumber(FMath::RoundToInt(VendorData.ReputationValue)));
	}
	if (ReputationProgressBar)
	{
		const float MaxValue = FMath::Max(VendorData.ReputationMaxValue, 0.01f);
		ReputationProgressBar->SetPercent(FMath::Clamp(VendorData.ReputationValue / MaxValue, 0.0f, 1.0f));
	}
}

void UShopHeaderSectionWidget::ApplyWalletData(const FShopWalletViewData& WalletData)
{
	ClearWalletData();

	if (!CurrencyListView)
	{
		return;
	}

	TArray<UObject*> ListItems;
	ListItems.Reserve(WalletData.Currencies.Num());
	CurrencyEntryDataObjects.Reserve(WalletData.Currencies.Num());

	for (const FShopCurrencyViewData& Currency : WalletData.Currencies)
	{
		UShopCurrencyEntryData* EntryData = NewObject<UShopCurrencyEntryData>(this);
		EntryData->Initialize(Currency);
		CurrencyEntryDataObjects.Add(EntryData);
		ListItems.Add(EntryData);
	}

	CurrencyListView->SetListItems(ListItems);
}

void UShopHeaderSectionWidget::ClearVendorData()
{
	if (VendorPortraitImage)
	{
		VendorPortraitImage->SetBrush(FSlateBrush());
	}
	if (VendorRoleText)
	{
		VendorRoleText->SetText(FText::GetEmpty());
	}
	if (VendorNameText)
	{
		VendorNameText->SetText(FText::GetEmpty());
	}
	if (VendorDescriptionText)
	{
		VendorDescriptionText->SetText(FText::GetEmpty());
	}
	if (ReputationNameText)
	{
		ReputationNameText->SetText(FText::GetEmpty());
	}
	if (ReputationValueText)
	{
		ReputationValueText->SetText(FText::GetEmpty());
	}
	if (ReputationProgressBar)
	{
		ReputationProgressBar->SetPercent(0.0f);
	}
}

void UShopHeaderSectionWidget::ClearWalletData()
{
	CurrencyEntryDataObjects.Reset();
	if (CurrencyListView)
	{
		CurrencyListView->ClearListItems();
	}
}

void UShopHeaderSectionWidget::SetInteractionEnabled(bool bEnabled)
{
	SetIsEnabled(bEnabled);
	if (CurrencyListView)
	{
		CurrencyListView->SetIsEnabled(bEnabled);
	}
}
