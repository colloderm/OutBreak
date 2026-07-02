// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Shop/EntryData/ShopCategoryEntryData.h"

void UShopCategoryEntryData::Initialize(const FShopCategoryViewData& InViewData, bool bInSelected)
{
	ViewData = InViewData;
	bSelected = bInSelected;
}
