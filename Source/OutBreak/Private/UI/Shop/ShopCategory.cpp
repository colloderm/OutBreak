// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Shop/ShopCategory.h"

#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "UI/Shop/ShopCategoryElement.h"

void UShopCategory::NativeConstruct()
{
	Super::NativeConstruct();
	ResolveCategoryElementClass();
}

void UShopCategory::NativeDestruct()
{
	ClearCategoryElements();
	Super::NativeDestruct();
}

void UShopCategory::SetCategories(const TArray<FShopCategoryViewData>& InCategories)
{
	ResolveCategoryElementClass();
	ClearCategoryElements();

	Categories = InCategories;
	Categories.Sort([](const FShopCategoryViewData& Left, const FShopCategoryViewData& Right)
	{
		if (Left.SortOrder == Right.SortOrder)
		{
			return Left.CategoryId.LexicalLess(Right.CategoryId);
		}
		return Left.SortOrder < Right.SortOrder;
	});

	if (!VBX_CategoryList)
	{
		ensureMsgf(false, TEXT("UShopCategory requires VBX_CategoryList."));
		return;
	}

	if (!CategoryElementClass)
	{
		ensureMsgf(false, TEXT("UShopCategory requires CategoryElementClass or a template child in VBX_CategoryList."));
		return;
	}

	for (const FShopCategoryViewData& Category : Categories)
	{
		UShopCategoryElement* Element = CreateWidget<UShopCategoryElement>(this, CategoryElementClass);
		if (!Element)
		{
			continue;
		}

		Element->SetCategoryData(Category);
		Element->SetSelected(Category.CategoryId == SelectedCategoryId);
		Element->OnCategorySelected.RemoveDynamic(this, &UShopCategory::HandleCategoryElementSelected);
		Element->OnCategorySelected.AddDynamic(this, &UShopCategory::HandleCategoryElementSelected);

		VBX_CategoryList->AddChild(Element);
		CategoryElements.Add(Element);
	}

	if (!SelectCategory(SelectedCategoryId))
	{
		SelectedCategoryId = NAME_None;
		for (const FShopCategoryViewData& Category : Categories)
		{
			if (Category.bIsEnabled && !Category.CategoryId.IsNone())
			{
				SelectCategory(Category.CategoryId);
				break;
			}
		}
	}
}

void UShopCategory::ClearCategories()
{
	Categories.Reset();
	SelectedCategoryId = NAME_None;
	ClearCategoryElements();
}

bool UShopCategory::SelectCategory(FName CategoryId)
{
	bool bFound = false;
	bool bEnabled = false;

	for (const FShopCategoryViewData& Category : Categories)
	{
		if (Category.CategoryId == CategoryId)
		{
			bFound = true;
			bEnabled = Category.bIsEnabled;
			break;
		}
	}

	if (!bFound || !bEnabled)
	{
		return false;
	}

	SelectedCategoryId = CategoryId;
	for (UShopCategoryElement* Element : CategoryElements)
	{
		if (Element)
		{
			Element->SetSelected(Element->GetCategoryId() == SelectedCategoryId);
		}
	}

	return true;
}

FName UShopCategory::GetSelectedCategoryId() const
{
	return SelectedCategoryId;
}

void UShopCategory::SetNewStockTime(const FText& InNewStockText)
{
	if (TXT_NewStockTime)
	{
		TXT_NewStockTime->SetText(InNewStockText);
	}
}

void UShopCategory::HandleCategoryElementSelected(FName CategoryId)
{
	if (SelectCategory(CategoryId))
	{
		OnCategorySelected.Broadcast(CategoryId);
	}
}

void UShopCategory::ResolveCategoryElementClass()
{
	if (CategoryElementClass || !VBX_CategoryList)
	{
		return;
	}

	for (int32 Index = 0; Index < VBX_CategoryList->GetChildrenCount(); ++Index)
	{
		if (UShopCategoryElement* ExistingElement = Cast<UShopCategoryElement>(VBX_CategoryList->GetChildAt(Index)))
		{
			CategoryElementClass = ExistingElement->GetClass();
			return;
		}
	}
}

void UShopCategory::ClearCategoryElements()
{
	for (UShopCategoryElement* Element : CategoryElements)
	{
		if (Element)
		{
			Element->OnCategorySelected.RemoveDynamic(this, &UShopCategory::HandleCategoryElementSelected);
		}
	}

	CategoryElements.Reset();

	if (VBX_CategoryList)
	{
		VBX_CategoryList->ClearChildren();
	}
}
