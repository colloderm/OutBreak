// Fill out your copyright notice in the Description page of Project Settings.

#include "Inventory/Components/OBInventoryComponent.h"

#include "Weapon/OBWeaponBase.h"
#include "Equipment/Components/OBEquipmentComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Ability/Tags/OBGameplayTags.h"
#include "Weapon/Data/OBWeaponData.h"
#include "Animation/AnimMontage.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

UOBInventoryComponent::UOBInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UOBInventoryComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(UOBInventoryComponent, WeaponSlots);
	DOREPLIFETIME(UOBInventoryComponent, ActiveSlot);
	DOREPLIFETIME(UOBInventoryComponent, AmmoPool);
	DOREPLIFETIME(UOBInventoryComponent, Items);
}

int32 UOBInventoryComponent::GetCount(const TArray<FOBCountEntry>& Arr, const FGameplayTag& Tag)
{
	for (const FOBCountEntry& E : Arr)
	{
		if (E.Tag == Tag)
			return E.Count;
	}
	
	return 0;
}

FOBWeaponSlotEntry* UOBInventoryComponent::FindSlotEntry(EOBWeaponSlot Slot)
{
	for (FOBWeaponSlotEntry& E : WeaponSlots) { if (E.Slot == Slot) return &E; }
	return nullptr;
}

TSubclassOf<AOBWeaponBase> UOBInventoryComponent::GetWeaponInSlot(EOBWeaponSlot Slot) const
{
	for (const FOBWeaponSlotEntry& E : WeaponSlots)
	{
		if (E.Slot == Slot)
			return E.WeaponClass;
	}
	
	return nullptr;
}

void UOBInventoryComponent::AddWeapon(TSubclassOf<AOBWeaponBase> WeaponClass)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !WeaponClass) return;
	
	EOBWeaponSlot Slot = EOBWeaponSlot::Primary;
	FGameplayTag AmmoType;
	int32 StartAmmo = 0;
	if (AOBWeaponBase* CDO = WeaponClass->GetDefaultObject<AOBWeaponBase>())
	{
		if (UOBWeaponData* Data = CDO->GetWeaponData())
		{
			Slot = Data->WeaponSlot;
			AmmoType = Data->AmmoType;
			StartAmmo = Data->MaxReserveAmmo;
		}
	}
	
	// 해당 슬롯에 배치(있으면 교체)
	bool bFound = false;
	for (FOBWeaponSlotEntry& E : WeaponSlots)
	{
		if (E.Slot == Slot)
		{
			E.WeaponClass = WeaponClass;
			bFound = true;
			break;
		}
	}
	if (!bFound)
	{
		FOBWeaponSlotEntry New;
		New.Slot = Slot;
		New.WeaponClass = WeaponClass;
		WeaponSlots.Add(New);
	}
	
	if (AmmoType.IsValid())
	{
		AddAmmo(AmmoType, StartAmmo);
	}
	
	OnInventoryChanged.Broadcast();
}

void UOBInventoryComponent::EquipSlot(EOBWeaponSlot Slot)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (!GetWeaponInSlot(Slot)) return;
	if (bSwitching) return;
	
	UOBEquipmentComponent* Equip = GetOwner()->FindComponentByClass<UOBEquipmentComponent>();
	const bool bHasWeapon = Equip && Equip->GetCurrentWeapon() != nullptr;

	// 이미 무기를 들고 다른 슬롯으로 → holster → draw 전환.
	if (bHasWeapon && Slot == ActiveSlot) return;
	
	// 다른 슬롯으로 → 즉시 교체 + draw.
	if (bHasWeapon)
	{
		SwapToSlot(Slot);
		return;
	}
	
	// 첫 장착(또는 동일 슬롯 재장착) → 즉시
	ActiveSlot = Slot;
	EquipActiveWeapon();
	OnInventoryChanged.Broadcast();
}

void UOBInventoryComponent::Server_EquipSlot_Implementation(EOBWeaponSlot Slot)
{
	EquipSlot(Slot);
}

void UOBInventoryComponent::EquipActiveWeapon()
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	
	UOBEquipmentComponent* Equip = GetOwner()->FindComponentByClass<UOBEquipmentComponent>();
	if (!Equip) return;
	
	Equip->EquipWeapon(GetWeaponInSlot(ActiveSlot));
	AOBWeaponBase* Weapon = Equip->GetCurrentWeapon();
	if (!Weapon) return;
	
	// 슬롯 탄창 복원(첫 장착이면 현재 full을 슬롯에 저장).
	if (FOBWeaponSlotEntry* Entry = FindSlotEntry(ActiveSlot))
	{
		if (Entry->MagazineAmmo >= 0)
		{
			Weapon->SetCurrentAmmo(Entry->MagazineAmmo);
		}
		else
		{
			Entry->MagazineAmmo = Weapon->GetCurrentAmmo();
		}
	}

	// 탄창 변화를 슬롯에 동기화하도록 구독(이전 무기 해제).
	if (BoundWeapon.IsValid() && WeaponAmmoHandle.IsValid())
	{
		BoundWeapon->OnAmmoChanged.Remove(WeaponAmmoHandle);
		WeaponAmmoHandle.Reset();
	}
	
	WeaponAmmoHandle = Weapon->OnAmmoChanged.AddUObject(this, &UOBInventoryComponent::SyncActiveMagazine);
	BoundWeapon = Weapon;
}

void UOBInventoryComponent::SyncActiveMagazine()
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	
	if (AOBWeaponBase* W = BoundWeapon.Get())
	{
		if (FOBWeaponSlotEntry* Entry = FindSlotEntry(ActiveSlot))
		{
			Entry->MagazineAmmo = W->GetCurrentAmmo();   // 발사/재장전마다 저장
		}
	}
}

void UOBInventoryComponent::SwapToSlot(EOBWeaponSlot NewSlot)
{
	if (!GetWorld()) return;

	bSwitching = true;
	SetSwitching(true);   // 발사/재장전 차단 + 진행 중 재장전 취소

	ActiveSlot = NewSlot;
	EquipActiveWeapon();
	OnInventoryChanged.Broadcast();

	// 꺼내기(draw) 시간만큼 후 차단 해제.
	float DrawTime = DefaultDrawTime;
	if (UOBEquipmentComponent* Equip = GetOwner()->FindComponentByClass<UOBEquipmentComponent>())
	{
		if (AOBWeaponBase* New = Equip->GetCurrentWeapon())
		{
			if (UOBWeaponData* Data = New->GetWeaponData())
			{
				if (Data->EquipMontage) DrawTime = Data->EquipMontage->GetPlayLength();
			}
		}
	}

	GetWorld()->GetTimerManager().SetTimer(
		SwapTimerHandle, this, &UOBInventoryComponent::EndSwitching, FMath::Max(0.01f, DrawTime), false
	);
}

void UOBInventoryComponent::EndSwitching()
{
	bSwitching = false;
	SetSwitching(false);
}

void UOBInventoryComponent::SetSwitching(bool bEnable)
{
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
	if (!ASC) return;

	if (bEnable)
	{
		// 다른 무기로 재장전이 완료되는 것 방지: 진행 중 재장전 취소.
		FGameplayTagContainer ReloadTags;
		ReloadTags.AddTag(OBGameplayTags::State_Reloading);
		ASC->CancelAbilities(&ReloadTags);

		// 복제 loose 태그 → 서버+소유 클라 모두 발사/재장전 차단.
		ASC->AddLooseGameplayTag(OBGameplayTags::State_Weapon_Switching);
	}
	else
	{
		ASC->RemoveLooseGameplayTag(OBGameplayTags::State_Weapon_Switching);
	}
}

int32 UOBInventoryComponent::GetAmmo(const FGameplayTag& AmmoType) const
{
	return GetCount(AmmoPool, AmmoType);
}

void UOBInventoryComponent::AddAmmo(const FGameplayTag& AmmoType, int32 Amount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !AmmoType.IsValid() || Amount <= 0) return;
	for (FOBCountEntry& E : AmmoPool)
	{
		if (E.Tag == AmmoType)
		{
			E.Count += Amount;
			OnAmmoPoolChanged.Broadcast();
			return;
		}
	}
	
	AmmoPool.Add({AmmoType, Amount});
	OnAmmoPoolChanged.Broadcast();
}

int32 UOBInventoryComponent::ConsumeAmmoFromPool(const FGameplayTag& AmmoType, int32 Amount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || Amount <= 0) return 0;
	for (FOBCountEntry& E : AmmoPool)
	{
		if (E.Tag == AmmoType)
		{
			const int32 C = FMath::Min(E.Count, Amount);
			E.Count -= C;
			OnAmmoPoolChanged.Broadcast();
			return C;
		}
	}
	
	return 0;
}

int32 UOBInventoryComponent::GetItemCount(const FGameplayTag& ItemTag) const
{
	return GetCount(Items, ItemTag);
}

void UOBInventoryComponent::AddItem(const FGameplayTag& ItemTag, int32 Amount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !ItemTag.IsValid() || Amount <= 0) return;
	for (FOBCountEntry& E : Items)
	{
		if (E.Tag == ItemTag)
		{
			E.Count += Amount;
			OnInventoryChanged.Broadcast();
			return;
		}
	}
	
	Items.Add({ItemTag, Amount});
	OnInventoryChanged.Broadcast();
}

int32 UOBInventoryComponent::ConsumeItem(const FGameplayTag& ItemTag, int32 Amount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || Amount <= 0) return 0;
	for (FOBCountEntry& E : Items)
	{
		if (E.Tag == ItemTag)
		{
			const int32 C = FMath::Min(E.Count, Amount);
			E.Count -= C;
			OnInventoryChanged.Broadcast();
			return C;
		}
	}
	
	return 0;
}

void UOBInventoryComponent::EquipDefaultSlot()
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	
	if (GetWeaponInSlot(EOBWeaponSlot::Primary))
	{
		EquipSlot(EOBWeaponSlot::Primary);
	}
	else if (GetWeaponInSlot(EOBWeaponSlot::Secondary))
	{
		EquipSlot(EOBWeaponSlot::Secondary);
	}
	else if (GetWeaponInSlot(EOBWeaponSlot::Melee))
	{
		EquipSlot(EOBWeaponSlot::Melee);
	}
}

void UOBInventoryComponent::OnRep_ActiveSlot()
{
	OnInventoryChanged.Broadcast();
}

void UOBInventoryComponent::OnRep_AmmoPool()
{
	OnAmmoPoolChanged.Broadcast();
}

void UOBInventoryComponent::OnRep_Items()
{
	OnInventoryChanged.Broadcast();
}
