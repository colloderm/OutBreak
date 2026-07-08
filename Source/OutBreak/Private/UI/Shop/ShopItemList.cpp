// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Shop/ShopItemList.h"

#include "Components/VerticalBox.h"
#include "UI/Shop/ItemListElement.h"

void UShopItemList::NativeConstruct()
{
	Super::NativeConstruct();
	ResolveItemElementClass();
}

void UShopItemList::NativeDestruct()
{
	ClearItemElements();
	Super::NativeDestruct();
}

void UShopItemList::SetItems(const TArray<FShopItemViewData>& InItems)
{
	ResolveItemElementClass();
	ClearItemElements();

	DisplayedItems = InItems;
	DisplayedItems.Sort([](const FShopItemViewData& Left, const FShopItemViewData& Right)
	{
		if (Left.SortOrder == Right.SortOrder)
		{
			return Left.ItemId.LexicalLess(Right.ItemId);
		}
		return Left.SortOrder < Right.SortOrder;
	});

	if (!VBX_ItemList)
	{
		ensureMsgf(false, TEXT("UShopItemList requires VBX_ItemList."));
		return;
	}

	if (!ItemElementClass)
	{
		ensureMsgf(false, TEXT("UShopItemList requires ItemElementClass or a template child in VBX_ItemList."));
		return;
	}

	for (const FShopItemViewData& Item : DisplayedItems)
	{
		UItemListElement* Element = CreateWidget<UItemListElement>(this, ItemElementClass);
		if (!Element)
		{
			continue;
		}

		Element->SetItemData(Item);
		Element->SetSelected(Item.ItemId == SelectedItemId);
		Element->OnItemSelected.RemoveDynamic(this, &UShopItemList::HandleItemElementSelected);
		Element->OnItemSelected.AddDynamic(this, &UShopItemList::HandleItemElementSelected);

		VBX_ItemList->AddChild(Element);
		ItemElements.Add(Element);
	}

	if (!SelectItem(SelectedItemId))
	{
		SelectedItemId = NAME_None;
		for (const FShopItemViewData& Item : DisplayedItems)
		{
			if (Item.bIsEnabled && !Item.ItemId.IsNone())
			{
				SelectItem(Item.ItemId);
				break;
			}
		}
	}
}

void UShopItemList::ClearItems()
{
	DisplayedItems.Reset();
	SelectedItemId = NAME_None;
	ClearItemElements();
}

bool UShopItemList::SelectItem(FName ItemId)
{
	bool bFound = false;
	bool bEnabled = false;

	for (const FShopItemViewData& Item : DisplayedItems)
	{
		if (Item.ItemId == ItemId)
		{
			bFound = true;
			bEnabled = Item.bIsEnabled;
			break;
		}
	}

	if (!bFound || !bEnabled)
	{
		return false;
	}

	SelectedItemId = ItemId;
	for (UItemListElement* Element : ItemElements)
	{
		if (Element)
		{
			Element->SetSelected(Element->GetItemId() == SelectedItemId);
		}
	}

	return true;
}

FName UShopItemList::GetSelectedItemId() const
{
	return SelectedItemId;
}

const FShopItemViewData* UShopItemList::FindDisplayedItem(FName ItemId) const
{
	return DisplayedItems.FindByPredicate([ItemId](const FShopItemViewData& Item)
	{
		return Item.ItemId == ItemId;
	});
}

void UShopItemList::HandleItemElementSelected(FName ItemId)
{
	if (SelectItem(ItemId))
	{
		OnItemSelected.Broadcast(ItemId);
	}
}

void UShopItemList::ResolveItemElementClass()
{
	if (ItemElementClass || !VBX_ItemList)
	{
		return;
	}

	for (int32 Index = 0; Index < VBX_ItemList->GetChildrenCount(); ++Index)
	{
		if (UItemListElement* ExistingElement = Cast<UItemListElement>(VBX_ItemList->GetChildAt(Index)))
		{
			ItemElementClass = ExistingElement->GetClass();
			return;
		}
	}
}

void UShopItemList::ClearItemElements()
{
	for (UItemListElement* Element : ItemElements)
	{
		if (Element)
		{
			Element->OnItemSelected.RemoveDynamic(this, &UShopItemList::HandleItemElementSelected);
		}
	}

	ItemElements.Reset();

	if (VBX_ItemList)
	{
		VBX_ItemList->ClearChildren();
	}
}
