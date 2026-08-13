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
#include "Weapon/Data/OBWeaponCatalog.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Data/OBGameDataSubsystem.h"
#include "Item/OBItemRegistry.h"

void ULoadout::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (UOBLoadoutSubsystem* LS = GetLoadout())
	{
		LS->GrantStarterIfEmpty(WeaponCatalog);   // 무일푼 복구

		// 상점 구매/판매·정산처럼 이 위젯 밖에서 바뀐 것도 반영해야 한다.
		LS->OnLoadoutChanged.RemoveAll(this);
		LS->OnLoadoutChanged.AddUObject(this, &ULoadout::HandleLoadoutChanged);
	}
	
	BindCardClicks();
	RebuildStash();
	RebuildSlots();
	ShowDefaultStats(); // 열 때 장착된 무기 스탯을 기본 표시
}

void ULoadout::NativeDestruct()
{
	if (UOBLoadoutSubsystem* LS = GetLoadout())
	{
		LS->OnLoadoutChanged.RemoveAll(this);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimer);
	}
	Super::NativeDestruct();
}

void ULoadout::HandleLoadoutChanged()
{
	// 통지 시점이 WeaponElement의 클릭 델리게이트 브로드캐스트 도중일 수 있다.
	// 그 자리에서 ClearChildren()을 하면 방금 클릭된 엘리먼트를 파괴하게 된다.
	// 다음 틱으로 밀면 안전하고, 한 프레임에 여러 번 와도 한 번만 다시 그린다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RefreshTimer, this, &ULoadout::RefreshAll, 0.f, false);
	}
}

void ULoadout::RefreshAll()
{
	RebuildStash();
	RebuildSlots();
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

	for (EOBWeaponSlot WeaponSlot : { EOBWeaponSlot::Primary, EOBWeaponSlot::Secondary, EOBWeaponSlot::Melee })
	{
		if (TSubclassOf<AOBWeaponBase> Loaded = LS->GetSlotWeaponClass(WeaponSlot))
		{
			ShowStats(Loaded); // 첫 장착 무기 스탯 표시
			return;
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

	if (TSubclassOf<AOBWeaponBase> Loaded = LS->GetSlotWeaponClass(WeaponSlot))
	{
		ShowStats(Loaded);   // 그 슬롯 무기 스탯으로 하단 갱신
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
		FText Cat  = UEnum::GetDisplayValueAsText(Data->WeaponCategory);
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

	auto FillSlot = [&](EOBWeaponSlot WeaponSlot)
	{
		FText Name = FText::FromString(TEXT("미선택"));
		FText Category = FText::GetEmpty();
		FText Desc = FText::GetEmpty();
		UTexture2D* Icon = nullptr;

		if (TSubclassOf<AOBWeaponBase> Loaded = LS->GetSlotWeaponClass(WeaponSlot))
		{
			const AOBWeaponBase* CDO = Loaded->GetDefaultObject<AOBWeaponBase>();
			if (const UOBWeaponData* Data = CDO ? CDO->GetWeaponData() : nullptr)
			{
				Name	 = Data->DisplayName;
				Category = UEnum::GetDisplayValueAsText(Data->WeaponCategory);
				Desc     = Data->Description;
				Icon	 = Data->WeaponIcon;
			}
		}
		LoadoutSelectionView->SetCardInfo(Name, WeaponSlot, Category, Desc, Icon);
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
	LS->EquipFromStash(WeaponClass); // 성공하면 OnLoadoutChanged가 다시 그린다
}

void ULoadout::ShowStats(TSubclassOf<AOBWeaponBase> WeaponClass)
{
	if (!LoadoutSelectionView || !WeaponClass) return;

	const UOBGameDataSubsystem* GameData = UOBGameDataSubsystem::Get();
	const FGameplayTag ItemTag = GameData
		? GameData->FindTagForWeaponClass(WeaponClass.Get())
		: FGameplayTag();
	const FOBWeaponDefinitionRow* D = GameData
		? GameData->FindWeapon(ItemTag)
		: nullptr;
	if (!D) return;

	auto Norm = [](float V, float Max) { return Max > 0.f ? FMath::Clamp(V / Max, 0.f, 1.f) : 0.f; };

	FText Name;
	UTexture2D* Icon = nullptr;
	UOBItemRegistry::GetItemDisplay(ItemTag, Name, Icon);
	const float Damage = Norm(D->Common.BaseDamage, MaxDamage);
	const float RawFireRate = D->WeaponType == EOBWeaponType::Ranged
		? D->Ranged.RoundsPerMinute
		: (D->Melee.AttackDuration > 0.f ? 60.f / D->Melee.AttackDuration : 0.f);
	const float FireRate = Norm(RawFireRate, MaxRPM);
	const float Accuracy = D->WeaponType == EOBWeaponType::Ranged
		? MaxSpread / (MaxSpread + FMath::Max(D->Ranged.BaseSpreadDegrees, 0.f))
		: FMath::Clamp(D->Melee.ArcDegrees / 180.f, 0.f, 1.f);
	const float Recoil = D->WeaponType == EOBWeaponType::Ranged
		? Norm(D->Ranged.VerticalRecoil + D->Ranged.HorizontalRecoil, MaxRecoil)
		: 0.f;
	const float Mobility = FMath::GetMappedRangeValueClamped(
		FVector2f(MinMobilityMultiplier, 1.f), FVector2f(0.f, 1.f), D->Common.MobilityMultiplier);
	FText Ammo = D->WeaponType == EOBWeaponType::Ranged
		? FText::FromString(FString::Printf(TEXT("%d / %d"), D->Ranged.MagazineSize, D->Ranged.MaxReserveAmmo))
		: FText::FromString(TEXT("Melee"));

	LoadoutSelectionView->SetStatView(Name, Damage, FireRate, Accuracy, Recoil, Mobility, Ammo);
}
