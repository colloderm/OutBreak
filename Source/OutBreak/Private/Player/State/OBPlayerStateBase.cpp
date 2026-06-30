// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/State/OBPlayerStateBase.h"

#include "Net/UnrealNetwork.h"
#include "AbilitySystemComponent.h"
#include "Ability/Components/OBAbilitySystemComponent.h"
#include "Ability/Attributes/OBAttributeSetBase.h"
#include "Weapon/OBWeaponBase.h"

AOBPlayerStateBase::AOBPlayerStateBase()
{
	AbilitySystemComponent = CreateDefaultSubobject<UOBAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	SetNetUpdateFrequency(100.0f);
	
	AttributeSet = CreateDefaultSubobject<UOBAttributeSetBase>(TEXT("AttributeSet"));
	
}

UAbilitySystemComponent* AOBPlayerStateBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AOBPlayerStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AOBPlayerStateBase, SelectedWeapons);
	DOREPLIFETIME(AOBPlayerStateBase, bReady);
}

void AOBPlayerStateBase::SetWeaponForSlot(EOBWeaponSlot Slot, TSubclassOf<AOBWeaponBase> WeaponClass)
{
	if (!HasAuthority()) return;

	// 같은 슬롯의 기존 선택 제거(슬롯당 1개).
	SelectedWeapons.RemoveAll([Slot](const TSubclassOf<AOBWeaponBase>& W)
	{
		if (!W) return false;
		if (AOBWeaponBase* CDO = W->GetDefaultObject<AOBWeaponBase>())
			if (UOBWeaponData* Data = CDO->GetWeaponData())
				return Data->WeaponSlot == Slot;
		return false;
	});

	if (WeaponClass) 
		SelectedWeapons.Add(WeaponClass);
	
	OnLobbyStateChanged.Broadcast();
}

void AOBPlayerStateBase::SetReady(bool bInReady)
{
	if (!HasAuthority()) return;
	
	bReady = bInReady;
	OnLobbyStateChanged.Broadcast();
}

void AOBPlayerStateBase::OnRep_SelectedWeapons()
{
	OnLobbyStateChanged.Broadcast();
}

void AOBPlayerStateBase::OnRep_Ready()
{
	OnLobbyStateChanged.Broadcast();
}

void AOBPlayerStateBase::CopyProperties(APlayerState* NewPlayerState)
{
	Super::CopyProperties(NewPlayerState);
	
	if (AOBPlayerStateBase* PS = Cast<AOBPlayerStateBase>(NewPlayerState))
	{
		PS->SelectedWeapons = SelectedWeapons;
		PS->bReady = bReady;
	}
}
