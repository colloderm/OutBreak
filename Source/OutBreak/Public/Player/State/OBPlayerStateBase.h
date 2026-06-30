// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "Weapon/Data/OBWeaponData.h"
#include "OBPlayerStateBase.generated.h"

class AOBWeaponBase;
class UOBAbilitySystemComponent;
class UOBAttributeSetBase;
class UAbilitySystemComponent;

UCLASS()
class OUTBREAK_API AOBPlayerStateBase : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()
public:
	AOBPlayerStateBase();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* NewPlayerState) override;
	
	UOBAttributeSetBase* GetAttributeSet() const { return AttributeSet; }
	UOBAbilitySystemComponent* GetOBAbilitySystemComponent() const { return AbilitySystemComponent; }
	
	// 로비 선택(서버).
	void SetWeaponForSlot(EOBWeaponSlot Slot, TSubclassOf<AOBWeaponBase> WeaponClass);
	void SetReady(bool bInReady);

	const TArray<TSubclassOf<AOBWeaponBase>>& GetSelectedWeapons() const { return SelectedWeapons; }
	bool IsReady() const { return bReady; }

	// 로비 UI 갱신.
	DECLARE_MULTICAST_DELEGATE(FOBOnLobbyStateChanged);
	FOBOnLobbyStateChanged OnLobbyStateChanged;
	
protected:
	UFUNCTION() 
	void OnRep_SelectedWeapons();
	
	UFUNCTION() 
	void OnRep_Ready();
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ability", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UOBAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UOBAttributeSetBase> AttributeSet;

	UPROPERTY(ReplicatedUsing = OnRep_SelectedWeapons)
	TArray<TSubclassOf<AOBWeaponBase>> SelectedWeapons;

	UPROPERTY(ReplicatedUsing = OnRep_Ready)
	bool bReady = false;
};
