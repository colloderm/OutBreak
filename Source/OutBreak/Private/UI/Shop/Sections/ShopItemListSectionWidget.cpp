// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Shop/Sections/ShopItemListSectionWidget.h"

#include "Components/Button.h"
#include "Components/ListView.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "UI/Shop/EntryData/ShopItemEntryData.h"

void UShopItemListSectionWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (ItemListView)
	{
		ItemListView->OnItemClicked().AddUObject(this, &UShopItemListSectionWidget::HandleItemClicked);
	}
	if (SortButton)
	{
		SortButton->OnClicked.AddUniqueDynamic(this, &UShopItemListSectionWidget::HandleSortClicked);
	}
}

void UShopItemListSectionWidget::NativeDestruct()
{
	if (ItemListView)
	{
		ItemListView->OnItemClicked().RemoveAll(this);
	}
	if (SortButton)
	{
		SortButton->OnClicked.RemoveDynamic(this, &UShopItemListSectionWidget::HandleSortClicked);
	}

	Super::NativeDestruct();
}

void UShopItemListSectionWidget::ApplyItems(const TArray<FShopItemSummaryViewData>& Items)
{
	ClearItems();

	if (!ItemListView)
	{
		SetEmptyState(true);
		SetItemCount(0);
		return;
	}

	TArray<UObject*> ListItems;
	ListItems.Reserve(Items.Num());
	ItemEntryDataObjects.Reserve(Items.Num());

	for (const FShopItemSummaryViewData& Item : Items)
	{
		UShopItemEntryData* EntryData = NewObject<UShopItemEntryData>(this);
		EntryData->Initialize(Item, Item.ItemId == SelectedItemId);
		ItemEntryDataObjects.Add(EntryData);
		ListItems.Add(EntryData);
	}

	ItemListView->SetListItems(ListItems);
	SetItemCount(Items.Num());
	SetEmptyState(Items.Num() == 0);
}

void UShopItemListSectionWidget::SetSelectedItem(FName ItemId)
{
	SelectedItemId = ItemId;
	RebuildSelectionState();
}

void UShopItemListSectionWidget::SetItemCount(int32 ItemCount)
{
	if (ItemCountText)
	{
		ItemCountText->SetText(FText::AsNumber(ItemCount));
	}
}

void UShopItemListSectionWidget::SetEmptyState(bool bIsEmpty)
{
	if (EmptyStateText)
	{
		EmptyStateText->SetVisibility(bIsEmpty ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UShopItemListSectionWidget::SetLoadingState(bool bIsLoading)
{
	if (LoadingIndicator)
	{
		LoadingIndicator->SetVisibility(bIsLoading ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UShopItemListSectionWidget::ClearItems()
{
	ItemEntryDataObjects.Reset();
	if (ItemListView)
	{
		ItemListView->ClearListItems();
	}
	SetItemCount(0);
}

void UShopItemListSectionWidget::SetCurrentSort(FName SortId)
{
	CurrentSortId = SortId;
}

void UShopItemListSectionWidget::SetInteractionEnabled(bool bEnabled)
{
	SetIsEnabled(bEnabled);
	if (ItemListView)
	{
		ItemListView->SetIsEnabled(bEnabled);
	}
	if (SortButton)
	{
		SortButton->SetIsEnabled(bEnabled);
	}
}

void UShopItemListSectionWidget::HandleSortClicked()
{
	OnSortRequested.Broadcast(CurrentSortId);
}

void UShopItemListSectionWidget::HandleItemClicked(UObject* Item)
{
	const UShopItemEntryData* EntryData = Cast<UShopItemEntryData>(Item);
	if (!EntryData)
	{
		return;
	}

	OnItemSelectionRequested.Broadcast(EntryData->GetItemId());
}

void UShopItemListSectionWidget::RebuildSelectionState()
{
	for (UShopItemEntryData* EntryData : ItemEntryDataObjects)
	{
		if (EntryData)
		{
			EntryData->SetSelected(EntryData->GetItemId() == SelectedItemId);
		}
	}
	if (ItemListView)
	{
		ItemListView->RequestRefresh();
	}
}
