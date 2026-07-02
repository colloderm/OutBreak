// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Shop/Entries/ShopTabEntryWidget.h"

#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "UI/Shop/EntryData/ShopTabEntryData.h"
#include "UI/Shop/ShopWidgetTypes.h"

void UShopTabEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	ResetEntry();

	const UShopTabEntryData* EntryData = Cast<UShopTabEntryData>(ListItemObject);
	if (!EntryData)
	{
		UE_LOG(LogShopWidget, Warning, TEXT("ShopTabEntryWidget received invalid entry data."));
		return;
	}

	const FShopTabViewData& ViewData = EntryData->GetViewData();
	if (TabLabelText)
	{
		TabLabelText->SetText(ViewData.DisplayName);
	}
	if (SelectedIndicator)
	{
		SelectedIndicator->SetVisibility(EntryData->IsSelected() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	SetIsEnabled(ViewData.bEnabled);
}

void UShopTabEntryWidget::ResetEntry()
{
	if (TabLabelText)
	{
		TabLabelText->SetText(FText::GetEmpty());
	}
	if (SelectedIndicator)
	{
		SelectedIndicator->SetVisibility(ESlateVisibility::Collapsed);
	}
	SetIsEnabled(true);
}
