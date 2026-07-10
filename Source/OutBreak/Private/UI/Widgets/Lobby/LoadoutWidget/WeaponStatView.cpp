// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Widgets/Lobby/LoadoutWidget/WeaponStatView.h"

#include "Components/TextBlock.h"
#include "UI/Widgets/Lobby/LoadoutWidget/WeaponStatElement.h"


void UWeaponStatView::SetViewData(FText& inSelectedWeaponName, float inDamage, float inFireRate, float inAccuracy,
	float inRecoil, float inMobility, FText& inAmmoInfo)
{
	TXT_SelectedWeaponName->SetText(inSelectedWeaponName);
	TXT_AmmoCount->SetText(inAmmoInfo);
	SetStat(EStatTypes::Damage, inDamage);
	SetStat(EStatTypes::FireRate, inFireRate);
	SetStat(EStatTypes::Accuracy, inAccuracy);
	SetStat(EStatTypes::Recoil, inRecoil);
	SetStat(EStatTypes::Mobility, inMobility);
}

void UWeaponStatView::SetStat(EStatTypes inStatType, float inPercent)
{
	UWeaponStatElement* StatElement = nullptr;
	switch (inStatType)
	{
		case EStatTypes::Damage:
			{
				StatElement = Stat_Damage;
				break;
			}
		
		case EStatTypes::FireRate:
			{
				StatElement = Stat_FireRate;
				break;
			}
		
		case EStatTypes::Accuracy:
			{
				StatElement = Stat_Accuracy;
				break;
			}
		
		case EStatTypes::Recoil:
			{
				StatElement = Stat_Recoil;
				break;
			}
		
		case EStatTypes::Mobility:
			{
				StatElement = Stat_Mobility;
				break;
			}
		default:
			{
				UE_LOG(LogTemp, Error, TEXT("%s::%s: Stat Type is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
				return;
			}
	}
	FText StatName = UEnum::GetDisplayValueAsText(inStatType);
	StatElement->SetStat(StatName, inPercent);
}

void UWeaponStatView::NativeConstruct()
{
	Super::NativeConstruct();
}
