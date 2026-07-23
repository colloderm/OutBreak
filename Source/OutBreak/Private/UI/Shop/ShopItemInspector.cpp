// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Shop/ShopItemInspector.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "UI/Shop/ItemStatElement.h"
#include "UI/Shop/KeyBindableBtn.h"

#define LOCTEXT_NAMESPACE "ShopItemInspector"

void UShopItemInspector::NativeConstruct()
{
	Super::NativeConstruct();

	ResolveItemStatElementClass();

	for (UKeyBindableBtn* Button : GetActionButtons())
	{
		if (Button)
		{
			Button->OnActionTriggered.RemoveDynamic(this, &UShopItemInspector::HandleActionTriggered);
			Button->OnActionTriggered.AddDynamic(this, &UShopItemInspector::HandleActionTriggered);
		}
	}
}

void UShopItemInspector::NativeDestruct()
{
	for (UKeyBindableBtn* Button : GetActionButtons())
	{
		if (Button)
		{
			Button->OnActionTriggered.RemoveDynamic(this, &UShopItemInspector::HandleActionTriggered);
		}
	}

	ClearStatElements();
	Super::NativeDestruct();
}

void UShopItemInspector::SetItemData(const FShopItemViewData& InData)
{
	ItemData = InData;

	if (TXT_InspectorTitle)
	{
		TXT_InspectorTitle->SetText(InData.DisplayName);
	}

	if (TXT_InspectorQty)
	{
		TXT_InspectorQty->SetText(FText::Format(LOCTEXT("OwnedQuantityFormat", "Owned: {0}"), FText::AsNumber(InData.OwnedQuantity)));
	}

	if (TXT_InspectorMeta)
	{
		TXT_InspectorMeta->SetText(InData.MetaText);
	}

	if (IMG_InspectorPreview_Placeholder && InData.DetailImageBrush.GetResourceObject())
	{
		IMG_InspectorPreview_Placeholder->SetBrush(InData.DetailImageBrush);
	}

	if (TXT_InspectorDescription)
	{
		TXT_InspectorDescription->SetText(InData.Description);
	}

	if (TXT_InspectorPriceValue)
	{
		TXT_InspectorPriceValue->SetText(FText::AsNumber(InData.Price));
	}

	RebuildStats(InData.Stats);
	RefreshActionStates(InData.Actions);
}

void UShopItemInspector::ClearItemData()
{
	ItemData = FShopItemViewData();

	if (TXT_InspectorTitle)
	{
		TXT_InspectorTitle->SetText(FText::GetEmpty());
	}

	if (TXT_InspectorQty)
	{
		TXT_InspectorQty->SetText(FText::GetEmpty());
	}

	if (TXT_InspectorMeta)
	{
		TXT_InspectorMeta->SetText(FText::GetEmpty());
	}

	if (IMG_InspectorPreview_Placeholder)
	{
		IMG_InspectorPreview_Placeholder->SetBrush(FSlateBrush());
	}

	if (TXT_InspectorDescription)
	{
		TXT_InspectorDescription->SetText(FText::GetEmpty());
	}

	if (TXT_InspectorPriceValue)
	{
		TXT_InspectorPriceValue->SetText(FText::GetEmpty());
	}

	ClearStatElements();

	for (UKeyBindableBtn* Button : GetActionButtons())
	{
		if (Button)
		{
			Button->ClearActionData();
		}
	}
}

void UShopItemInspector::RefreshActionStates(const TArray<FShopActionViewData>& InActions)
{
	TArray<UKeyBindableBtn*> ActionButtons = GetActionButtons();

	for (int32 Index = 0; Index < ActionButtons.Num(); ++Index)
	{
		UKeyBindableBtn* Button = ActionButtons[Index];
		if (!Button)
		{
			continue;
		}

		if (InActions.IsValidIndex(Index))
		{
			Button->SetActionData(InActions[Index]);
			Button->SetActionEnabled(InActions[Index].bCanExecute);
		}
		else
		{
			Button->ClearActionData();
		}
	}
}

void UShopItemInspector::HandleActionTriggered(FName ActionId)
{
	if (!ItemData.ItemId.IsNone() && !ActionId.IsNone())
	{
		OnActionTriggered.Broadcast(ActionId);
	}
}

void UShopItemInspector::ResolveItemStatElementClass()
{
	if (ItemStatElementClass || !VBX_ItemStatList)
	{
		return;
	}

	for (int32 Index = 0; Index < VBX_ItemStatList->GetChildrenCount(); ++Index)
	{
		if (UItemStatElement* ExistingElement = Cast<UItemStatElement>(VBX_ItemStatList->GetChildAt(Index)))
		{
			ItemStatElementClass = ExistingElement->GetClass();
			return;
		}
	}
}

void UShopItemInspector::ClearStatElements()
{
	StatElements.Reset();

	if (VBX_ItemStatList)
	{
		VBX_ItemStatList->ClearChildren();
	}
}

void UShopItemInspector::RebuildStats(const TArray<FShopItemStatViewData>& InStats)
{
	ResolveItemStatElementClass();
	ClearStatElements();

	if (!VBX_ItemStatList)
	{
		ensureMsgf(false, TEXT("UShopItemInspector requires VBX_ItemStatList."));
		return;
	}

	if (!ItemStatElementClass)
	{
		ensureMsgf(false, TEXT("UShopItemInspector requires ItemStatElementClass or a template child in VBX_ItemStatList."));
		return;
	}

	TArray<FShopItemStatViewData> SortedStats = InStats;
	SortedStats.Sort([](const FShopItemStatViewData& Left, const FShopItemStatViewData& Right)
	{
		if (Left.SortOrder == Right.SortOrder)
		{
			return Left.StatId.LexicalLess(Right.StatId);
		}
		return Left.SortOrder < Right.SortOrder;
	});

	for (const FShopItemStatViewData& Stat : SortedStats)
	{
		UItemStatElement* Element = CreateWidget<UItemStatElement>(this, ItemStatElementClass);
		if (!Element)
		{
			continue;
		}

		Element->SetStatData(Stat);
		VBX_ItemStatList->AddChild(Element);
		StatElements.Add(Element);
	}
}

TArray<UKeyBindableBtn*> UShopItemInspector::GetActionButtons() const
{
	TArray<UKeyBindableBtn*> Buttons;
	Buttons.Reserve(2);
	Buttons.Add(BTN_0.Get());
	Buttons.Add(BTN_1.Get());
	return Buttons;
}

#undef LOCTEXT_NAMESPACE
