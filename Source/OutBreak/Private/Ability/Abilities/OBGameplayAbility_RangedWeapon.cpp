// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Abilities/OBGameplayAbility_RangedWeapon.h"

#include "Ability/Tags/OBGameplayTags.h"
#include "Character/OBCharacterBase.h"
#include "Equipment/Components/OBEquipmentComponent.h"
#include "Weapon/OBWeaponBase.h"
#include "Weapon/Data/OBWeaponData.h"
#include "Core/OBCollisionChannels.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"

UOBGameplayAbility_RangedWeapon::UOBGameplayAbility_RangedWeapon(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 발사는 입력 순간 1회 발동(연사는 입력 정책/반복으로 확장).
	ActivationPolicy = EOBAbilityActivationPolicy::OnInputTriggered;
}

void UOBGameplayAbility_RangedWeapon::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 코스트/쿨다운 커밋(설정돼 있으면 적용). 실패 시 능력 종료.
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicate=*/true, /*bWasCancelled=*/true);
		return;
	}

	// 데미지 판정은 서버 권위에서만 수행.
	if (HasAuthority(&ActivationInfo))
	{
		PerformServerWeaponTrace();
	}

	// [확장] 머즐 VFX/사격음/반동/몽타주는 여기서 양쪽 재생(예측 표현).

	// 단발 발사이므로 즉시 종료.
	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicate=*/true, /*bWasCancelled=*/false);
}

AOBWeaponBase* UOBGameplayAbility_RangedWeapon::GetEquippedWeapon() const
{
	AOBCharacterBase* Character = GetOBCharacterFromActorInfo();
	if (!Character) return nullptr;

	if (UOBEquipmentComponent* Equipment = Character->FindComponentByClass<UOBEquipmentComponent>())
	{
		return Equipment->GetCurrentWeapon();
	}
	return nullptr;
}

void UOBGameplayAbility_RangedWeapon::PerformServerWeaponTrace()
{
	AOBCharacterBase* Character = GetOBCharacterFromActorInfo();
	AOBWeaponBase* Weapon = GetEquippedWeapon();
	if (!Character || !Weapon) return;

	UOBWeaponData* WeaponData = Weapon->GetWeaponData();
	if (!WeaponData) return;

	FVector ViewLocation;
	FRotator ViewRotation;
	Character->GetActorEyesViewPoint(ViewLocation, ViewRotation);

	const FVector TraceStart = ViewLocation;
	const FVector TraceEnd = TraceStart + ViewRotation.Vector() * WeaponData->Range;

	// 사격 트레이스: Weapon 채널(캐릭터/벽 Block, 카메라 프로브와 분리).
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(OBWeaponTrace), /*bTraceComplex=*/true);
	QueryParams.AddIgnoredActor(Character);
	QueryParams.AddIgnoredActor(Weapon);

	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit, TraceStart, TraceEnd, OB_TraceChannel_Weapon, QueryParams
	);

#if ENABLE_DRAW_DEBUG
	const FVector DebugEnd = bHit ? Hit.ImpactPoint : TraceEnd;
	if (bDrawDebugTrace)
	{
		// 서버에서 호출 → 모든 클라이언트가 동일하게 그린다.
		Character->Multicast_DrawFireTrace(TraceStart, DebugEnd, bHit);
	}
#endif
	
	// 소스 ASC를 먼저 확보(발사 큐는 명중 여부와 무관하게 발생).
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();

	// --- 발사 큐: 총구 화염 + 사격음 (매 발사) ---
	if (SourceASC)
	{
		FGameplayCueParameters FireCueParams;
		FireCueParams.Location = Weapon->GetMuzzleLocation(); // 머즐 위치(큐에 실어 복제)
		FireCueParams.Instigator = Character;
		FireCueParams.SourceObject = Weapon;
		SourceASC->ExecuteGameplayCue(OBGameplayTags::GameplayCue_Weapon_Fire, FireCueParams);
	}
	
	// 발사 반동 몽타주(모든 클라에 복제). 명중 여부와 무관.
	if (WeaponData->FireMontage)
	{
		Character->Multicast_PlayFireMontage(WeaponData->FireMontage);
	}

	if (!bHit || !Hit.GetActor()) return; // 빗나감: 발사 큐만 재생하고 종료.
	
	// --- 피격 큐: 탄착 이펙트 (명중 지점) ---
	if (SourceASC)
	{
		FGameplayCueParameters ImpactCueParams;
		ImpactCueParams.Location = Hit.ImpactPoint;      // 탄착 위치
		ImpactCueParams.Normal = Hit.ImpactNormal;			// 표면 방향(이펙트 회전용)
		ImpactCueParams.PhysicalMaterial = Hit.PhysMaterial;
		SourceASC->ExecuteGameplayCue(OBGameplayTags::GameplayCue_Weapon_Impact, ImpactCueParams);
	}

	// --- 데미지 적용 ---
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Hit.GetActor());

	if (!TargetASC || !WeaponData->DamageEffect) return;

	// 데미지 GE 스펙 생성 + SetByCaller로 무기 데미지 주입.
	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddSourceObject(Weapon);
	Context.AddHitResult(Hit);

	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(WeaponData->DamageEffect, GetAbilityLevel(), Context);

	if (SpecHandle.IsValid())
	{
		SpecHandle.Data->SetSetByCallerMagnitude(OBGameplayTags::SetByCaller_Damage, WeaponData->BaseDamage);
		SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data, TargetASC);
	}
}