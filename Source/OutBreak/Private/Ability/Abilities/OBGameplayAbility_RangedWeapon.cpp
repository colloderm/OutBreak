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
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Ability/Components/OBAbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Player/Controller/OBPlayerController.h"
#include "Player/State/OBPlayerStateBase.h"

UOBGameplayAbility_RangedWeapon::UOBGameplayAbility_RangedWeapon(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 누름당 1회 활성화 → 내부에서 FireMode별 패턴 처리.
	ActivationPolicy = EOBAbilityActivationPolicy::OnInputTriggered;
	
	// 무기 전환 중에는 발사 차단.
	ActivationBlockedTags.AddTag(OBGameplayTags::State_Weapon_Switching);
	ActivationBlockedTags.AddTag(OBGameplayTags::State_UsingConsumable);
}

void UOBGameplayAbility_RangedWeapon::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 코스트/쿨다운 커밋(설정돼 있으면 적용). 실패 시 능력 종료.
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicate=*/true, /*bWasCancelled=*/true);
		return;
	}
	
	CurrentFireMode = GetFireMode();
	ShotsFired = 0;

	// 첫 발 즉시.
	FireOneShot();
	++ShotsFired;

	// 단발: 1발 후 종료. (홀드해도 OnInputTriggered라 재발동 안 됨)
	if (CurrentFireMode == EOBWeaponFireMode::Single)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// 점사/연사: RPM 간격으로 반복.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			FireTimerHandle, this, &UOBGameplayAbility_RangedWeapon::FireLoop, GetFireInterval(), true);
	}

	// 연사: 입력 뗌을 기다려 종료(커스텀 ASC 파이프라인 필요).
	if (CurrentFireMode == EOBWeaponFireMode::FullAuto)
	{
		UAbilityTask_WaitInputRelease* WaitRelease =
			UAbilityTask_WaitInputRelease::WaitInputRelease(this, /*bTestAlreadyReleased=*/false);
		WaitRelease->OnRelease.AddDynamic(this, &UOBGameplayAbility_RangedWeapon::OnFireInputReleased);
		WaitRelease->ReadyForActivation();
	}
}

void UOBGameplayAbility_RangedWeapon::FireLoop()
{
	// 연사 중 입력 뗌 체크는 "로컬 조종" 머신에서만.
	// (서버의 원격 클라 인스턴스는 InputHeld가 없으므로 제외)
	if (CurrentFireMode == EOBWeaponFireMode::FullAuto && CurrentActorInfo && CurrentActorInfo->IsLocallyControlled())
	{
		const UOBAbilitySystemComponent* ASC = Cast<UOBAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
		if (!ASC || !ASC->IsAbilityInputHeld(CurrentSpecHandle))
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, false);
			return;
		}
	}
	
	FireOneShot();
	++ShotsFired;

	if (CurrentFireMode == EOBWeaponFireMode::Burst && ShotsFired >= GetBurstCount())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UOBGameplayAbility_RangedWeapon::OnFireInputReleased(float TimeHeld)
{
	// 연사: 입력 떼면 종료.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UOBGameplayAbility_RangedWeapon::EndAbility(
	const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 반복 타이머 정리.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FireTimerHandle);
	}
	
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UOBGameplayAbility_RangedWeapon::FireOneShot()
{
	AOBWeaponBase* Weapon = GetEquippedWeapon();

	// 탄약 없음 → 발사 중단(연사/점사 종료). 탄약은 복제되어 클라도 인지.
	if (!Weapon || !Weapon->HasAmmo())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// 탄약 소모(서버 권위).
	if (HasAuthority(&CurrentActivationInfo))
	{
		Weapon->ConsumeAmmo(1);
	}
	
	// 반동/카메라 쉐이크: 소유 클라. 조준 중이면 감소
	if (CurrentActorInfo && CurrentActorInfo->IsLocallyControlled())
	{
		if (UOBWeaponData* Data = Weapon->GetWeaponData())
		{
			if (AOBCharacterBase* Char = GetOBCharacterFromActorInfo())
			{
				Char->NotifyFired();
				
				// 조준 중이면 반동 배율 적용.
				const float RecoilMult = Char->IsAiming() ? Data->ADSRecoilMultiplier : 1.0f;
				
				if (AOBPlayerController* PC = Cast<AOBPlayerController>(Char->GetController()))
				{
					PC->ApplyWeaponRecoil(
						Data->VerticalRecoil * RecoilMult,
						Data->HorizontalRecoil * RecoilMult,
						Data->RecoilRecoverySpeed,
						Data->FireCameraShake,
						RecoilMult);
					
					Char->AddFireFocusPulse(Data->FireFocusPulse);  // 화면 집중 펄스
				}
			}
		}
	}

	// 트레이스/데미지/큐/몽타주: 서버에서만 수행.
	if (HasAuthority(&CurrentActivationInfo))
	{
		PerformServerWeaponTrace();
	}
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

EOBWeaponFireMode UOBGameplayAbility_RangedWeapon::GetFireMode() const
{
	if (AOBWeaponBase* Weapon = GetEquippedWeapon())
	{
		if (UOBWeaponData* Data = Weapon->GetWeaponData())
		{
			return Data->FireMode;
		}
	}
	return EOBWeaponFireMode::Single;
}

int32 UOBGameplayAbility_RangedWeapon::GetBurstCount() const
{
	if (AOBWeaponBase* Weapon = GetEquippedWeapon())
	{
		if (UOBWeaponData* Data = Weapon->GetWeaponData())
		{
			return FMath::Max(1, Data->BurstCount);
		}
	}
	return 3;
}

float UOBGameplayAbility_RangedWeapon::GetFireInterval() const
{
	if (AOBWeaponBase* Weapon = GetEquippedWeapon())
	{
		if (UOBWeaponData* Data = Weapon->GetWeaponData())
		{
			return (Data->RoundsPerMinute > 0.0f) ? (60.0f / Data->RoundsPerMinute) : 0.1f;
		}
	}
	return 0.1f;
}

float UOBGameplayAbility_RangedWeapon::GetCurrentSpreadAngle() const
{
	const AOBCharacterBase* Character = GetOBCharacterFromActorInfo();
	return Character ? Character->GetCurrentSpreadAngle() : 0.f;
}

void UOBGameplayAbility_RangedWeapon::PerformServerWeaponTrace()
{
	AOBCharacterBase* Character = GetOBCharacterFromActorInfo();
	AOBWeaponBase* Weapon = GetEquippedWeapon();
	if (!Character || !Weapon) return;

	UOBWeaponData* WeaponData = Weapon->GetWeaponData();
	if (!WeaponData) return;

	// 1) 카메라에서 전방으로 트레이스해 실제 조준점을 구한다(크로스헤어=화면 중앙과 일치).
	FVector CamLoc;
	FRotator CamRot;
	if (UCameraComponent* Cam = Character->GetFollowCamera())
	{
		CamLoc = Cam->GetComponentLocation();
		CamRot = Cam->GetComponentRotation();
	}
	else
	{
		Character->GetActorEyesViewPoint(CamLoc, CamRot); // 폴백
	}
	
	const FVector CamEnd = CamLoc + CamRot.Vector() * WeaponData->Range;
	
	FCollisionQueryParams AimParams(SCENE_QUERY_STAT(OBAimProbe), /*bTraceComplex=*/true);
	AimParams.AddIgnoredActor(Character);
	AimParams.AddIgnoredActor(Weapon);
	
	// 카메라~머즐 사이 벽에 맞으면 조준점을 그 지점으로. 아니면 먼 지점.
	FHitResult AimHit;
	FVector AimPoint = GetWorld()->LineTraceSingleByChannel(
		AimHit, CamLoc, CamEnd, OB_TraceChannel_Weapon, AimParams) 
		? AimHit.ImpactPoint : CamEnd;
	
	// 2) 실제 탄환은 머즐에서 조준점으로. 여기에 스프레드 콘 적용.
	const FVector MuzzleLoc = Weapon->GetMuzzleLocation();
	
	// 카메라가 벽에 끼면 조준점이 머즐 뒤에 잡혀 탄이 뒤로 나간다. 카메라 전방으로 폴백.
	if (FVector::DotProduct(AimPoint - MuzzleLoc, CamRot.Vector()) <= 0.f)
	{
		AimPoint = MuzzleLoc + CamRot.Vector() * WeaponData->Range;
	}
	
	const FVector AimDir = (AimPoint - MuzzleLoc).GetSafeNormal();
	
	const float SpreadRadians = FMath::DegreesToRadians(GetCurrentSpreadAngle());
	
	// 벽에 밀착하면 무기 메시가 벽을 관통해 머즐이 벽 너머에 놓인다.
	// 몸통→머즐 구간이 막혀 있으면 시작점을 몸통으로 당겨 탄이 벽에 정상적으로 박히게 한다.
	const FVector BodyOrigin = Character->GetActorLocation();
	FHitResult MuzzleBlock;
	const bool bMuzzleBlocked = GetWorld()->LineTraceSingleByChannel(
		MuzzleBlock, BodyOrigin, MuzzleLoc, OB_TraceChannel_Weapon, AimParams);
	
	const FVector TraceStart = bMuzzleBlocked ? BodyOrigin : MuzzleLoc;

	// 사격 트레이스: Weapon 채널(캐릭터/벽 Block, 카메라 프로브와 분리).
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(OBWeaponTrace), /*bTraceComplex=*/true);
	QueryParams.AddIgnoredActor(Character);
	QueryParams.AddIgnoredActor(Weapon);
	
	QueryParams.bReturnPhysicalMaterial = true;
	
	// 소스 ASC를 먼저 확보(발사 큐는 명중 여부와 무관하게 발생).
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	
	// --- 발사 큐: 총구 화염 + 사격음. 탄자 수와 무관하게 1회 ---
	if (SourceASC)
	{
		FGameplayCueParameters FireCueParams;
		FireCueParams.Location = MuzzleLoc;
		FireCueParams.Instigator = Character;
		FireCueParams.SourceObject = Weapon;
		SourceASC->ExecuteGameplayCue(OBGameplayTags::GameplayCue_Weapon_Fire, FireCueParams);
	}
	
	// 발사 반동 몽타주(모든 클라에 복제). 명중 여부와 무관.
	if (WeaponData->AttackMontage)
	{
		Character->Multicast_PlayFireMontage(WeaponData->AttackMontage);
	}
	
	// --- 탄자 루프: 샷건은 1회 발사에 여러 발이 각자 퍼진다 ---
	// ponytail: 탄자마다 임팩트 큐를 개별 멀티캐스트. 8발이면 8회 — 대역폭이 문제되면 큐 1회로 묶을 것.
	const int32 Pellets = FMath::Max(1, WeaponData->PelletsPerShot);
	for (int32 PelletIndex = 0; PelletIndex < Pellets; ++PelletIndex)
	{
		const FVector ShotDirection = (SpreadRadians > 0.f) ? FMath::VRandCone(AimDir, SpreadRadians) : AimDir;
		const FVector TraceEnd = TraceStart + ShotDirection * WeaponData->Range;
		
		FHitResult Hit;
		const bool bHit = GetWorld()->LineTraceSingleByChannel(
			Hit, TraceStart, TraceEnd, OB_TraceChannel_Weapon, QueryParams);
		
		AActor* HitActor = Hit.GetActor();

		if (IsValid(HitActor))
		{
			UGameplayStatics::ApplyPointDamage(
				HitActor,
				WeaponData->BaseDamage,
				ShotDirection,
				Hit,
				nullptr,
				nullptr,
				UDamageType::StaticClass());
		}

#if ENABLE_DRAW_DEBUG
		if (bDrawDebugTrace)
		{
			// 서버에서 호출 → 모든 클라이언트가 동일하게 그린다.
			const FVector DebugEnd = bHit ? Hit.ImpactPoint : TraceEnd;
			Character->Multicast_DrawFireTrace(TraceStart, DebugEnd, bHit);
		}
#endif

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
	
		// 같은 팀이면 무기 피해 무시(탄착 이펙트는 이미 재생됨).
		if (AOBPlayerStateBase::AreSameTeam(Character, Hit.GetActor())) return;

		// --- 데미지 적용 ---
		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Hit.GetActor());
		if (!TargetASC || !WeaponData->DamageEffect || !SourceASC) continue;

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
}
