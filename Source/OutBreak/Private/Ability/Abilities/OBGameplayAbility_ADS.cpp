// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Abilities/OBGameplayAbility_ADS.h"

#include "Ability/Tags/OBGameplayTags.h"
#include "Character/OBCharacterBase.h"

UOBGameplayAbility_ADS::UOBGameplayAbility_ADS(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EOBAbilityActivationPolicy::WhileInputActive;
	ActivationOwnedTags.AddTag(OBGameplayTags::State_Aiming);
}

void UOBGameplayAbility_ADS::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	AOBCharacterBase* Character = GetOBCharacterFromActorInfo();
	if (!Character || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	// 조준 상태(서버: 이동 감속 처리, 클라: 카메라는 로컬에서 처리)
	if (HasAuthority(&ActivationInfo))
	{
		Character->SetAiming(true);
	}
}

void UOBGameplayAbility_ADS::EndAbility(
	const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 조준 해제
	if (AOBCharacterBase* Character = GetOBCharacterFromActorInfo())
	{
		if (HasAuthority(&ActivationInfo))
		{
			Character->SetAiming(false);
		}
	}
	
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
