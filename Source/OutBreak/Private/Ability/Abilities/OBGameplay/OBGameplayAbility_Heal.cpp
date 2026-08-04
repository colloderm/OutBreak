// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Abilities/OBGameplay/OBGameplayAbility_Heal.h"

#include "AbilitySystemComponent.h"
#include "Ability/Attributes/OBAttributeSetBase.h"
#include "Ability/Tags/OBGameplayTags.h"

bool UOBGameplayAbility_Heal::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC) return false;

	bool bFound = false;
	const float Health = ASC->GetGameplayAttributeValue(UOBAttributeSetBase::GetHealthAttribute(), bFound);
	if (!bFound) return true;

	const float MaxHealth = ASC->GetGameplayAttributeValue(UOBAttributeSetBase::GetMaxHealthAttribute(), bFound);

	// 속성을 못 읽으면 막지 않는다. 회복이 영영 안 되는 쪽이 더 나쁘다.
	if (!bFound || MaxHealth <= 0.f) return true;

	// 부동소수 오차로 99.999가 남는 경우를 풀피로 친다.
	return Health < MaxHealth - KINDA_SMALL_NUMBER;
}

void UOBGameplayAbility_Heal::ApplyConsumableEffect()
{
	if (!HealEffect) return;
	
	FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(HealEffect, GetAbilityLevel());
	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(OBGameplayTags::SetByCaller_Heal, HealAmount);
		ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, Spec);
	}
}
