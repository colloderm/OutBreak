// Fill out your copyright notice in the Description page of Project Settings.

#include "Inventory/Components/OBInventoryComponent.h"

#include "Weapon/OBWeaponBase.h"
#include "Equipment/Components/OBEquipmentComponent.h"
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
	
	UE_LOG(LogTemp, Warning, TEXT("[Inv] AddWeapon slot=%d class=%s"), (int32)Slot, *GetNameSafe(WeaponClass));
	
	OnInventoryChanged.Broadcast();
}

void UOBInventoryComponent::EquipSlot(EOBWeaponSlot Slot)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (!GetWeaponInSlot(Slot)) return;
	
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
	if (UOBEquipmentComponent* Equip = GetOwner()->FindComponentByClass<UOBEquipmentComponent>())
	{
		Equip->EquipWeapon(GetWeaponInSlot(ActiveSlot));
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
	if (GetOwner() || !GetOwner()->HasAuthority() || Amount <= 0) return 0;
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
	if (GetWeaponInSlot(EOBWeaponSlot::Secondary))
	{
		EquipSlot(EOBWeaponSlot::Secondary);
	}
	if (GetWeaponInSlot(EOBWeaponSlot::Melee))
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
