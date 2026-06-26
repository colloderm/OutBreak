// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Ability/Data/OBAbilitySet.h"
#include "OBEquipmentComponent.generated.h"

class AOBWeaponBase;

DECLARE_MULTICAST_DELEGATE_OneParam(FOBOnWeaponChanged, AOBWeaponBase*);

UCLASS(ClassGroup=(OutBreak), meta=(BlueprintSpawnableComponent))
class OUTBREAK_API UOBEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOBEquipmentComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void EquipWeapon(TSubclassOf<AOBWeaponBase> WeaponClass);

	void UnequipWeapon();

	UFUNCTION(BlueprintPure, Category = "Equipment")
	AOBWeaponBase* GetCurrentWeapon() const { return CurrentWeapon; }
	
public:
	FOBOnWeaponChanged OnWeaponChanged;
	
protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UFUNCTION()
	void OnRep_CurrentWeapon();

	void AttachWeaponToOwner();

protected:
	UPROPERTY(ReplicatedUsing = OnRep_CurrentWeapon)
	TObjectPtr<AOBWeaponBase> CurrentWeapon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	FName AttachSocketName = TEXT("hand_r_Socket");
	
	//현재 무기로 부여한 능력/효과 핸들. 해제 시 회수에 사용.
	FOBAbilitySet_GrantedHandles GrantedAbilityHandles;
	
};
