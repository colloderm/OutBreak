// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/ViewModels/OBAmmoViewModel.h"

#include "Weapon/OBWeaponBase.h"
#include "Weapon/Data/OBWeaponData.h"
#include "Inventory/Components/OBInventoryComponent.h"

void UOBAmmoViewModel::SetWeapon(AOBWeaponBase* InWeapon)
{
	if (Weapon.IsValid() && AmmoChangedHandle.IsValid())
	{
		Weapon->OnAmmoChanged.Remove(AmmoChangedHandle);
		AmmoChangedHandle.Reset();
	}

	Weapon = InWeapon;
	if (InWeapon)
	{
		AmmoChangedHandle = InWeapon->OnAmmoChanged.AddUObject(this, &UOBAmmoViewModel::HandleAmmoChanged);
		RefreshAmmo();
	}
	// null이면 마지막 값 유지(사망/교체 깜빡임 방지)
}

void UOBAmmoViewModel::SetInventory(UOBInventoryComponent* InInventory)
{
	if (Inventory.IsValid() && PoolChangedHandle.IsValid())
	{
		Inventory->OnAmmoPoolChanged.Remove(PoolChangedHandle);
		PoolChangedHandle.Reset();
	}
	Inventory = InInventory;
	if (InInventory)
	{
		PoolChangedHandle = InInventory->OnAmmoPoolChanged.AddUObject(this, &UOBAmmoViewModel::HandleAmmoChanged);
	}
	RefreshAmmo();
}

void UOBAmmoViewModel::HandleAmmoChanged()
{
	RefreshAmmo();
}

void UOBAmmoViewModel::RefreshAmmo()
{
	int32 Mag = 0;
	int32 Reserve = 0;

	if (AOBWeaponBase* W = Weapon.Get())
	{
		Mag = W->GetCurrentAmmo();
		if (UOBWeaponData* Data = W->GetWeaponData())
		{
			if (UOBInventoryComponent* Inv = Inventory.Get())
			{
				Reserve = Inv->GetAmmo(Data->AmmoType);   // 무기 타입의 풀 예비탄
			}
		}
		SetCurrentAmmo(Mag);
		SetReserveAmmo(Reserve);
	}
	// 무기 없으면 마지막 값 유지
}

void UOBAmmoViewModel::SetCurrentAmmo(int32 NewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(CurrentAmmo, NewValue);
}

void UOBAmmoViewModel::SetReserveAmmo(int32 NewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(ReserveAmmo, NewValue);
}