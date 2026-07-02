// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Shop/Entries/ShopItemStatEntryWidget.h"

#include "Components/TextBlock.h"
#include "UI/Shop/EntryData/ShopItemStatEntryData.h"
#include "UI/Shop/ShopWidgetTypes.h"

void UShopItemStatEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	ResetEntry();

	const UShopItemStatEntryData* EntryData = Cast<UShopItemStatEntryData>(ListItemObject);
	if (!EntryData)
	{
		UE_LOG(LogShopWidget, Warning, TEXT("ShopItemStatEntryWidget received invalid entry data."));
		return;
	}

	const FShopItemStatViewData& ViewData = EntryData->GetViewData();
	if (StatNameText)
	{
		StatNameText->SetText(ViewData.DisplayName);
	}
	if (StatValueText)
	{
		StatValueText->SetText(ViewData.ValueText);
	}
	if (DeltaText)
	{
		const bool bHasDelta = !ViewData.DeltaText.IsEmpty();
		DeltaText->SetText(ViewData.DeltaText);
		DeltaText->SetVisibility(bHasDelta ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UShopItemStatEntryWidget::ResetEntry()
{
	if (StatNameText)
	{
		StatNameText->SetText(FText::GetEmpty());
	}
	if (StatValueText)
	{
		StatValueText->SetText(FText::GetEmpty());
	}
	if (DeltaText)
	{
		DeltaText->SetText(FText::GetEmpty());
		DeltaText->SetVisibility(ESlateVisibility::Collapsed);
	}
}
