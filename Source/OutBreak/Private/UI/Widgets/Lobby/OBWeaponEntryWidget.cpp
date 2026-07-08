// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/Lobby/OBWeaponEntryWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Weapon/OBWeaponBase.h"

void UOBWeaponEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (RootButton) 
		RootButton->OnClicked.AddDynamic(this, &UOBWeaponEntryWidget::HandleClicked);
}

void UOBWeaponEntryWidget::Setup(TSubclassOf<AOBWeaponBase> InWeaponClass)
{
	WeaponClass = InWeaponClass;
	if (!InWeaponClass) return;
	AOBWeaponBase* CDO = InWeaponClass->GetDefaultObject<AOBWeaponBase>();
	UOBWeaponData* Data = CDO ? CDO->GetWeaponData() : nullptr;
	if (!Data) return;

	SlotType = Data->WeaponSlot;
	if (NameText) 
		NameText->SetText(Data->DisplayName);
	
	if (IconImage && Data->WeaponIcon) 
		IconImage->SetBrushFromTexture(Data->WeaponIcon);
	
	if (CheckImage) 
		CheckImage->SetVisibility(ESlateVisibility::Hidden);
}

void UOBWeaponEntryWidget::SetSelected(bool bSelected)
{
	if (CheckImage) 
		CheckImage->SetVisibility(bSelected ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
}

void UOBWeaponEntryWidget::HandleClicked()
{
	OnEntryClicked.Broadcast(WeaponClass, SlotType);
}