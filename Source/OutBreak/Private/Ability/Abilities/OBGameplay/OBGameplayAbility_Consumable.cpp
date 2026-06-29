// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Abilities/OBGameplay/OBGameplayAbility_Consumable.h"

#include "Ability/Tags/OBGameplayTags.h"
#include "Character/OBCharacterBase.h"
#include "Inventory/Components/OBInventoryComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"


UOBGameplayAbility_Consumable::UOBGameplayAbility_Consumable(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EOBAbilityActivationPolicy::OnInputTriggered;
	
	// 사용 중 상태 태그(발사/재장전/교체 차단에 사용)
	ActivationOwnedTags.AddTag(OBGameplayTags::State_UsingConsumable);
	
	// 전환/재장전/다른 소모품 사용 중엔 발동 금지.
	ActivationBlockedTags.AddTag(OBGameplayTags::State_Weapon_Switching);
	ActivationBlockedTags.AddTag(OBGameplayTags::State_Reloading);
	ActivationBlockedTags.AddTag(OBGameplayTags::State_UsingConsumable);
}

void UOBGameplayAbility_Consumable::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	UOBInventoryComponent* Inv = GetInventory();
	if (!Inv || Inv->GetItemCount(ItemTag) <= 0 || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	float UseTime = DefaultUseTime;
	if (UseMontage)
	{
		UseTime = UseMontage->GetPlayLength();
		
		UAbilityTask_PlayMontageAndWait* MontageTask = 
			UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, UseMontage);
		// 중단 시 취소
		MontageTask->OnInterrupted.AddDynamic(this, &UOBGameplayAbility_Consumable::OnUseCancelled);
		MontageTask->OnCancelled.AddDynamic(this, &UOBGameplayAbility_Consumable::OnUseCancelled);
		MontageTask->ReadyForActivation();
	}
	
	// 온료 타이밍은 WaitDelay로 서버 확정(효과/소모는 여기서만 1회
	UAbilityTask_WaitDelay* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, UseTime);
	DelayTask->OnFinish.AddDynamic(this, &UOBGameplayAbility_Consumable::OnUseCompleted);
	DelayTask->ReadyForActivation();
}

void UOBGameplayAbility_Consumable::OnUseCompleted()
{
	if (HasAuthority(&CurrentActivationInfo))
	{
		ApplyConsumableEffect();
		if (UOBInventoryComponent* Inv = GetInventory())
		{
			Inv->ConsumeItem(ItemTag, 1);
		}
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UOBGameplayAbility_Consumable::OnUseCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

UOBInventoryComponent* UOBGameplayAbility_Consumable::GetInventory() const
{
	if (AOBCharacterBase* Character = GetOBCharacterFromActorInfo())
	{
		return Character->FindComponentByClass<UOBInventoryComponent>();
	}
	return nullptr;
}
