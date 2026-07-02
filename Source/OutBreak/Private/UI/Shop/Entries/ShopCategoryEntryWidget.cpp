// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Shop/Entries/ShopCategoryEntryWidget.h"

#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "UI/Shop/EntryData/ShopCategoryEntryData.h"
#include "UI/Shop/ShopWidgetTypes.h"

void UShopCategoryEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	ResetEntry();

	const UShopCategoryEntryData* EntryData = Cast<UShopCategoryEntryData>(ListItemObject);
	if (!EntryData)
	{
		UE_LOG(LogShopWidget, Warning, TEXT("ShopCategoryEntryWidget received invalid entry data."));
		return;
	}

	const FShopCategoryViewData& ViewData = EntryData->GetViewData();
	if (CategoryNameText)
	{
		CategoryNameText->SetText(ViewData.DisplayName);
	}
	if (ItemCountText)
	{
		ItemCountText->SetText(FText::AsNumber(ViewData.ItemCount));
	}
	if (SelectedIndicator)
	{
		SelectedIndicator->SetVisibility(EntryData->IsSelected() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	SetIsEnabled(ViewData.bEnabled);
}

void UShopCategoryEntryWidget::ResetEntry()
{
	if (CategoryNameText)
	{
		CategoryNameText->SetText(FText::GetEmpty());
	}
	if (ItemCountText)
	{
		ItemCountText->SetText(FText::GetEmpty());
	}
	if (SelectedIndicator)
	{
		SelectedIndicator->SetVisibility(ESlateVisibility::Collapsed);
	}
	SetIsEnabled(true);
}
