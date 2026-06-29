// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "OBAmmoViewModel.generated.h"

class UOBInventoryComponent;
class AOBWeaponBase;

UCLASS()
class OUTBREAK_API UOBAmmoViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()
	
public:
	// 표시할 무기 설정(교체 시 재바인딩).
	UFUNCTION(BlueprintCallable, Category = "OB|UI")
	void SetWeapon(AOBWeaponBase* InWeapon);
	
	void SetInventory(UOBInventoryComponent* InInventory);

private:
	void HandleAmmoChanged();
	void RefreshAmmo();

	int32 GetCurrentAmmo() const { return CurrentAmmo; }
	void SetCurrentAmmo(int32 NewValue);
	int32 GetReserveAmmo() const { return ReserveAmmo; }
	void SetReserveAmmo(int32 NewValue);

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Setter, Category = "Ammo", Meta = (AllowPrivateAccess = "true"))
	int32 CurrentAmmo = 0;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Setter, Category = "Ammo", Meta = (AllowPrivateAccess = "true"))
	int32 ReserveAmmo = 0;

	UPROPERTY()
	TWeakObjectPtr<AOBWeaponBase> Weapon;
	
	TWeakObjectPtr<UOBInventoryComponent> Inventory;

	FDelegateHandle AmmoChangedHandle;
	
	FDelegateHandle PoolChangedHandle;
};
