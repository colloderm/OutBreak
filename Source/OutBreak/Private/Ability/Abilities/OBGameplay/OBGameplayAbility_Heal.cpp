// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Abilities/OBGameplay/OBGameplayAbility_Heal.h"

#include "Ability/Tags/OBGameplayTags.h"

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
