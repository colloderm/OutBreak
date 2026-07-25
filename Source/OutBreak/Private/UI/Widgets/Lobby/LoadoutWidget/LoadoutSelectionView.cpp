// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Widgets/Lobby/LoadoutWidget/LoadoutSelectionView.h"

#include "Components/VerticalBox.h"
#include "UI/Widgets/Lobby/LoadoutWidget/WeaponStatView.h"
#include "UI/Widgets/Lobby/LoadoutWidget/LoadoutCardElement.h"

void ULoadoutSelectionView::SetCardInfo(FText& inName, EOBWeaponSlot inType, FString& inDesc, UTexture2D* inIcon)
{
	ULoadoutCardElement* CardPtr = nullptr;
	switch (inType)
	{
		case EOBWeaponSlot::Primary:
			{
				CardPtr = LoadoutCardElement_Primary;
				break;
			}
		case EOBWeaponSlot::Secondary:
			{
				CardPtr = LoadoutCardElement_Secondary;
				break;
			}
		case EOBWeaponSlot::Melee:
			{
				CardPtr = LoadoutCardElement_Melee;
				break;
			}
		default:
			{
				UE_LOG(LogTemp, Error, TEXT("%s::%s: Loadout Slot Type is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
				return;
			}
	}
	CardPtr->SetLoadoutCard(inName, inType, inDesc, inIcon);
}

void ULoadoutSelectionView::SetStatView(FText& inSelectedWeaponName, float inDamage, float inFireRate, float inAccuracy,
	float inRecoil, float inMobility, FText& inAmmoInfo)
{
	WeaponStatView->SetViewData(inSelectedWeaponName, inDamage, inFireRate, inAccuracy, inRecoil, inMobility, inAmmoInfo);
}

void ULoadoutSelectionView::NativeConstruct()
{
	Super::NativeConstruct();
}
