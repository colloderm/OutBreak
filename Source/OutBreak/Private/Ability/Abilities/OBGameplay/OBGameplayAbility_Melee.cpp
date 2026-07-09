// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Abilities/OBGameplay/OBGameplayAbility_Melee.h"

#include "Ability/Tags/OBGameplayTags.h"
#include "Character/OBCharacterBase.h"
#include "Equipment/Components/OBEquipmentComponent.h"
#include "Weapon/OBWeaponBase.h"
#include "Weapon/Data/OBWeaponData.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Engine/OverlapResult.h"
#include "Player/State/OBPlayerStateBase.h"

UOBGameplayAbility_Melee::UOBGameplayAbility_Melee(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EOBAbilityActivationPolicy::OnInputTriggered;
	
	// 스윙 1번씩(진행 중 재입력 무기) + 전환/소모품 중 금지.
	ActivationOwnedTags.AddTag(OBGameplayTags::State_Melee_Attacking);
	ActivationBlockedTags.AddTag(OBGameplayTags::State_Melee_Attacking);
	ActivationBlockedTags.AddTag(OBGameplayTags::State_Weapon_Switching);
	ActivationBlockedTags.AddTag(OBGameplayTags::State_UsingConsumable);
}

void UOBGameplayAbility_Melee::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	AOBWeaponBase* Weapon = GetEquippedWeapon();
	UOBWeaponData* Data = Weapon ? Weapon->GetWeaponData() : nullptr;
	if (!Data || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	float Len = DefaultAttackTime;
	if (UAnimMontage* Montage = Data->AttackMontage) // 근접은 AttackMontage = 스윙 몽타주로 사용
	{
		Len = Montage->GetPlayLength();
		UAbilityTask_PlayMontageAndWait* MT = 
			UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage);
		MT->OnCompleted.AddDynamic(this, &UOBGameplayAbility_Melee::OnAttackFinished);
		MT->OnInterrupted.AddDynamic(this, &UOBGameplayAbility_Melee::OnAttackCancelled);
		MT->OnCancelled.AddDynamic(this, &UOBGameplayAbility_Melee::OnAttackCancelled);
		MT->ReadyForActivation();
	}
	
	// 타격 판정 타이밍(접촉 순간)
	UAbilityTask_WaitDelay* HitTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Clamp(HitTime, 0.01f, Len));
	HitTask->OnFinish.AddDynamic(this, &UOBGameplayAbility_Melee::OnHitWindow);
	HitTask->ReadyForActivation();
	
	// 몽타주 없으면 Len 후 종료
	if (!Data->AttackMontage)
	{
		UAbilityTask_WaitDelay* EndTask = UAbilityTask_WaitDelay::WaitDelay(this, Len);
		EndTask->OnFinish.AddDynamic(this, &UOBGameplayAbility_Melee::OnAttackFinished);
		EndTask->ReadyForActivation();
	}
}

void UOBGameplayAbility_Melee::OnHitWindow()
{
	if (HasAuthority(&CurrentActivationInfo))
	{
		PerformMeleeTrace();
	}
}

void UOBGameplayAbility_Melee::PerformMeleeTrace()
{
	AOBCharacterBase* Char = GetOBCharacterFromActorInfo();
	AOBWeaponBase* Weapon = GetEquippedWeapon();
	UOBWeaponData* Data = Weapon ? Weapon->GetWeaponData() : nullptr;
	if (!Char || !Data || !GetWorld()) return;

	const FVector Start = Char->GetActorLocation() + FVector(0, 0, TraceHeight);
	const FVector Dir = Char->GetBaseAimRotation().Vector();
	
	// 전방 리치 중심의 스피어 오버랩(반경 내 다중 대상).
	const FVector Center = Start + Dir * (Data->Range * 0.5f);
	const float SphereRadius = Data->Range * 0.5f + MeleeRadius;
	
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Char);
	GetWorld()->OverlapMultiByChannel(
		Overlaps, Center, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(SphereRadius), Params);

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!SourceASC) return;
	
	// --- 스윙 연출(명중 무관, 매 스윙 1회) ---
	{
		FGameplayCueParameters SwingCue;
		SwingCue.Location = Weapon->GetMuzzleLocation(); // 칼끝 / 무기 위치(소켓 없으면 메시 원점)
		SwingCue.Instigator = Char;
		SwingCue.SourceObject = Weapon;						// 큐 BP가 WeaponData(사운드 등) 참조
		SourceASC->ExecuteGameplayCue(OBGameplayTags::GameplayCue_Melee_Swing, SwingCue);
	}

	// --- 히트 처리 + 임팩트 연출 ---
	TSet<UAbilitySystemComponent*> AlreadyHit;
	for (const FOverlapResult& H : Overlaps)
	{
		AActor* HitActor = H.GetActor();
		if (!HitActor || HitActor == Char) continue;
		
		// 전방 판정(뒤/옆 과도 타격 방지).
		const FVector ToTarget = (HitActor->GetActorLocation() - Start).GetSafeNormal();
		if (FVector::DotProduct(Dir, ToTarget) < MinFacingDot) continue;

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
		if (!TargetASC || AlreadyHit.Contains(TargetASC)) continue;
		AlreadyHit.Add(TargetASC);

		// 데미지 (같은 팀이면 스킵 → 팀킬 OFF, 임팩트 연출은 아래에서 유지)
		if (Data->DamageEffect && !AOBPlayerStateBase::AreSameTeam(Char, HitActor))
		{
			FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
			Ctx.AddInstigator(Char, Weapon);
			FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(Data->DamageEffect, 1.f, Ctx);
			if (Spec.IsValid())
			{
				Spec.Data->SetSetByCallerMagnitude(OBGameplayTags::SetByCaller_Damage, Data->BaseDamage);
				SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
			}
		}
		
		// 임팩트 연출(오버랩엔 ImpactPoint 없음 → 대상 가슴 높이, Normal은 공격자 방향).
		{
			FGameplayCueParameters ImpactCue;
			ImpactCue.Location = HitActor->GetActorLocation() + FVector(0, 0, TraceHeight * 0.5f);
			ImpactCue.Normal = -ToTarget; // VFX가 공격자 쪽을 향하도록
			ImpactCue.Instigator = Char;
			ImpactCue.SourceObject = Weapon;
			SourceASC->ExecuteGameplayCue(OBGameplayTags::GameplayCue_Melee_Impact, ImpactCue);
		}
	}
}

void UOBGameplayAbility_Melee::OnAttackFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UOBGameplayAbility_Melee::OnAttackCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

AOBWeaponBase* UOBGameplayAbility_Melee::GetEquippedWeapon() const
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
