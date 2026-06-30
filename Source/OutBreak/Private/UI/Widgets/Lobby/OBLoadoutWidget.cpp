// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/Lobby/OBLoadoutWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Weapon/OBWeaponBase.h"

UOBWeaponData* UOBLoadoutWidget::GetData(TSubclassOf<AOBWeaponBase> W) const
{
	if (!W) return nullptr;
	
	AOBWeaponBase* CDO = W->GetDefaultObject<AOBWeaponBase>();
	return CDO ? CDO->GetWeaponData() : nullptr;
}

void UOBLoadoutWidget::RefreshLoadout(const TArray<TSubclassOf<AOBWeaponBase>>& Selected)
{
	auto Fill = [&](UImage* Icon, UTextBlock* Name, EOBWeaponSlot WeaponSlot)
	{
		for (const TSubclassOf<AOBWeaponBase>& W : Selected)
		{
			UOBWeaponData* D = GetData(W);
			if (D && D->WeaponSlot == WeaponSlot)
			{
				if (Name) Name->SetText(D->DisplayName);
				if (Icon && D->WeaponIcon) Icon->SetBrushFromTexture(D->WeaponIcon);
				return;
			}
		}
		if (Name) Name->SetText(FText::FromString(TEXT("미선택")));
	};
	
	Fill(IconPrimary,   NamePrimary,   EOBWeaponSlot::Primary);
	Fill(IconSecondary, NameSecondary, EOBWeaponSlot::Secondary);
	Fill(IconMelee,     NameMelee,     EOBWeaponSlot::Melee);
}

void UOBLoadoutWidget::ShowStats(TSubclassOf<AOBWeaponBase> WeaponClass)
{
	UOBWeaponData* D = GetData(WeaponClass);
	if (!D) return;
	
	auto Norm = [](float V, float Max)
	{
		return Max > 0.f ? FMath::Clamp(V / Max, 0.f, 1.f) : 0.f;
	};

	if (StatName)    
		StatName->SetText(D->DisplayName);
	
	if (BarDamage)   
		BarDamage->SetPercent(Norm(D->BaseDamage, MaxDamage));
	
	if (BarFireRate) 
		BarFireRate->SetPercent(Norm(D->RoundsPerMinute, MaxRPM));
	
	if (BarAccuracy) 
		BarAccuracy->SetPercent(FMath::Clamp(1.f - (D->BaseSpreadDegrees / MaxSpread), 0.f, 1.f));
	
	if (BarRecoil)   
		BarRecoil->SetPercent(Norm(D->VerticalRecoil + D->HorizontalRecoil, MaxRecoil));
	
	if (BarMobility) 
		BarMobility->SetPercent(0.5f);   // WeaponData에 이동 스탯 추가 시 교체
	
	if (AmmoText)    
		AmmoText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), D->MagazineSize, D->MaxReserveAmmo)));
}