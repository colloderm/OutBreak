// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Shop/EntryData/ShopItemEntryData.h"

void UShopItemEntryData::Initialize(const FShopItemSummaryViewData& InViewData, bool bInSelected)
{
	ViewData = InViewData;
	bSelected = bInSelected;
}
