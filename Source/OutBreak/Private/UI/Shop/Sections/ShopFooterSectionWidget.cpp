// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Shop/Sections/ShopFooterSectionWidget.h"

#include "Components/Button.h"
#include "Components/ListView.h"
#include "UI/Shop/EntryData/ShopKeyHintEntryData.h"

void UShopFooterSectionWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (CloseButton)
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &UShopFooterSectionWidget::HandleCloseClicked);
	}
	if (BackButton)
	{
		BackButton->OnClicked.AddUniqueDynamic(this, &UShopFooterSectionWidget::HandleBackClicked);
	}
	if (CompareButton)
	{
		CompareButton->OnClicked.AddUniqueDynamic(this, &UShopFooterSectionWidget::HandleCompareClicked);
	}
	if (HelpButton)
	{
		HelpButton->OnClicked.AddUniqueDynamic(this, &UShopFooterSectionWidget::HandleHelpClicked);
	}
}

void UShopFooterSectionWidget::NativeDestruct()
{
	if (CloseButton)
	{
		CloseButton->OnClicked.RemoveDynamic(this, &UShopFooterSectionWidget::HandleCloseClicked);
	}
	if (BackButton)
	{
		BackButton->OnClicked.RemoveDynamic(this, &UShopFooterSectionWidget::HandleBackClicked);
	}
	if (CompareButton)
	{
		CompareButton->OnClicked.RemoveDynamic(this, &UShopFooterSectionWidget::HandleCompareClicked);
	}
	if (HelpButton)
	{
		HelpButton->OnClicked.RemoveDynamic(this, &UShopFooterSectionWidget::HandleHelpClicked);
	}

	Super::NativeDestruct();
}

void UShopFooterSectionWidget::ApplyKeyHints(const TArray<FShopKeyHintViewData>& KeyHints)
{
	ClearKeyHints();

	if (!KeyHintListView)
	{
		return;
	}

	TArray<UObject*> ListItems;
	ListItems.Reserve(KeyHints.Num());
	KeyHintEntryDataObjects.Reserve(KeyHints.Num());

	for (const FShopKeyHintViewData& KeyHint : KeyHints)
	{
		UShopKeyHintEntryData* EntryData = NewObject<UShopKeyHintEntryData>(this);
		EntryData->Initialize(KeyHint.ActionText, KeyHint.KeyText);
		KeyHintEntryDataObjects.Add(EntryData);
		ListItems.Add(EntryData);
	}

	KeyHintListView->SetListItems(ListItems);
}

void UShopFooterSectionWidget::ClearKeyHints()
{
	KeyHintEntryDataObjects.Reset();
	if (KeyHintListView)
	{
		KeyHintListView->ClearListItems();
	}
}

void UShopFooterSectionWidget::SetInteractionEnabled(bool bEnabled)
{
	SetIsEnabled(bEnabled);
	if (CloseButton)
	{
		CloseButton->SetIsEnabled(bEnabled);
	}
	if (BackButton)
	{
		BackButton->SetIsEnabled(bEnabled);
	}
	if (CompareButton)
	{
		CompareButton->SetIsEnabled(bEnabled);
	}
	if (HelpButton)
	{
		HelpButton->SetIsEnabled(bEnabled);
	}
}

void UShopFooterSectionWidget::HandleCloseClicked()
{
	OnCloseRequested.Broadcast();
}

void UShopFooterSectionWidget::HandleBackClicked()
{
	OnBackRequested.Broadcast();
}

void UShopFooterSectionWidget::HandleCompareClicked()
{
	OnCompareRequested.Broadcast();
}

void UShopFooterSectionWidget::HandleHelpClicked()
{
	OnHelpRequested.Broadcast();
}
