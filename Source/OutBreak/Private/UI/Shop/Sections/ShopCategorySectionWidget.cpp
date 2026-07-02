// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Shop/Sections/ShopCategorySectionWidget.h"

#include "Components/Button.h"
#include "Components/ListView.h"
#include "Components/TextBlock.h"
#include "UI/Shop/EntryData/ShopCategoryEntryData.h"

void UShopCategorySectionWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (CategoryListView)
	{
		CategoryListView->OnItemClicked().AddUObject(this, &UShopCategorySectionWidget::HandleCategoryItemClicked);
	}
	if (RefreshButton)
	{
		RefreshButton->OnClicked.AddUniqueDynamic(this, &UShopCategorySectionWidget::HandleRefreshClicked);
	}
}

void UShopCategorySectionWidget::NativeDestruct()
{
	if (CategoryListView)
	{
		CategoryListView->OnItemClicked().RemoveAll(this);
	}
	if (RefreshButton)
	{
		RefreshButton->OnClicked.RemoveDynamic(this, &UShopCategorySectionWidget::HandleRefreshClicked);
	}

	Super::NativeDestruct();
}

void UShopCategorySectionWidget::ApplyCategories(const TArray<FShopCategoryViewData>& Categories)
{
	ClearCategories();

	if (!CategoryListView)
	{
		SetEmptyState(true);
		return;
	}

	TArray<UObject*> ListItems;
	ListItems.Reserve(Categories.Num());
	CategoryEntryDataObjects.Reserve(Categories.Num());

	for (const FShopCategoryViewData& Category : Categories)
	{
		UShopCategoryEntryData* EntryData = NewObject<UShopCategoryEntryData>(this);
		EntryData->Initialize(Category, Category.CategoryId == SelectedCategoryId);
		CategoryEntryDataObjects.Add(EntryData);
		ListItems.Add(EntryData);
	}

	CategoryListView->SetListItems(ListItems);
	SetEmptyState(Categories.Num() == 0);
}

void UShopCategorySectionWidget::SetSelectedCategory(FName CategoryId)
{
	SelectedCategoryId = CategoryId;
	RebuildSelectionState();
}

void UShopCategorySectionWidget::SetStockRefreshText(const FText& RefreshText)
{
	if (StockRefreshText)
	{
		StockRefreshText->SetText(RefreshText);
	}
}

void UShopCategorySectionWidget::SetEmptyState(bool bIsEmpty)
{
	if (EmptyCategoryText)
	{
		EmptyCategoryText->SetVisibility(bIsEmpty ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UShopCategorySectionWidget::ClearCategories()
{
	CategoryEntryDataObjects.Reset();
	if (CategoryListView)
	{
		CategoryListView->ClearListItems();
	}
}

void UShopCategorySectionWidget::SetInteractionEnabled(bool bEnabled)
{
	SetIsEnabled(bEnabled);
	if (CategoryListView)
	{
		CategoryListView->SetIsEnabled(bEnabled);
	}
	if (RefreshButton)
	{
		RefreshButton->SetIsEnabled(bEnabled);
	}
}

void UShopCategorySectionWidget::HandleRefreshClicked()
{
	OnStockRefreshRequested.Broadcast();
}

void UShopCategorySectionWidget::HandleCategoryItemClicked(UObject* Item)
{
	const UShopCategoryEntryData* EntryData = Cast<UShopCategoryEntryData>(Item);
	if (!EntryData || !EntryData->GetViewData().bEnabled)
	{
		return;
	}

	OnCategoryRequested.Broadcast(EntryData->GetCategoryId());
}

void UShopCategorySectionWidget::RebuildSelectionState()
{
	for (UShopCategoryEntryData* EntryData : CategoryEntryDataObjects)
	{
		if (EntryData)
		{
			EntryData->SetSelected(EntryData->GetCategoryId() == SelectedCategoryId);
		}
	}
	if (CategoryListView)
	{
		CategoryListView->RequestRefresh();
	}
}
