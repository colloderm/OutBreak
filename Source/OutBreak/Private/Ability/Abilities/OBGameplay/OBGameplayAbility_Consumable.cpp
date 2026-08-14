// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Abilities/OBGameplay/OBGameplayAbility_Consumable.h"

#include "Ability/Tags/OBGameplayTags.h"
#include "Character/OBCharacterBase.h"
#include "Inventory/Components/PlayerInventoryComponent.h"
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
	
	UPlayerInventoryComponent* Inv = GetInventory();

	// 세 가지 실패를 한 줄에 뭉쳐두면 "키를 눌러도 아무 일도 안 난다"만 남는다.
	// 특히 ItemTag 불일치(예: 소지품은 Item.MedKit인데 이 어빌리티는 Item.Bandage)는
	// 발동 자체는 성공한 뒤 여기서 조용히 끝나서 원인을 찾기 어렵다.
	if (!Inv)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Consumable] %s: 인벤토리 컴포넌트를 못 찾았다."), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (Inv->GetItemCount(ItemTag) <= 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Consumable] %s: 소지 수량 0 (어빌리티 ItemTag=%s). "
				 "실제 소지 아이템 태그와 이 어빌리티의 ItemTag가 다른지 확인할 것."),
			*GetName(), *ItemTag.ToString());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Consumable] %s: CommitAbility 실패(코스트/쿨다운)."), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bReleased = false;
	
	float MontageLen = DefaultUseTime;
	if (UseMontage)
	{
		MontageLen = UseMontage->GetPlayLength();
		
		UAbilityTask_PlayMontageAndWait* MontageTask = 
			UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, UseMontage);
		MontageTask->OnCompleted.AddDynamic(this, &UOBGameplayAbility_Consumable::OnMontageEnded);
		MontageTask->OnInterrupted.AddDynamic(this, &UOBGameplayAbility_Consumable::OnUseCancelled);
		MontageTask->OnCancelled.AddDynamic(this, &UOBGameplayAbility_Consumable::OnUseCancelled);
		MontageTask->ReadyForActivation();
	}
	
	// 효과 적용 시점: ReleaseTime>0 → 그 시점(몽타주 중간), 아니면 몽타주 끝.
	const float ApplyAt = (ReleaseTime > 0.f) ? FMath::Min(ReleaseTime, MontageLen) : MontageLen;
	
	UAbilityTask_WaitDelay* ReleaseTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(0.01f, ApplyAt));
	ReleaseTask->OnFinish.AddDynamic(this, &UOBGameplayAbility_Consumable::OnReleased);
	ReleaseTask->ReadyForActivation();
}

void UOBGameplayAbility_Consumable::OnReleased()
{
	if (bReleased) return;
	bReleased = true;

	if (HasAuthority(&CurrentActivationInfo))
	{
		ApplyConsumableEffect();                 // 투척 / 회복 등
		if (UPlayerInventoryComponent* Inv = GetInventory())
		{
			Inv->TryRemoveItem(ItemTag, 1);
		}
	}

	// 몽타주가 없으면 즉시 종료. 있으면 몽타주가 끝날 때 종료(팔로우스루 유지).
	if (!UseMontage)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UOBGameplayAbility_Consumable::OnMontageEnded()
{
	// 릴리즈가 아직이면(몽타주가 릴리즈보다 먼저 끝나는 예외) 보장.
	if (!bReleased)
	{
		OnReleased();
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UOBGameplayAbility_Consumable::OnUseCancelled()
{
	// 릴리즈 전에 중단되면 효과 미적용(수류탄 안 던져지고 소모 안 됨).
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

UPlayerInventoryComponent* UOBGameplayAbility_Consumable::GetInventory() const
{
	if (AOBCharacterBase* Character = GetOBCharacterFromActorInfo())
	{
		return Character->GetPlayerInventoryComponent();
	}
	return nullptr;
}
