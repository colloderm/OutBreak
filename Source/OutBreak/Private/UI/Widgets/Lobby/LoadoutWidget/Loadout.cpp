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
#include "Item/Data/OBItemDefinition.h"

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
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	bRefreshPending = false;
	Super::NativeDestruct();
}

void ULoadout::HandleLoadoutChanged()
{
	// 통지 시점이 WeaponElement의 클릭 델리게이트 브로드캐스트 도중일 수 있다.
	// 그 자리에서 ClearChildren()을 하면 방금 클릭된 엘리먼트를 파괴하게 된다.
	// 그래서 다음 틱으로 민다.
	// SetTimer(rate=0)은 쓰면 안 된다 — FTimerManager는 rate>0일 때만 등록하고
	// 아니면 핸들만 무효화해서 콜백이 영영 안 온다. 합치기는 플래그로 한다.
	if (bRefreshPending) return;
	bRefreshPending = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &ULoadout::RefreshAll);
		return;
	}

	// 월드가 없으면 미룰 곳이 없다. 즉시 갱신(위젯 파괴 위험도 없는 경로).
	RefreshAll();
}

void ULoadout::RefreshAll()
{
	bRefreshPending = false;
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

		FText Name, Cat, Desc;
		UTexture2D* Icon = nullptr;
		if (!GetWeaponDisplay(WClass, Name, Cat, Desc, Icon)) continue;

		if (UWeaponElement* Elem = LoadoutSelectionList->AddWeaponElement(PC, WClass, Icon, Name, Cat))
		{
			Elem->OnClicked.AddDynamic(this, &ULoadout::HandleWeaponClicked);
		}
	}
}

bool ULoadout::GetWeaponDisplay(TSubclassOf<AOBWeaponBase> WeaponClass,
	FText& OutName, FText& OutCategory, FText& OutDesc, UTexture2D*& OutIcon) const
{
	OutName = FText::GetEmpty();
	OutCategory = FText::GetEmpty();
	OutDesc = FText::GetEmpty();
	OutIcon = nullptr;
	if (!WeaponClass) return false;

	// 1) WeaponData가 기본값. 세부 분류(돌격소총/권총)는 무기 스펙의 소유라 여기에만 있다.
	const AOBWeaponBase* CDO = WeaponClass->GetDefaultObject<AOBWeaponBase>();
	if (const UOBWeaponData* WData = CDO ? CDO->GetWeaponData() : nullptr)
	{
		OutName     = WData->DisplayName;
		OutCategory = UEnum::GetDisplayValueAsText(WData->WeaponCategory);
		OutDesc     = WData->Description;
		OutIcon     = WData->WeaponIcon;
	}

	// 2) ItemTable에 값이 있으면 그쪽이 이긴다. 상점/스탯창과 같은 원본을 보게 만든다.
	const FGameplayTag ItemTag = UOBItemRegistry::FindTagForWeaponClass(WeaponClass);
	if (ItemTag.IsValid())
	{
		// 이름/아이콘은 레지스트리가 WeaponData 상속까지 처리해 준다.
		FText RegistryName;
		UTexture2D* RegistryIcon = nullptr;
		if (UOBItemRegistry::GetItemDisplay(ItemTag, RegistryName, RegistryIcon))
		{
			if (!RegistryName.IsEmpty()) OutName = RegistryName;
			if (RegistryIcon)            OutIcon = RegistryIcon;
		}

		// 설명에는 상속 헬퍼가 없다. 상점(MakeItemView)과 같은 규칙을 여기서 적용한다.
		if (const FOBItemDefinitionRow* Def = UOBItemRegistry::FindItem(ItemTag))
		{
			if (!Def->Description.IsEmpty()) OutDesc = Def->Description;
		}
	}

	// WeaponData도 ItemTable도 없으면 표시할 게 없다.
	return !OutName.IsEmpty() || OutIcon != nullptr;
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
			FText SlotName, SlotCategory, SlotDesc;
			UTexture2D* SlotIcon = nullptr;
			if (GetWeaponDisplay(Loaded, SlotName, SlotCategory, SlotDesc, SlotIcon))
			{
				Name     = SlotName;
				Category = SlotCategory;
				Desc     = SlotDesc;
				Icon     = SlotIcon;
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
