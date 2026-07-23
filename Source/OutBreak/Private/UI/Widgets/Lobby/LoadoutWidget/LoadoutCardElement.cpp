// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Widgets/Lobby/LoadoutWidget/LoadoutCardElement.h"

#include "Components/TextBlock.h"

void ULoadoutCardElement::SetLoadoutCard(FText& inWeaponName, EOBWeaponSlot inType, FString& inDesc)
{
	SetWeaponName(inWeaponName);
	SetWeaponType(inType);
	SetWeaponDesc(inDesc);
}

void ULoadoutCardElement::SetWeaponName(FText& inWeaponName)
{
	TXT_WeaponName->SetText(inWeaponName);
}

void ULoadoutCardElement::SetWeaponType(EOBWeaponSlot inType)
{
	const FText DisplayName = UEnum::GetDisplayValueAsText(inType);
	TXT_WeaponType->SetText(DisplayName);
}

void ULoadoutCardElement::SetWeaponDesc(FString& inDesc)
{
	TXT_WeaponDesc->SetText(FText::FromString(inDesc));
}

void ULoadoutCardElement::NativeConstruct()
{
	Super::NativeConstruct();
}
