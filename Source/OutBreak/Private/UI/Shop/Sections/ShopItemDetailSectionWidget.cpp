// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Shop/Sections/ShopItemDetailSectionWidget.h"

#include "Components/Image.h"
#include "Components/ListView.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/Texture2D.h"
#include "UI/Shop/EntryData/ShopItemStatEntryData.h"
#include "UI/Shop/Sections/ShopActionSectionWidget.h"

void UShopItemDetailSectionWidget::ApplyItemDetail(const FShopItemDetailViewData& ItemDetail)
{
	SetEmptyState(false);

	if (ItemNameText)
	{
		ItemNameText->SetText(ItemDetail.DisplayName);
	}
	if (ItemTypeText)
	{
		ItemTypeText->SetText(ItemDetail.ItemTypeText);
	}
	if (RarityText)
	{
		RarityText->SetText(ItemDetail.RarityText);
	}
	if (DescriptionText)
	{
		DescriptionText->SetText(ItemDetail.DescriptionText);
	}
	if (OwnedQuantityText)
	{
		OwnedQuantityText->SetText(ItemDetail.OwnedQuantityText);
	}
	if (PriceText)
	{
		PriceText->SetText(ItemDetail.PriceText);
	}
	if (ItemPreviewImage)
	{
		if (UTexture2D* LoadedTexture = ItemDetail.PreviewTexture.Get())
		{
			ItemPreviewImage->SetBrushFromTexture(LoadedTexture);
		}
		else
		{
			ItemPreviewImage->SetBrush(ItemDetail.PreviewBrush);
		}
	}
	if (CurrencyIconImage)
	{
		if (UTexture2D* LoadedTexture = ItemDetail.CurrencyIconTexture.Get())
		{
			CurrencyIconImage->SetBrushFromTexture(LoadedTexture);
		}
		else
		{
			CurrencyIconImage->SetBrush(ItemDetail.CurrencyIconBrush);
		}
	}

	StatEntryDataObjects.Reset();
	if (StatListView)
	{
		TArray<UObject*> ListItems;
		ListItems.Reserve(ItemDetail.Stats.Num());
		StatEntryDataObjects.Reserve(ItemDetail.Stats.Num());

		for (const FShopItemStatViewData& Stat : ItemDetail.Stats)
		{
			UShopItemStatEntryData* EntryData = NewObject<UShopItemStatEntryData>(this);
			EntryData->Initialize(Stat);
			StatEntryDataObjects.Add(EntryData);
			ListItems.Add(EntryData);
		}

		StatListView->SetListItems(ListItems);
	}

	ApplyActionState(ItemDetail.ActionState);
}

void UShopItemDetailSectionWidget::ApplyActionState(const FShopActionState& ActionState)
{
	if (ActionSection)
	{
		ActionSection->ApplyActionState(ActionState);
	}
}

void UShopItemDetailSectionWidget::ClearItemDetail()
{
	if (ItemNameText)
	{
		ItemNameText->SetText(FText::GetEmpty());
	}
	if (ItemTypeText)
	{
		ItemTypeText->SetText(FText::GetEmpty());
	}
	if (RarityText)
	{
		RarityText->SetText(FText::GetEmpty());
	}
	if (DescriptionText)
	{
		DescriptionText->SetText(FText::GetEmpty());
	}
	if (ItemPreviewImage)
	{
		ItemPreviewImage->SetBrush(FSlateBrush());
	}
	if (OwnedQuantityText)
	{
		OwnedQuantityText->SetText(FText::GetEmpty());
	}
	if (PriceText)
	{
		PriceText->SetText(FText::GetEmpty());
	}
	if (CurrencyIconImage)
	{
		CurrencyIconImage->SetBrush(FSlateBrush());
	}
	if (StatListView)
	{
		StatListView->ClearListItems();
	}
	StatEntryDataObjects.Reset();

	if (ActionSection)
	{
		ActionSection->ResetActionState();
	}

	SetEmptyState(true);
}

void UShopItemDetailSectionWidget::SetEmptyState(bool bIsEmpty)
{
	if (EmptyStateWidget)
	{
		EmptyStateWidget->SetVisibility(bIsEmpty ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UShopItemDetailSectionWidget::SetInteractionEnabled(bool bEnabled)
{
	SetIsEnabled(bEnabled);
	if (StatListView)
	{
		StatListView->SetIsEnabled(bEnabled);
	}
	if (ActionSection)
	{
		ActionSection->SetInteractionEnabled(bEnabled);
	}
}
