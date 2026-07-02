// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Shop/EntryData/ShopTabEntryData.h"

void UShopTabEntryData::Initialize(const FShopTabViewData& InViewData, bool bInSelected)
{
	ViewData = InViewData;
	bSelected = bInSelected;
}
