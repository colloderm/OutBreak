// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Shop/ItemListElement.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"

#define LOCTEXT_NAMESPACE "ItemListElement"

void UItemListElement::NativeConstruct()
{
	Super::NativeConstruct();

	if (UButton* ClickButton = GetClickButton())
	{
		ClickButton->OnClicked.RemoveDynamic(this, &UItemListElement::HandleClicked);
		ClickButton->OnClicked.AddDynamic(this, &UItemListElement::HandleClicked);
	}

	SetSelected(bSelected);
	SetInteractionEnabled(bInteractionEnabled);
}

void UItemListElement::NativeDestruct()
{
	if (UButton* ClickButton = GetClickButton())
	{
		ClickButton->OnClicked.RemoveDynamic(this, &UItemListElement::HandleClicked);
	}

	Super::NativeDestruct();
}

FReply UItemListElement::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!GetClickButton() && bInteractionEnabled && ItemData.bIsEnabled)
	{
		BroadcastSelected();
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UItemListElement::SetItemData(const FShopItemViewData& InData)
{
	ItemData = InData;

	if (TXT_ItemName)
	{
		TXT_ItemName->SetText(InData.DisplayName);
	}

	if (TXT_ItemPrice)
	{
		TXT_ItemPrice->SetText(FText::AsNumber(InData.Price));
	}

	if (TXT_ItemQty)
	{
		TXT_ItemQty->SetText(FText::Format(LOCTEXT("ItemStockFormat", "x{0}"), FText::AsNumber(InData.StockQuantity)));
	}

	if (TXT_ItemMeta)
	{
		TXT_ItemMeta->SetText(InData.MetaText);
	}

	if (IMG_ItemThumb_Placeholder && InData.ListIconBrush.GetResourceObject())
	{
		IMG_ItemThumb_Placeholder->SetBrush(InData.ListIconBrush);
	}

	SetInteractionEnabled(InData.bIsEnabled);
}

void UItemListElement::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;

	if (BG_ItemRow_0_Outline)
	{
		BG_ItemRow_0_Outline->SetVisibility(bSelected ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UItemListElement::SetInteractionEnabled(bool bEnabled)
{
	bInteractionEnabled = bEnabled;
	const bool bCanInteract = bInteractionEnabled && ItemData.bIsEnabled;
	SetIsEnabled(bCanInteract);

	if (UButton* ClickButton = GetClickButton())
	{
		ClickButton->SetIsEnabled(bCanInteract);
	}
}

FName UItemListElement::GetItemId() const
{
	return ItemData.ItemId;
}

void UItemListElement::ResetElement()
{
	ItemData = FShopItemViewData();
	bSelected = false;
	bInteractionEnabled = true;

	if (TXT_ItemName)
	{
		TXT_ItemName->SetText(FText::GetEmpty());
	}

	if (TXT_ItemPrice)
	{
		TXT_ItemPrice->SetText(FText::GetEmpty());
	}

	if (TXT_ItemQty)
	{
		TXT_ItemQty->SetText(FText::GetEmpty());
	}

	if (TXT_ItemMeta)
	{
		TXT_ItemMeta->SetText(FText::GetEmpty());
	}

	if (IMG_ItemThumb_Placeholder)
	{
		IMG_ItemThumb_Placeholder->SetBrush(FSlateBrush());
	}

	SetSelected(false);
	SetInteractionEnabled(false);
}

void UItemListElement::HandleClicked()
{
	BroadcastSelected();
}

void UItemListElement::BroadcastSelected()
{
	if (bInteractionEnabled && ItemData.bIsEnabled && !ItemData.ItemId.IsNone())
	{
		OnItemSelected.Broadcast(ItemData.ItemId);
	}
}

UButton* UItemListElement::GetClickButton() const
{
	if (BTN_ItemRow)
	{
		return BTN_ItemRow.Get();
	}

	if (BTN_ItemRow_0)
	{
		return BTN_ItemRow_0.Get();
	}

	UButton* FoundButton = nullptr;
	if (WidgetTree)
	{
		WidgetTree->ForEachWidget([&FoundButton](UWidget* Widget)
		{
			if (!FoundButton)
			{
				FoundButton = Cast<UButton>(Widget);
			}
		});
	}

	return FoundButton;
}

#undef LOCTEXT_NAMESPACE
