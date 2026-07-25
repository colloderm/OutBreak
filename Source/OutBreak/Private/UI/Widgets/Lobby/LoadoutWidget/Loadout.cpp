// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/Lobby/LoadoutWidget/Loadout.h"

#include "UI/Widgets/Lobby/LoadoutWidget/LoadoutSelectionList.h"
#include "UI/Widgets/Lobby/LoadoutWidget/LoadoutSelectionView.h"
#include "UI/Widgets/Lobby/LoadoutWidget/WeaponElement.h"
#include "UI/Widgets/Lobby/LoadoutWidget/LoadoutCardElement.h"
#include "LoadOut/OBLoadoutSubsystem.h"
#include "LoadOut/OBLoadoutTypes.h"
#include "Weapon/OBWeaponBase.h"
#include "Weapon/Data/OBWeaponData.h"
#include "Engine/GameInstance.h"

namespace
{
	FText SlotToCategory(EOBWeaponSlot Slot)
	{
		switch (Slot)
		{
		case EOBWeaponSlot::Primary: 
			return FText::FromString(TEXT("주무기"));
		case EOBWeaponSlot::Secondary: 
			return FText::FromString(TEXT("보조무기"));
		case EOBWeaponSlot::Melee: 
			return FText::FromString(TEXT("근접"));
		default: 
			return FText::GetEmpty();
		}
	}
}

void ULoadout::NativeConstruct()
{
	Super::NativeConstruct();
	
	BindCardClicks();
	RebuildStash();
	RebuildSlots();
	ShowDefaultStats(); // 열 때 장착된 무기 스탯을 기본 표시
}

UOBLoadoutSubsystem* ULoadout::GetLoadout() const
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		return GI->GetSubsystem<UOBLoadoutSubsystem>();
	}
	
	return nullptr;
}

void ULoadout::ShowDefaultStats()
{
	UOBLoadoutSubsystem* LS = GetLoadout();
	if (!LS) return;

	const FOBLoadout& LO = LS->GetLoadout();
	for (EOBWeaponSlot WeaponSlot : { EOBWeaponSlot::Primary, EOBWeaponSlot::Secondary, EOBWeaponSlot::Melee })
	{
		if (const TSoftClassPtr<AOBWeaponBase>* Soft = LO.SlotWeapons.Find(WeaponSlot))
		{
			if (UClass* Loaded = Soft->LoadSynchronous())
			{
				ShowStats(Loaded); // 첫 장착 무기 스탯 표시
				return;
			}
		}
	}
}

void ULoadout::BindCardClicks()
{
	if (!LoadoutSelectionView) return;
	
	auto Bind = [&](ULoadoutCardElement* Card)
	{
		if (!Card) return;
		Card->OnClicked.RemoveAll(this);
		Card->OnClicked.AddDynamic(this, &ULoadout::HandleCardClicked);
	};
	
	Bind(LoadoutSelectionView->LoadoutCardElement_Primary);
	Bind(LoadoutSelectionView->LoadoutCardElement_Secondary);
	Bind(LoadoutSelectionView->LoadoutCardElement_Melee);
}

void ULoadout::HandleCardClicked(EOBWeaponSlot WeaponSlot)
{
	UOBLoadoutSubsystem* LS = GetLoadout();
	if (!LS) return;

	const FOBLoadout& LO = LS->GetLoadout();
	if (const TSoftClassPtr<AOBWeaponBase>* Soft = LO.SlotWeapons.Find(WeaponSlot))
	{
		if (UClass* Loaded = Soft->LoadSynchronous())
		{
			ShowStats(Loaded);   // 그 슬롯 무기 스탯으로 하단 갱신
		}
	}
}

void ULoadout::RebuildStash()
{
	UOBLoadoutSubsystem* LS = GetLoadout();
	if (!LS || !LoadoutSelectionList) return;

	LoadoutSelectionList->ClearElements();

	APlayerController* PC = GetOwningPlayer();
	for (const TSubclassOf<AOBWeaponBase>& WClass : LS->GetOwnedClasses())
	{
		if (!WClass) continue;
		const AOBWeaponBase* CDO = WClass->GetDefaultObject<AOBWeaponBase>();
		const UOBWeaponData* Data = CDO ? CDO->GetWeaponData() : nullptr;
		if (!Data) continue;

		FText Name = Data->DisplayName;
		FText Cat  = SlotToCategory(Data->WeaponSlot);
		if (UWeaponElement* Elem = LoadoutSelectionList->AddWeaponElement(PC, WClass, Data->WeaponIcon, Name, Cat))
		{
			Elem->OnClicked.AddDynamic(this, &ULoadout::HandleWeaponClicked);
		}
	}
}

void ULoadout::RebuildSlots()
{
	UOBLoadoutSubsystem* LS = GetLoadout();
	if (!LS || !LoadoutSelectionView) return;

	const FOBLoadout& LO = LS->GetLoadout();

	auto FillSlot = [&](EOBWeaponSlot WeaponSlot)
	{
		FText Name = FText::FromString(TEXT("미선택"));
		FString Desc;
		UTexture2D* Icon = nullptr;
		if (const TSoftClassPtr<AOBWeaponBase>* Soft = LO.SlotWeapons.Find(WeaponSlot))
		{
			if (UClass* Loaded = Soft->LoadSynchronous())
			{
				const AOBWeaponBase* CDO = Loaded->GetDefaultObject<AOBWeaponBase>();
				const UOBWeaponData* Data = CDO ? CDO->GetWeaponData() : nullptr;
				if (Data) 
				{ 
					Name = Data->DisplayName; 
					Icon = Data->WeaponIcon; 
				}
			}
		}
		LoadoutSelectionView->SetCardInfo(Name, WeaponSlot, Desc, Icon);
	};

	FillSlot(EOBWeaponSlot::Primary);
	FillSlot(EOBWeaponSlot::Secondary);
	FillSlot(EOBWeaponSlot::Melee);
}

void ULoadout::HandleWeaponClicked(TSubclassOf<AOBWeaponBase> WeaponClass)
{
	UOBLoadoutSubsystem* LS = GetLoadout();
	if (!LS || !WeaponClass) return;

	ShowStats(WeaponClass);          // 클릭 무기 스탯 표시
	LS->EquipFromStash(WeaponClass); // 창고→슬롯(이전 슬롯 무기는 창고로)
	RebuildStash();
	RebuildSlots();
}

void ULoadout::ShowStats(TSubclassOf<AOBWeaponBase> WeaponClass)
{
	if (!LoadoutSelectionView || !WeaponClass) return;

	const AOBWeaponBase* CDO = WeaponClass->GetDefaultObject<AOBWeaponBase>();
	const UOBWeaponData* D = CDO ? CDO->GetWeaponData() : nullptr;
	if (!D) return;

	auto Norm = [](float V, float Max) { return Max > 0.f ? FMath::Clamp(V / Max, 0.f, 1.f) : 0.f; };

	FText Name = D->DisplayName;
	const float Damage   = Norm(D->BaseDamage, MaxDamage);
	const float FireRate = Norm(D->RoundsPerMinute, MaxRPM);
	const float Accuracy = FMath::Clamp(1.f - (D->BaseSpreadDegrees / MaxSpread), 0.f, 1.f);
	const float Recoil   = Norm(D->VerticalRecoil + D->HorizontalRecoil, MaxRecoil);
	const float Mobility = 0.5f;   // WeaponData에 이동 스탯 추가 시 교체
	FText Ammo = FText::FromString(FString::Printf(TEXT("%d / %d"), D->MagazineSize, D->MaxReserveAmmo));

	LoadoutSelectionView->SetStatView(Name, Damage, FireRate, Accuracy, Recoil, Mobility, Ammo);
}
