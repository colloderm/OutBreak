// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Weapon/Data/OBWeaponData.h"
#include "OBInventoryComponent.generated.h"

class AOBWeaponBase;

DECLARE_MULTICAST_DELEGATE(FOBOnInventoryChanged);
DECLARE_MULTICAST_DELEGATE(FOBOnAmmoPoolChanged);

USTRUCT(BlueprintType)
struct FOBWeaponSlotEntry
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly)
	EOBWeaponSlot Slot = EOBWeaponSlot::Primary;
	
	UPROPERTY(BlueprintReadOnly)
	TSubclassOf<AOBWeaponBase> WeaponClass;
};

USTRUCT(BlueprintType)
struct FOBCountEntry // 탄약 풀 / 소모품 공용 (태그->개수)
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly)
	FGameplayTag Tag;
	
	UPROPERTY(BlueprintReadOnly)
	int32 Count = 0;
};

UCLASS(ClassGroup=(OutBreak), meta=(BlueprintSpawnableComponent))
class OUTBREAK_API UOBInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOBInventoryComponent();
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	// --- 무기 슬롯 ---
	void AddWeapon(TSubclassOf<AOBWeaponBase> WeaponClass); // 서버
	void EquipSlot(EOBWeaponSlot Slot);						// 서버
	
	UFUNCTION(Server, Reliable)
	void Server_EquipSlot(EOBWeaponSlot Slot);				// 클라 요청
	
	UFUNCTION(BlueprintPure, Category = "Inventory")
	EOBWeaponSlot GetActiveSlot() const { return ActiveSlot; }
	TSubclassOf<AOBWeaponBase> GetWeaponInSlot(EOBWeaponSlot Slot) const;
	
	// --- 탄약 풀(타입별) ---
	UFUNCTION(BlueprintPure, Category = "Inventory|Ammo")
	int32 GetAmmo(const FGameplayTag& AmmoType) const;
	void AddAmmo(const FGameplayTag& AmmoType, int32 Amount);              // 서버
	int32 ConsumeAmmoFromPool(const FGameplayTag& AmmoType, int32 Amount); // 서버

	// --- 소모품(붕대/치료킷/수류탄 등) 카운트 ---  (사용 어빌리티는 다음 단계)
	UFUNCTION(BlueprintPure, Category = "Inventory|Items")
	int32 GetItemCount(const FGameplayTag& ItemTag) const;
	void AddItem(const FGameplayTag& ItemTag, int32 Amount);               // 서버
	int32 ConsumeItem(const FGameplayTag& ItemTag, int32 Amount);          // 서버
	
	void EquipDefaultSlot();

public:
	FOBOnInventoryChanged OnInventoryChanged;
	FOBOnAmmoPoolChanged OnAmmoPoolChanged;

protected:
	UFUNCTION()
	void OnRep_ActiveSlot();
	
	UFUNCTION()
	void OnRep_AmmoPool();
	
	UFUNCTION()
	void OnRep_Items();
	
	void EquipActiveWeapon();
	static int32 GetCount(const TArray<FOBCountEntry>& Arr, const FGameplayTag& Tag);
	
	UPROPERTY(Replicated)
	TArray<FOBWeaponSlotEntry> WeaponSlots;
	
	UPROPERTY(ReplicatedUsing = OnRep_ActiveSlot)
	EOBWeaponSlot ActiveSlot = EOBWeaponSlot::Primary;
	
	UPROPERTY(ReplicatedUsing = OnRep_AmmoPool)
	TArray<FOBCountEntry> AmmoPool;
	
	UPROPERTY(ReplicatedUsing = OnRep_Items)
	TArray<FOBCountEntry> Items;
	
};
