// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Widgets/Lobby/LoadoutWidget/WeaponElement.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

void UWeaponElement::SetElementData(UTexture2D* inIcon, FText& inName, FText& inCategory)
{
	SetIcon(inIcon);
	SetWeaponName(inName);
	SetCategory(inCategory);
}

void UWeaponElement::SetIcon(UTexture2D* inIcon)
{
	IMG_WeaponIcon->SetBrushFromTexture(inIcon);
}

void UWeaponElement::SetWeaponName(FText& inName)
{
	TXT_WeaponName->SetText(inName);
}

void UWeaponElement::SetCategory(FText& inCategory)
{
	TXT_Category->SetText(inCategory);
}

void UWeaponElement::NativeConstruct()
{
	Super::NativeConstruct();
}
