// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/ViewModels/OBAmmoViewModel.h"

#include "Weapon/OBWeaponBase.h"

void UOBAmmoViewModel::SetWeapon(AOBWeaponBase* InWeapon)
{
	// 이전 무기 구독 해제.
	if (Weapon.IsValid() && AmmoChangedHandle.IsValid())
	{
		Weapon->OnAmmoChanged.Remove(AmmoChangedHandle);
		AmmoChangedHandle.Reset();
	}

	Weapon = InWeapon;

	// 새 무기 구독.
	if (InWeapon)
	{
		AmmoChangedHandle = InWeapon->OnAmmoChanged.AddUObject(this, &UOBAmmoViewModel::HandleAmmoChanged);
	}

	RefreshAmmo();
}

void UOBAmmoViewModel::HandleAmmoChanged()
{
	RefreshAmmo();
}

void UOBAmmoViewModel::RefreshAmmo()
{
	if (AOBWeaponBase* W = Weapon.Get())
	{
		SetCurrentAmmo(W->GetCurrentAmmo());
		SetReserveAmmo(W->GetReserveAmmo());
	}
	else
	{
		SetCurrentAmmo(0);
		SetReserveAmmo(0);
	}
}

void UOBAmmoViewModel::SetCurrentAmmo(int32 NewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(CurrentAmmo, NewValue);
}

void UOBAmmoViewModel::SetReserveAmmo(int32 NewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(ReserveAmmo, NewValue);
}