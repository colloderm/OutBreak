// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Data/OBAbilitySet.h"

#include "Ability/Abilities/OBGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

void FOBAbilitySet_GrantedHandles::AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle)
{
	if (Handle.IsValid())
	{
		AbilitySpecHandles.Add(Handle);
	}
}

void FOBAbilitySet_GrantedHandles::AddGameplayEffectHandle(const FActiveGameplayEffectHandle& Handle)
{
	if (Handle.IsValid())
	{
		GameplayEffectHandles.Add(Handle);
	}
}

void FOBAbilitySet_GrantedHandles::TakeFromAbilitySystem(UAbilitySystemComponent* ASC)
{
	// 회수도 서버 권위에서만.
	if (!ASC || !ASC->IsOwnerActorAuthoritative()) return;

	// 지속 효과 제거
	for (const FActiveGameplayEffectHandle& Handle : GameplayEffectHandles)
	{
		if (Handle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(Handle);
		}
	}
	
	// 능력 제거
	for (const FGameplayAbilitySpecHandle& Handle : AbilitySpecHandles)
	{
		if (Handle.IsValid())
		{
			ASC->ClearAbility(Handle);
		}
	}
	
	GameplayEffectHandles.Reset();
	AbilitySpecHandles.Reset();
}

void UOBAbilitySet::GiveToAbilitySystem(UAbilitySystemComponent* ASC, FOBAbilitySet_GrantedHandles* OutGrantedHandles, UObject* SourceObject) const
{
	check(ASC);

	// 서버 권위가 없으면 즉시 반환: GE 적용은 서버 전용.
	if (!ASC->IsOwnerActorAuthoritative()) return;

	// GameplayEffect 부여
	for (const FOBAbilitySet_GameplayEffect& EffectDef : GrantedGameplayEffects)
	{
		if (!IsValid(EffectDef.GameplayEffect)) continue;

		const UGameplayEffect* GameplayEffectCDO = EffectDef.GameplayEffect->GetDefaultObject<UGameplayEffect>();

		FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
		ContextHandle.AddSourceObject(SourceObject);

		const FActiveGameplayEffectHandle GEHandle = 
			ASC->ApplyGameplayEffectToSelf(GameplayEffectCDO, EffectDef.EffectLevel, ContextHandle);

		if (OutGrantedHandles)
		{
			OutGrantedHandles->AddGameplayEffectHandle(GEHandle);
		}
	}
	
	// GameplayAbility 부여
	for (const FOBAbilitySet_GameplayAbility& AbilityDef : GrantedGameplayAbilities)
	{
		if (!IsValid(AbilityDef.Ability)) continue;
		
		UOBGameplayAbility* AbilityCDO = AbilityDef.Ability->GetDefaultObject<UOBGameplayAbility>();
		
		// 능력 스펙 생성(레벨/소스 오브젝트 포함)
		FGameplayAbilitySpec AbilitySpec(AbilityCDO, AbilityDef.AbilityLevel, INDEX_NONE, SourceObject);
		
		// 입력 태그를 스펙에 심어 입력 단계에서 발동 매칭.
		if (AbilityDef.InputTag.IsValid())
		{
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(AbilityDef.InputTag);
		}
		
		const FGameplayAbilitySpecHandle AbilityHandle = ASC->GiveAbility(AbilitySpec);
		if (OutGrantedHandles)
		{
			OutGrantedHandles->AddAbilitySpecHandle(AbilityHandle);
		}
	}
}
