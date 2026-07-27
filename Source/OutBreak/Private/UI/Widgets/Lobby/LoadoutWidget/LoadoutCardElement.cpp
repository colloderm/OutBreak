// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/Lobby/LoadoutWidget/LoadoutCardElement.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

void ULoadoutCardElement::SetLoadoutCard(FText& inWeaponName, EOBWeaponSlot inSlot, FText& inCategory, FText& inDesc, UTexture2D* inIcon)
{
	CardSlot = inSlot;
	SetWeaponName(inWeaponName);
	SetCategory(inCategory);
	SetWeaponDesc(inDesc);
	SetIcon(inIcon);
}

void ULoadoutCardElement::SetWeaponName(FText& inWeaponName)
{
	if (TXT_WeaponName)
		TXT_WeaponName->SetText(inWeaponName);
}

void ULoadoutCardElement::SetCategory(FText& inCategory)
{
	if (TXT_WeaponType) 
		TXT_WeaponType->SetText(inCategory);
}

void ULoadoutCardElement::SetWeaponDesc(FText& inDesc)
{
	if (TXT_WeaponDesc) 
		TXT_WeaponDesc->SetText(inDesc);
}

void ULoadoutCardElement::SetIcon(UTexture2D* inIcon)
{
	if (!IMG_WeaponIcon) return;
	if (inIcon)
	{
		IMG_WeaponIcon->SetBrushFromTexture(inIcon);
		IMG_WeaponIcon->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		IMG_WeaponIcon->SetVisibility(ESlateVisibility::Hidden); // 미선택 슬롯
	}
}

FReply ULoadoutCardElement::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	OnClicked.Broadcast(CardSlot);
	
	return FReply::Handled();
}

void ULoadoutCardElement::HandleButtonClicked()
{
	OnClicked.Broadcast(CardSlot);
}

void ULoadoutCardElement::NativeConstruct()
{
	Super::NativeConstruct();
	
	SetVisibility(ESlateVisibility::Visible);

	if (WidgetTree)
	{
		WidgetTree->ForEachWidget([this](UWidget* W)
		{
			if (UButton* Btn = Cast<UButton>(W))
			{
				Btn->OnClicked.RemoveDynamic(this, &ULoadoutCardElement::HandleButtonClicked);
				Btn->OnClicked.AddDynamic(this, &ULoadoutCardElement::HandleButtonClicked);
			}
		});
	}
}
