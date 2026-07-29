// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Shop/ShopWindow.h"

#include "InputCoreTypes.h"
#include "UI/Shop/ShopCategory.h"
#include "UI/Shop/ShopItemInspector.h"
#include "UI/Shop/ShopItemList.h"
#include "UI/Shop/ShopkeeperPortrait.h"
#include "UI/Shop/UserCurrency.h"

void UShopWindow::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);
	BindChildDelegates();
}

void UShopWindow::NativeDestruct()
{
	UnbindChildDelegates();
	Super::NativeDestruct();
}

FReply UShopWindow::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (const FShopItemViewData* SelectedItem = FindDisplayedItem(CurrentSelectedItemId))
	{
		for (const FShopActionViewData& Action : SelectedItem->Actions)
		{
			if (Action.InputKey.IsValid() && Action.InputKey == InKeyEvent.GetKey())
			{
				HandleActionTriggered(Action.ActionId);
				return FReply::Handled();
			}
		}
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UShopWindow::InitializeShop(const FShopWindowViewData& InData)
{
	ApplyViewData(InData, false);
}

void UShopWindow::RefreshShop(const FShopWindowViewData& InData)
{
	ApplyViewData(InData, true);
}

void UShopWindow::ClearShop()
{
	CurrentViewData = FShopWindowViewData();
	DisplayedItems.Reset();
	CategoryIndexById.Reset();
	ItemIndexById.Reset();
	CurrentSelectedCategoryId = NAME_None;
	CurrentSelectedItemId = NAME_None;

	if (WBP_ShopkeeperPortrait)
	{
		WBP_ShopkeeperPortrait->ClearShopkeeperData();
	}

	if (WBP_UserCurrency)
	{
		WBP_UserCurrency->ClearCurrencyData();
	}

	if (WBP_ShopCategory)
	{
		WBP_ShopCategory->ClearCategories();
		WBP_ShopCategory->SetNewStockTime(FText::GetEmpty());
	}

	if (WBP_ShopItemList)
	{
		WBP_ShopItemList->ClearItems();
	}

	if (WBP_ItemInspector)
	{
		WBP_ItemInspector->ClearItemData();
	}
}

void UShopWindow::UpdateCurrency(const FUserCurrencyViewData& InData)
{
	CurrentViewData.Currency = InData;

	if (WBP_UserCurrency)
	{
		WBP_UserCurrency->SetCurrencyData(InData);
	}
}

void UShopWindow::UpdateItem(const FShopItemViewData& InData)
{
	if (InData.ItemId.IsNone())
	{
		return;
	}

	bool bUpdated = false;
	for (FShopItemViewData& Item : CurrentViewData.Items)
	{
		if (Item.ItemId == InData.ItemId)
		{
			Item = InData;
			bUpdated = true;
			break;
		}
	}

	if (!bUpdated)
	{
		CurrentViewData.Items.Add(InData);
	}

	RebuildIndexes();
	RefreshCurrentCategoryItems();

	if (!IsItemSelectableInCategory(CurrentSelectedItemId, CurrentSelectedCategoryId))
	{
		CurrentSelectedItemId = ResolveItemSelection(CurrentSelectedCategoryId, CurrentSelectedItemId);
	}

	if (!CurrentSelectedItemId.IsNone() && WBP_ShopItemList)
	{
		WBP_ShopItemList->SelectItem(CurrentSelectedItemId);
	}

	RefreshInspector();
}

void UShopWindow::UpdateItems(const TArray<FShopItemViewData>& InItems)
{
	CurrentViewData.Items = InItems;
	RebuildIndexes();
	RefreshCurrentCategoryItems();

	CurrentSelectedItemId = ResolveItemSelection(CurrentSelectedCategoryId, CurrentSelectedItemId);
	if (!CurrentSelectedItemId.IsNone() && WBP_ShopItemList)
	{
		WBP_ShopItemList->SelectItem(CurrentSelectedItemId);
	}

	RefreshInspector();
}

void UShopWindow::UpdateCategories(const TArray<FShopCategoryViewData>& InCategories)
{
	CurrentViewData.Categories = InCategories;
	RebuildIndexes();

	CurrentSelectedCategoryId = ResolveCategorySelection(CurrentSelectedCategoryId, CurrentSelectedItemId);
	if (WBP_ShopCategory)
	{
		WBP_ShopCategory->SetCategories(CurrentViewData.Categories);
		if (!CurrentSelectedCategoryId.IsNone())
		{
			WBP_ShopCategory->SelectCategory(CurrentSelectedCategoryId);
		}
	}

	RefreshCurrentCategoryItems();
	CurrentSelectedItemId = ResolveItemSelection(CurrentSelectedCategoryId, CurrentSelectedItemId);
	if (!CurrentSelectedItemId.IsNone() && WBP_ShopItemList)
	{
		WBP_ShopItemList->SelectItem(CurrentSelectedItemId);
	}

	RefreshInspector();
}

bool UShopWindow::SelectCategory(FName CategoryId)
{
	return SelectCategoryInternal(CategoryId, true);
}

bool UShopWindow::SelectItem(FName ItemId)
{
	return SelectItemInternal(ItemId, true);
}

FName UShopWindow::GetSelectedCategoryId() const
{
	return CurrentSelectedCategoryId;
}

FName UShopWindow::GetSelectedItemId() const
{
	return CurrentSelectedItemId;
}

const FShopWindowViewData& UShopWindow::GetCurrentViewData() const
{
	return CurrentViewData;
}

void UShopWindow::RequestClose()
{
	OnShopCloseRequested.Broadcast(CurrentViewData.ShopId);

	Super::RequestClose();
}

void UShopWindow::HandleCategorySelected(FName CategoryId)
{
	SelectCategoryInternal(CategoryId, true);
}

void UShopWindow::HandleItemSelected(FName ItemId)
{
	SelectItemInternal(ItemId, true);
}

void UShopWindow::HandleActionTriggered(FName ActionId)
{
	const FShopItemViewData* SelectedItem = FindDisplayedItem(CurrentSelectedItemId);
	const FShopActionViewData* Action = FindActionForSelectedItem(ActionId);
	if (!SelectedItem || !Action || !Action->bCanExecute)
	{
		return;
	}

	const int32 RequestQuantity = FMath::Max(1, Action->Quantity);

	OnShopActionRequested.Broadcast(CurrentViewData.ShopId, SelectedItem->ItemId, Action->ActionId, RequestQuantity);

	if (Action->ActionType == EShopActionType::Purchase)
	{
		OnPurchaseRequested.Broadcast(CurrentViewData.ShopId, SelectedItem->ItemId, Action->ActionId, RequestQuantity);
	}
	else if (Action->ActionType == EShopActionType::Exchange)
	{
		OnExchangeRequested.Broadcast(CurrentViewData.ShopId, SelectedItem->ItemId, Action->ActionId, RequestQuantity);
	}
}

void UShopWindow::BindChildDelegates()
{
	if (WBP_ShopCategory)
	{
		WBP_ShopCategory->OnCategorySelected.RemoveDynamic(this, &UShopWindow::HandleCategorySelected);
		WBP_ShopCategory->OnCategorySelected.AddDynamic(this, &UShopWindow::HandleCategorySelected);
	}

	if (WBP_ShopItemList)
	{
		WBP_ShopItemList->OnItemSelected.RemoveDynamic(this, &UShopWindow::HandleItemSelected);
		WBP_ShopItemList->OnItemSelected.AddDynamic(this, &UShopWindow::HandleItemSelected);
	}

	if (WBP_ItemInspector)
	{
		WBP_ItemInspector->OnActionTriggered.RemoveDynamic(this, &UShopWindow::HandleActionTriggered);
		WBP_ItemInspector->OnActionTriggered.AddDynamic(this, &UShopWindow::HandleActionTriggered);
	}
}

void UShopWindow::UnbindChildDelegates()
{
	if (WBP_ShopCategory)
	{
		WBP_ShopCategory->OnCategorySelected.RemoveDynamic(this, &UShopWindow::HandleCategorySelected);
	}

	if (WBP_ShopItemList)
	{
		WBP_ShopItemList->OnItemSelected.RemoveDynamic(this, &UShopWindow::HandleItemSelected);
	}

	if (WBP_ItemInspector)
	{
		WBP_ItemInspector->OnActionTriggered.RemoveDynamic(this, &UShopWindow::HandleActionTriggered);
	}
}

void UShopWindow::ApplyViewData(const FShopWindowViewData& InData, bool bPreserveSelection)
{
	const FName PreviousCategoryId = CurrentSelectedCategoryId;
	const FName PreviousItemId = CurrentSelectedItemId;

	CurrentViewData = InData;
	RebuildIndexes();

	if (WBP_ShopkeeperPortrait)
	{
		WBP_ShopkeeperPortrait->SetShopkeeperData(CurrentViewData.Shopkeeper);
	}

	if (WBP_UserCurrency)
	{
		WBP_UserCurrency->SetCurrencyData(CurrentViewData.Currency);
	}

	if (WBP_ShopCategory)
	{
		WBP_ShopCategory->SetNewStockTime(CurrentViewData.NewStockText);
		WBP_ShopCategory->SetCategories(CurrentViewData.Categories);
	}

	const FName PreferredCategoryId = bPreserveSelection ? PreviousCategoryId : CurrentViewData.InitialSelectedCategoryId;
	const FName PreferredItemId = bPreserveSelection ? PreviousItemId : CurrentViewData.InitialSelectedItemId;

	CurrentSelectedCategoryId = ResolveCategorySelection(PreferredCategoryId, PreferredItemId);
	if (!CurrentSelectedCategoryId.IsNone() && WBP_ShopCategory)
	{
		WBP_ShopCategory->SelectCategory(CurrentSelectedCategoryId);
	}

	RefreshCurrentCategoryItems();

	CurrentSelectedItemId = ResolveItemSelection(CurrentSelectedCategoryId, PreferredItemId);
	if (!CurrentSelectedItemId.IsNone() && WBP_ShopItemList)
	{
		WBP_ShopItemList->SelectItem(CurrentSelectedItemId);
	}

	RefreshInspector();
}

void UShopWindow::RebuildIndexes()
{
	CategoryIndexById.Reset();
	ItemIndexById.Reset();

	for (int32 Index = 0; Index < CurrentViewData.Categories.Num(); ++Index)
	{
		const FShopCategoryViewData& Category = CurrentViewData.Categories[Index];
		if (Category.CategoryId.IsNone())
		{
			continue;
		}

		if (CategoryIndexById.Contains(Category.CategoryId))
		{
			ensureMsgf(false, TEXT("Duplicate shop category id: %s"), *Category.CategoryId.ToString());
			continue;
		}

		CategoryIndexById.Add(Category.CategoryId, Index);
	}

	for (int32 Index = 0; Index < CurrentViewData.Items.Num(); ++Index)
	{
		const FShopItemViewData& Item = CurrentViewData.Items[Index];
		if (Item.ItemId.IsNone())
		{
			continue;
		}

		if (ItemIndexById.Contains(Item.ItemId))
		{
			ensureMsgf(false, TEXT("Duplicate shop item id: %s"), *Item.ItemId.ToString());
			continue;
		}

		if (!Item.CategoryId.IsNone() && !CategoryIndexById.Contains(Item.CategoryId))
		{
			ensureMsgf(false, TEXT("Shop item %s uses missing category id %s."), *Item.ItemId.ToString(), *Item.CategoryId.ToString());
		}

		ItemIndexById.Add(Item.ItemId, Index);
	}
}

void UShopWindow::RefreshCurrentCategoryItems()
{
	DisplayedItems = BuildItemsForCategory(CurrentSelectedCategoryId);

	if (WBP_ShopItemList)
	{
		WBP_ShopItemList->SetItems(DisplayedItems);
	}
}

void UShopWindow::RefreshInspector()
{
	if (!WBP_ItemInspector)
	{
		return;
	}

	if (const FShopItemViewData* SelectedItem = FindDisplayedItem(CurrentSelectedItemId))
	{
		WBP_ItemInspector->SetItemData(*SelectedItem);
	}
	else
	{
		WBP_ItemInspector->ClearItemData();
	}
}

bool UShopWindow::SelectCategoryInternal(FName CategoryId, bool bBroadcast)
{
	if (!IsCategorySelectable(CategoryId))
	{
		return false;
	}

	CurrentSelectedCategoryId = CategoryId;
	if (WBP_ShopCategory)
	{
		WBP_ShopCategory->SelectCategory(CategoryId);
	}

	RefreshCurrentCategoryItems();

	const FName NewSelectedItemId = ResolveItemSelection(CurrentSelectedCategoryId, CurrentSelectedItemId);
	const bool bItemChanged = NewSelectedItemId != CurrentSelectedItemId;
	CurrentSelectedItemId = NewSelectedItemId;

	if (!CurrentSelectedItemId.IsNone() && WBP_ShopItemList)
	{
		WBP_ShopItemList->SelectItem(CurrentSelectedItemId);
	}

	RefreshInspector();

	if (bBroadcast)
	{
		OnCategorySelectionChanged.Broadcast(CurrentViewData.ShopId, CurrentSelectedCategoryId);
		if (bItemChanged)
		{
			OnItemSelectionChanged.Broadcast(CurrentViewData.ShopId, CurrentSelectedItemId);
		}
	}

	return true;
}

bool UShopWindow::SelectItemInternal(FName ItemId, bool bBroadcast)
{
	if (!IsItemSelectableInCategory(ItemId, CurrentSelectedCategoryId))
	{
		return false;
	}

	CurrentSelectedItemId = ItemId;
	if (WBP_ShopItemList)
	{
		WBP_ShopItemList->SelectItem(ItemId);
	}

	RefreshInspector();

	if (bBroadcast)
	{
		OnItemSelectionChanged.Broadcast(CurrentViewData.ShopId, CurrentSelectedItemId);
	}

	return true;
}

FName UShopWindow::ResolveCategorySelection(FName PreferredCategoryId, FName PreferredItemId) const
{
	if (IsCategorySelectable(PreferredCategoryId))
	{
		return PreferredCategoryId;
	}

	if (const FShopItemViewData* PreferredItem = FindItem(PreferredItemId))
	{
		if (IsCategorySelectable(PreferredItem->CategoryId))
		{
			return PreferredItem->CategoryId;
		}
	}

	TArray<FShopCategoryViewData> SortedCategories = CurrentViewData.Categories;
	SortedCategories.Sort([](const FShopCategoryViewData& Left, const FShopCategoryViewData& Right)
	{
		if (Left.SortOrder == Right.SortOrder)
		{
			return Left.CategoryId.LexicalLess(Right.CategoryId);
		}
		return Left.SortOrder < Right.SortOrder;
	});

	for (const FShopCategoryViewData& Category : SortedCategories)
	{
		if (Category.bIsEnabled && !Category.CategoryId.IsNone())
		{
			return Category.CategoryId;
		}
	}

	return NAME_None;
}

FName UShopWindow::ResolveItemSelection(FName CategoryId, FName PreferredItemId) const
{
	if (IsItemSelectableInCategory(PreferredItemId, CategoryId))
	{
		return PreferredItemId;
	}

	for (const FShopItemViewData& Item : DisplayedItems)
	{
		if (Item.bIsEnabled && !Item.ItemId.IsNone())
		{
			return Item.ItemId;
		}
	}

	return NAME_None;
}

const FShopCategoryViewData* UShopWindow::FindCategory(FName CategoryId) const
{
	const int32* Index = CategoryIndexById.Find(CategoryId);
	return (Index && CurrentViewData.Categories.IsValidIndex(*Index)) ? &CurrentViewData.Categories[*Index] : nullptr;
}

const FShopItemViewData* UShopWindow::FindItem(FName ItemId) const
{
	const int32* Index = ItemIndexById.Find(ItemId);
	return (Index && CurrentViewData.Items.IsValidIndex(*Index)) ? &CurrentViewData.Items[*Index] : nullptr;
}

const FShopItemViewData* UShopWindow::FindDisplayedItem(FName ItemId) const
{
	return DisplayedItems.FindByPredicate([ItemId](const FShopItemViewData& Item)
	{
		return Item.ItemId == ItemId;
	});
}

const FShopActionViewData* UShopWindow::FindActionForSelectedItem(FName ActionId) const
{
	const FShopItemViewData* SelectedItem = FindDisplayedItem(CurrentSelectedItemId);
	if (!SelectedItem)
	{
		return nullptr;
	}

	return SelectedItem->Actions.FindByPredicate([ActionId](const FShopActionViewData& Action)
	{
		return Action.ActionId == ActionId;
	});
}

bool UShopWindow::IsCategorySelectable(FName CategoryId) const
{
	const FShopCategoryViewData* Category = FindCategory(CategoryId);
	return Category && Category->bIsEnabled && !Category->CategoryId.IsNone();
}

bool UShopWindow::IsItemSelectableInCategory(FName ItemId, FName CategoryId) const
{
	const FShopItemViewData* Item = FindItem(ItemId);
	return Item && Item->bIsEnabled && Item->CategoryId == CategoryId && !Item->ItemId.IsNone();
}

TArray<FShopItemViewData> UShopWindow::BuildItemsForCategory(FName CategoryId) const
{
	TArray<FShopItemViewData> Result;
	for (const FShopItemViewData& Item : CurrentViewData.Items)
	{
		if (Item.CategoryId == CategoryId)
		{
			Result.Add(Item);
		}
	}

	Result.Sort([](const FShopItemViewData& Left, const FShopItemViewData& Right)
	{
		if (Left.SortOrder == Right.SortOrder)
		{
			return Left.ItemId.LexicalLess(Right.ItemId);
		}
		return Left.SortOrder < Right.SortOrder;
	});

	return Result;
}
