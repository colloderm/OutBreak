// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Data/OBAbilitySet.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

void UOBAbilitySet::GiveToAbilitySystem(UAbilitySystemComponent* ASC, UObject* SourceObject) const
{
	check(ASC);

	// 서버 권위가 없으면 즉시 반환: GE 적용은 서버 전용.
	if (!ASC->IsOwnerActorAuthoritative()) return;

	for (const FOBAbilitySet_GameplayEffect& EffectDef : GrantedGameplayEffects)
	{
		if (!IsValid(EffectDef.GameplayEffect)) continue;

		const UGameplayEffect* GameplayEffectCDO = EffectDef.GameplayEffect->GetDefaultObject<UGameplayEffect>();

		FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
		ContextHandle.AddSourceObject(SourceObject);

		ASC->ApplyGameplayEffectToSelf(GameplayEffectCDO, EffectDef.EffectLevel, ContextHandle);
	}
}