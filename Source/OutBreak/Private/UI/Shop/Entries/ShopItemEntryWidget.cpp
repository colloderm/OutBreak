// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Shop/Entries/ShopItemEntryWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/Texture2D.h"
#include "UI/Shop/EntryData/ShopItemEntryData.h"
#include "UI/Shop/ShopWidgetTypes.h"

void UShopItemEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	ResetEntry();

	const UShopItemEntryData* EntryData = Cast<UShopItemEntryData>(ListItemObject);
	if (!EntryData)
	{
		UE_LOG(LogShopWidget, Warning, TEXT("ShopItemEntryWidget received invalid entry data."));
		return;
	}

	const FShopItemSummaryViewData& ViewData = EntryData->GetViewData();
	if (ItemNameText)
	{
		ItemNameText->SetText(ViewData.DisplayName);
	}
	if (PriceText)
	{
		PriceText->SetText(ViewData.PriceText);
	}
	if (ItemTypeText)
	{
		ItemTypeText->SetText(ViewData.ItemTypeText);
	}
	if (RarityText)
	{
		RarityText->SetText(ViewData.RarityText);
	}
	if (OwnedQuantityText)
	{
		OwnedQuantityText->SetText(FText::AsNumber(ViewData.OwnedQuantity));
	}
	if (UnavailableReasonText)
	{
		const bool bHasReason = !ViewData.UnavailableReason.IsEmpty();
		UnavailableReasonText->SetText(ViewData.UnavailableReason);
		UnavailableReasonText->SetVisibility(bHasReason ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (ThumbnailImage)
	{
		if (UTexture2D* LoadedTexture = ViewData.ThumbnailTexture.Get())
		{
			ThumbnailImage->SetBrushFromTexture(LoadedTexture);
		}
		else
		{
			ThumbnailImage->SetBrush(ViewData.ThumbnailBrush);
		}
	}
	if (SelectedIndicator)
	{
		SelectedIndicator->SetVisibility(EntryData->IsSelected() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	SetIsEnabled(ViewData.bInStock && ViewData.bCanPurchase);
}

void UShopItemEntryWidget::ResetEntry()
{
	if (ItemNameText)
	{
		ItemNameText->SetText(FText::GetEmpty());
	}
	if (PriceText)
	{
		PriceText->SetText(FText::GetEmpty());
	}
	if (ItemTypeText)
	{
		ItemTypeText->SetText(FText::GetEmpty());
	}
	if (RarityText)
	{
		RarityText->SetText(FText::GetEmpty());
	}
	if (OwnedQuantityText)
	{
		OwnedQuantityText->SetText(FText::GetEmpty());
	}
	if (UnavailableReasonText)
	{
		UnavailableReasonText->SetText(FText::GetEmpty());
		UnavailableReasonText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (ThumbnailImage)
	{
		ThumbnailImage->SetBrush(FSlateBrush());
	}
	if (SelectedIndicator)
	{
		SelectedIndicator->SetVisibility(ESlateVisibility::Collapsed);
	}
	SetIsEnabled(true);
}
