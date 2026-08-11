// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Abilities/OBGameplayAbility.h"

#include "Character/OBCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "Ability/Tags/OBGameplayTags.h"

UOBGameplayAbility::UOBGameplayAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 능력 인스턴스를 액터당 1개 유지(상태/예측에 유리).
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 소유 클라이언트가 예측 실행 후 서버가 확정(발사 등 반응성 표준).
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

AOBCharacterBase* UOBGameplayAbility::GetOBCharacterFromActorInfo() const
{
	return CurrentActorInfo ? Cast<AOBCharacterBase>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
}

bool UOBGameplayAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// OnSpawn abilities are the project's passive policy. They must remain able
	// to initialize if the avatar becomes ready while insertion transit is active.
	if (ActivationPolicy == EOBAbilityActivationPolicy::OnSpawn)
	{
		return true;
	}

	const UAbilitySystemComponent* AbilitySystem =
		ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	return !AbilitySystem
		|| !AbilitySystem->HasMatchingGameplayTag(OBGameplayTags::State_HelicopterTransit);
}

void UOBGameplayAbility::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);

	// OnSpawn 정책: 부여 즉시 1회 발동(패시브 능력 자동 시작).
	if (ActivationPolicy == EOBAbilityActivationPolicy::OnSpawn && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		ActorInfo->AbilitySystemComponent->TryActivateAbility(Spec.Handle);
	}
}
