// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Shop/ShopkeeperPortrait.h"

#include "Components/TextBlock.h"
#include "Components/Border.h"

void UShopkeeperPortrait::NativeConstruct()
{
	Super::NativeConstruct();
}

void UShopkeeperPortrait::SetShopkeeperData(const FShopkeeperViewData& InData)
{
	if (TXT_Role)
	{
		TXT_Role->SetText(InData.RoleText);
	}

	if (TXT_ShopkeeperName)
	{
		TXT_ShopkeeperName->SetText(InData.DisplayName);
	}

	if (TXT_ShopkeeperSubtitle)
	{
		TXT_ShopkeeperSubtitle->SetText(InData.Subtitle);
	}

	if (IMG_ShopkeeperPortrait_Placeholder && InData.PortraitBrush.GetResourceObject())
	{
		IMG_ShopkeeperPortrait_Placeholder->SetBrush(InData.PortraitBrush);
	}
}

void UShopkeeperPortrait::ClearShopkeeperData()
{
	if (TXT_Role)
	{
		TXT_Role->SetText(FText::GetEmpty());
	}

	if (TXT_ShopkeeperName)
	{
		TXT_ShopkeeperName->SetText(FText::GetEmpty());
	}

	if (TXT_ShopkeeperSubtitle)
	{
		TXT_ShopkeeperSubtitle->SetText(FText::GetEmpty());
	}

	if (IMG_ShopkeeperPortrait_Placeholder)
	{
		IMG_ShopkeeperPortrait_Placeholder->SetBrush(FSlateBrush());
	}
}
