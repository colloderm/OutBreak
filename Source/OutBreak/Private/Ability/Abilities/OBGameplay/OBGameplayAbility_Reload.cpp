// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Abilities/OBGameplay/OBGameplayAbility_Reload.h"

#include "Ability/Tags/OBGameplayTags.h"
#include "Character/OBCharacterBase.h"
#include "Equipment/Components/OBEquipmentComponent.h"
#include "Weapon/OBWeaponBase.h"
#include "Weapon/Data/OBWeaponData.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"

UOBGameplayAbility_Reload::UOBGameplayAbility_Reload(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EOBAbilityActivationPolicy::OnInputTriggered;
	// 재장전 중 상태 태그(발사 차단에 사용).
	ActivationOwnedTags.AddTag(OBGameplayTags::State_Reloading);
}

void UOBGameplayAbility_Reload::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	AOBWeaponBase* Weapon = GetEquippedWeapon();
	if (!Weapon || !Weapon->CanReload() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAnimMontage* Montage = Weapon->GetWeaponData() ? Weapon->GetWeaponData()->ReloadMontage : nullptr;
	float ReloadTime = 1.5f; // 몽타주 없을 때 기본
	
	if (Montage)
	{
		ReloadTime = Montage->GetPlayLength();
		
		UAbilityTask_PlayMontageAndWait* MontageTask = 
			UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage);
		MontageTask->OnCompleted.AddDynamic(this, &UOBGameplayAbility_Reload::OnReloadCompleted);
		MontageTask->OnInterrupted.AddDynamic(this, &UOBGameplayAbility_Reload::OnReloadCancelled);
		MontageTask->OnCancelled.AddDynamic(this, &UOBGameplayAbility_Reload::OnReloadCancelled);
		MontageTask->ReadyForActivation();
	}
	
	// 충전 타이밍: WaitDelay는 클라/서버 모두 확실히 발화 → 서버에서 PerformReload 보장.
	UAbilityTask_WaitDelay* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, ReloadTime);
	DelayTask->OnFinish.AddDynamic(this, &UOBGameplayAbility_Reload::OnReloadCompleted);
	DelayTask->ReadyForActivation();
}

void UOBGameplayAbility_Reload::OnReloadCompleted()
{
	if (HasAuthority(&CurrentActivationInfo))
	{
		if (AOBWeaponBase* Weapon = GetEquippedWeapon())
		{
			Weapon->PerformReload();
		}
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UOBGameplayAbility_Reload::OnReloadCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

AOBWeaponBase* UOBGameplayAbility_Reload::GetEquippedWeapon() const
{
	if (AOBCharacterBase* Character = GetOBCharacterFromActorInfo())
	{
		if (UOBEquipmentComponent* Equipment = Character->FindComponentByClass<UOBEquipmentComponent>())
		{
			return Equipment->GetCurrentWeapon();
		}
	}
	return nullptr;
}
