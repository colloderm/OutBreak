// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Shop/ShopCategoryElement.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"

void UShopCategoryElement::NativeConstruct()
{
	Super::NativeConstruct();

	if (UButton* ClickButton = GetClickButton())
	{
		ClickButton->OnClicked.RemoveDynamic(this, &UShopCategoryElement::HandleClicked);
		ClickButton->OnClicked.AddDynamic(this, &UShopCategoryElement::HandleClicked);
	}

	SetSelected(bSelected);
	SetInteractionEnabled(bInteractionEnabled);
}

void UShopCategoryElement::NativeDestruct()
{
	if (UButton* ClickButton = GetClickButton())
	{
		ClickButton->OnClicked.RemoveDynamic(this, &UShopCategoryElement::HandleClicked);
	}

	Super::NativeDestruct();
}

FReply UShopCategoryElement::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!GetClickButton() && bInteractionEnabled && CategoryData.bIsEnabled)
	{
		BroadcastSelected();
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UShopCategoryElement::SetCategoryData(const FShopCategoryViewData& InData)
{
	CategoryData = InData;

	if (TXT_Category_Name)
	{
		TXT_Category_Name->SetText(InData.DisplayName);
	}

	if (TXT_Category_Count)
	{
		TXT_Category_Count->SetText(FText::AsNumber(InData.ItemCount));
	}

	SetInteractionEnabled(InData.bIsEnabled);
}

void UShopCategoryElement::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;

	if (BG_Category_All_Selected)
	{
		BG_Category_All_Selected->SetVisibility(bSelected ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UShopCategoryElement::SetInteractionEnabled(bool bEnabled)
{
	bInteractionEnabled = bEnabled;
	const bool bCanInteract = bInteractionEnabled && CategoryData.bIsEnabled;
	SetIsEnabled(bCanInteract);

	if (UButton* ClickButton = GetClickButton())
	{
		ClickButton->SetIsEnabled(bCanInteract);
	}
}

FName UShopCategoryElement::GetCategoryId() const
{
	return CategoryData.CategoryId;
}

void UShopCategoryElement::HandleClicked()
{
	BroadcastSelected();
}

void UShopCategoryElement::BroadcastSelected()
{
	if (bInteractionEnabled && CategoryData.bIsEnabled && !CategoryData.CategoryId.IsNone())
	{
		OnCategorySelected.Broadcast(CategoryData.CategoryId);
	}
}

UButton* UShopCategoryElement::GetClickButton() const
{
	if (BTN_Category_All)
	{
		return BTN_Category_All.Get();
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
