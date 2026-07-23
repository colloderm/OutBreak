// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Widgets/Lobby/LoadoutWidget/WeaponStatElement.h"

#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"

void UWeaponStatElement::SetStat(FText& inName, float inPercent)
{
	SetStatName(inName);
	SetPercent(inPercent);
}

void UWeaponStatElement::SetStatName(FText& inName)
{
	TXT_StatLabel->SetText(inName);
}

void UWeaponStatElement::SetPercent(float inPercent)
{
	PB_Percentage->SetPercent(inPercent);
}

void UWeaponStatElement::NativeConstruct()
{
	Super::NativeConstruct();
	
}
