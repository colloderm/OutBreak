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
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Ability/Components/OBAbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameplayPrediction.h"
#include "Kismet/GameplayStatics.h"
#include "Player/Controller/OBPlayerController.h"
#include "Player/State/OBPlayerStateBase.h"
#include "AI/EnemyCharacter.h"

DEFINE_LOG_CATEGORY_STATIC(LogOBWeaponAim, Log, All);

namespace
{
	// UE 마네킹 스켈레톤 기준. 다른 스켈레톤을 쓰는 적이 생기면 데이터로 뺀다.
	bool IsHeadBone(const FName BoneName)
	{
		return BoneName == TEXT("head") || BoneName == TEXT("neck_01");
	}

	bool HasFireBlockingState(
		const AOBCharacterBase* Character,
		const UAbilitySystemComponent* AbilitySystem)
	{
		return !Character
			|| Character->IsDead()
			|| Character->IsDowned()
			|| (AbilitySystem
				&& (AbilitySystem->HasMatchingGameplayTag(OBGameplayTags::State_HelicopterTransit)
					|| AbilitySystem->HasMatchingGameplayTag(OBGameplayTags::State_Dead)
					|| AbilitySystem->HasMatchingGameplayTag(OBGameplayTags::State_Downed)));
	}
}

UOBGameplayAbility_RangedWeapon::UOBGameplayAbility_RangedWeapon(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 누름당 1회 활성화 → 내부에서 FireMode별 패턴 처리.
	ActivationPolicy = EOBAbilityActivationPolicy::OnInputTriggered;
	
	// 무기 전환 중에는 발사 차단.
	ActivationBlockedTags.AddTag(OBGameplayTags::State_Weapon_Switching);
	ActivationBlockedTags.AddTag(OBGameplayTags::State_UsingConsumable);
	ActivationBlockedTags.AddTag(OBGameplayTags::State_HelicopterTransit);
	ActivationBlockedTags.AddTag(OBGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(OBGameplayTags::State_Downed);
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
	PendingServerAimShots = 0;
	bRemoteAimDelegateBound = false;

	const bool bRemoteAuthoritativeInstance =
		HasAuthority(&ActivationInfo) && ActorInfo && !ActorInfo->IsLocallyControlled();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (bRemoteAuthoritativeInstance && ASC)
	{
		ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey())
			.AddUObject(this, &UOBGameplayAbility_RangedWeapon::HandleAimTargetData);
		bRemoteAimDelegateBound = true;
	}

	// 첫 발 즉시. 요청을 만들지 못했다면 FireOneShot이 능력을 종료한다.
	if (!FireOneShot())
	{
		return;
	}
	++ShotsFired;

	// Target data may have reached the PlayerState ASC before the server ability
	// finished activating. Consume it only after FireOneShot issued the matching
	// authoritative shot token.
	if (bRemoteAuthoritativeInstance && ASC)
	{
		ASC->CallReplicatedTargetDataDelegatesIfSet(
			Handle,
			ActivationInfo.GetActivationPredictionKey());
	}

	// 단발: 1발 후 종료. (홀드해도 OnInputTriggered라 재발동 안 됨)
	if (CurrentFireMode == EOBWeaponFireMode::Single)
	{
		// A remote server instance stays alive until its prediction-key target data
		// arrives. The locally predicted client sends data before ending, so RPC
		// ordering on the ASC guarantees target data precedes the replicated end.
		if (!bRemoteAuthoritativeInstance)
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		}
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
	
	if (!FireOneShot())
	{
		return;
	}
	++ShotsFired;

	if (CurrentFireMode == EOBWeaponFireMode::Burst && ShotsFired >= GetBurstCount())
	{
		const bool bRemoteServerStillWaitingForAim = bRemoteAimDelegateBound
			&& PendingServerAimShots > 0;
		if (!bRemoteServerStillWaitingForAim)
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		}
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
	if (bRemoteAimDelegateBound)
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey())
				.RemoveAll(this);
		}
		bRemoteAimDelegateBound = false;
	}
	PendingServerAimShots = 0;

	// 반복 타이머 정리.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FireTimerHandle);
		World->GetTimerManager().ClearTimer(AimTargetDataTimeoutHandle);
	}
	
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UOBGameplayAbility_RangedWeapon::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
		return false;

		// 스프린트 중에는 발사 시작 불가.
	if (IsOwnerSprinting()) return false;
	return true;
}

bool UOBGameplayAbility_RangedWeapon::FireOneShot()
{
	// 발사 도중 스프린트를 누르면 연사/점사를 즉시 끊는다.
	// CanActivateAbility는 시작만 막으므로 매 발마다 여기서 다시 본다.
	if (IsOwnerSprinting())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return false;
	}

	AOBCharacterBase* Character = GetOBCharacterFromActorInfo();
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	if (HasFireBlockingState(Character, AbilitySystem))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return false;
	}
	
	AOBWeaponBase* Weapon = GetEquippedWeapon();

	// 탄약 없음 → 발사 중단(연사/점사 종료). 탄약은 복제되어 클라도 인지.
	if (!Weapon || !Weapon->HasAmmo())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return false;
	}

	// Remote full-auto can have one or more server shot requests waiting for
	// client TargetData. Treat those requests as reservations so latency cannot
	// queue more shots than the magazine can authorize.
	if (HasAuthority(&CurrentActivationInfo)
		&& Weapon->GetCurrentAmmo() <= PendingServerAimShots)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return false;
	}

	// Capture the evaluated gameplay view before recoil mutates ControlRotation.
	// GetPlayerViewPoint resolves PlayerCameraManager, including Gameplay Cameras'
	// transient output camera, instead of the unused native FollowCamera component.
	FGameplayAbilityTargetDataHandle LocalAimTargetData;
	const bool bHasLocalAimTargetData = CurrentActorInfo
		&& CurrentActorInfo->IsLocallyControlled()
		&& BuildLocalAimTargetData(LocalAimTargetData);

	if (CurrentActorInfo && CurrentActorInfo->IsLocallyControlled() && !bHasLocalAimTargetData)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return false;
	}

	// 서버는 여기서 탄약을 소모하지 않는다. 한 발의 승인 토큰만 예약하고,
	// TargetData 검증이 끝난 CommitServerShot에서 탄약/연출/피해를 함께 커밋한다.
	if (HasAuthority(&CurrentActivationInfo))
	{
		++PendingServerAimShots;

		const bool bRemoteAuthoritativeInstance = CurrentActorInfo
			&& !CurrentActorInfo->IsLocallyControlled();
		if (bRemoteAuthoritativeInstance)
		{
			if (UWorld* World = GetWorld(); World
				&& !World->GetTimerManager().IsTimerActive(AimTargetDataTimeoutHandle))
			{
				World->GetTimerManager().SetTimer(
					AimTargetDataTimeoutHandle,
					this,
					&UOBGameplayAbility_RangedWeapon::HandleAimTargetDataTimeout,
					FMath::Max(0.1f, AimTargetDataTimeoutSeconds),
					false);
			}
		}
	}
	
	// 반동/카메라 쉐이크: 소유 클라. 조준 중이면 감소
	if (CurrentActorInfo && CurrentActorInfo->IsLocallyControlled())
	{
		if (Weapon->GetResolvedStats().WeaponType == EOBWeaponType::Ranged)
		{
			const FOBResolvedWeaponStats& Stats = Weapon->GetResolvedStats();
			const FOBWeaponDefinitionRow* Definition = Weapon->GetWeaponDefinition();
			if (AOBCharacterBase* Char = GetOBCharacterFromActorInfo())
			{
				Char->NotifyFired();
				
				// 조준 중이면 반동 배율 적용.
				const float RecoilMult = Char->IsAiming() ? Stats.ADSRecoilMultiplier : 1.0f;
				
				if (AOBPlayerController* PC = Cast<AOBPlayerController>(Char->GetController()))
				{
					PC->ApplyWeaponRecoil(
						Stats.VerticalRecoil * RecoilMult,
						Stats.HorizontalRecoil * RecoilMult,
						Stats.RecoilRecoverySpeed,
						Definition ? Definition->Ranged.FireCameraShake : nullptr,
						(Definition ? Definition->Ranged.FireCameraShakeScale : 1.f) * RecoilMult);
					
					Char->AddFireFocusPulse(Stats.FireFocusPulse);
				}
			}
		}
	}

	// Each predicted local shot carries exactly one evaluated camera view. Remote
	// server instances never invent a view from a non-rendering camera component.
	if (bHasLocalAimTargetData)
	{
		SubmitLocalAimTargetData(LocalAimTargetData);
	}

	return true;
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
		return Weapon->GetResolvedStats().FireMode;
	}
	return EOBWeaponFireMode::Single;
}

int32 UOBGameplayAbility_RangedWeapon::GetBurstCount() const
{
	if (AOBWeaponBase* Weapon = GetEquippedWeapon())
	{
		return FMath::Max(1, Weapon->GetResolvedStats().BurstCount);
	}
	return 3;
}

float UOBGameplayAbility_RangedWeapon::GetFireInterval() const
{
	if (AOBWeaponBase* Weapon = GetEquippedWeapon())
	{
		const float RPM = Weapon->GetResolvedStats().RoundsPerMinute;
		return RPM > 0.f ? 60.f / RPM : 0.1f;
	}
	return 0.1f;
}

float UOBGameplayAbility_RangedWeapon::GetCurrentSpreadAngle() const
{
	const AOBCharacterBase* Character = GetOBCharacterFromActorInfo();
	return Character ? Character->GetCurrentSpreadAngle() : 0.f;
}

bool UOBGameplayAbility_RangedWeapon::IsOwnerSprinting() const
{
	const AOBCharacterBase* Char = GetOBCharacterFromActorInfo();
	if (!Char) return false;

	return Char->IsSprintInputHeld() && Char->GetVelocity().SizeSquared2D() > 1.f;
}

bool UOBGameplayAbility_RangedWeapon::BuildLocalAimTargetData(
	FGameplayAbilityTargetDataHandle& OutTargetData) const
{
	const AOBCharacterBase* Character = GetOBCharacterFromActorInfo();
	const APlayerController* PlayerController = Character
		? Cast<APlayerController>(Character->GetController())
		: nullptr;
	const AOBWeaponBase* Weapon = GetEquippedWeapon();
	if (!Character || !PlayerController || !PlayerController->IsLocalController() || !Weapon
		|| PlayerController->GetViewTarget() != Character)
	{
		return false;
	}

	FVector ViewOrigin;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewOrigin, ViewRotation);
	if (ViewOrigin.ContainsNaN() || ViewRotation.ContainsNaN())
	{
		return false;
	}

	const float Range = FMath::Max(100.f, Weapon->GetResolvedStats().Range);
	auto* AimData = new FGameplayAbilityTargetData_LocationInfo();
	AimData->SourceLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
	AimData->SourceLocation.LiteralTransform = FTransform(ViewRotation, ViewOrigin);
	AimData->TargetLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
	AimData->TargetLocation.LiteralTransform = FTransform(
		ViewRotation,
		ViewOrigin + ViewRotation.Vector() * Range);
	OutTargetData.Add(AimData);

	return true;
}

void UOBGameplayAbility_RangedWeapon::SubmitLocalAimTargetData(
	const FGameplayAbilityTargetDataHandle& TargetData)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || TargetData.Num() == 0)
	{
		return;
	}

	if (HasAuthority(&CurrentActivationInfo))
	{
		HandleAimTargetData(TargetData, FGameplayTag());
		return;
	}

	FScopedPredictionWindow ScopedPrediction(ASC, true);

	// The server receives this target data inside a scoped window carrying this
	// exact per-shot prediction key. Predict the instantaneous fire cue in the
	// same window so the server multicast is correctly de-duplicated only after
	// the owning client has actually played its muzzle flash and fire sound.
	//
	// Previously only recoil/camera shake was predicted. GAS still suppressed the
	// server FireCue on the owning client because the multicast carried this local
	// key, which left remote shooters with camera shake but no muzzle presentation.
	if (AOBWeaponBase* Weapon = GetEquippedWeapon())
	{
		FGameplayCueParameters FireCueParams;
		FireCueParams.Location = Weapon->GetMuzzleLocation();
		FireCueParams.Instigator = GetOBCharacterFromActorInfo();
		FireCueParams.SourceObject = Weapon;
		ASC->ExecuteGameplayCue(OBGameplayTags::GameplayCue_Weapon_Fire, FireCueParams);
	}

	ASC->CallServerSetReplicatedTargetData(
		CurrentSpecHandle,
		CurrentActivationInfo.GetActivationPredictionKey(),
		TargetData,
		FGameplayTag(),
		ASC->ScopedPredictionKey);
}

void UOBGameplayAbility_RangedWeapon::HandleAimTargetData(
	const FGameplayAbilityTargetDataHandle& TargetData,
	FGameplayTag ActivationTag)
{
	(void)ActivationTag;
	if (!HasAuthority(&CurrentActivationInfo))
	{
		return;
	}

	FGameplayAbilityTargetDataHandle ReceivedData = TargetData;
	if (bRemoteAimDelegateBound)
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->ConsumeClientReplicatedTargetData(
				CurrentSpecHandle,
				CurrentActivationInfo.GetActivationPredictionKey());
		}
	}

	if (PendingServerAimShots <= 0)
	{
		return;
	}
	--PendingServerAimShots;

	const FGameplayAbilityTargetData* AimData = ReceivedData.Num() > 0 ? ReceivedData.Get(0) : nullptr;
	if (!AimData || !AimData->HasOrigin() || !AimData->HasEndPoint())
	{
		UE_LOG(LogOBWeaponAim, Warning,
			TEXT("[WeaponAim] Rejected malformed target data Character=%s DataCount=%d"),
			*GetNameSafe(GetOBCharacterFromActorInfo()), ReceivedData.Num());
	}
	else
	{
		const FVector ViewOrigin = AimData->GetOrigin().GetLocation();
		const FVector ViewDirection = (AimData->GetEndPoint() - ViewOrigin).GetSafeNormal();
		CommitServerShot(ViewOrigin, ViewDirection);
	}

	if (bRemoteAimDelegateBound)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(AimTargetDataTimeoutHandle);
			if (PendingServerAimShots > 0)
			{
				World->GetTimerManager().SetTimer(
					AimTargetDataTimeoutHandle,
					this,
					&UOBGameplayAbility_RangedWeapon::HandleAimTargetDataTimeout,
					FMath::Max(0.1f, AimTargetDataTimeoutSeconds),
					false);
			}
		}
	}

	const bool bRemoteSingleComplete = CurrentFireMode == EOBWeaponFireMode::Single;
	const bool bRemoteBurstComplete = CurrentFireMode == EOBWeaponFireMode::Burst
		&& ShotsFired >= GetBurstCount()
		&& PendingServerAimShots == 0;
	if (bRemoteAimDelegateBound && (bRemoteSingleComplete || bRemoteBurstComplete) && IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UOBGameplayAbility_RangedWeapon::HandleAimTargetDataTimeout()
{
	if (!HasAuthority(&CurrentActivationInfo) || !IsActive() || PendingServerAimShots <= 0)
	{
		return;
	}

	// No ammo has been consumed for these pending requests, so cancellation does
	// not require a refund and cannot leave a half-fired authoritative shot.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

bool UOBGameplayAbility_RangedWeapon::CommitServerShot(
	const FVector& ViewOrigin,
	const FVector& ViewDirection)
{
	AOBCharacterBase* Character = GetOBCharacterFromActorInfo();
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	if (HasFireBlockingState(Character, AbilitySystem))
	{
		return false;
	}

	AOBWeaponBase* Weapon = GetEquippedWeapon();
	if (!Character || !Weapon || ViewOrigin.ContainsNaN() || ViewDirection.ContainsNaN()
		|| ViewDirection.IsNearlyZero())
	{
		return false;
	}

	const FOBResolvedWeaponStats& Stats = Weapon->GetResolvedStats();
	const FOBWeaponDefinitionRow* WeaponDefinition = Weapon->GetWeaponDefinition();
	if (Stats.WeaponType != EOBWeaponType::Ranged || !WeaponDefinition)
	{
		return false;
	}

	const FVector NormalizedViewDirection = ViewDirection.GetSafeNormal();
	const FVector PawnViewOrigin = Character->GetPawnViewLocation();
	const float CameraDistance = FVector::Distance(ViewOrigin, Character->GetActorLocation());
	const FVector ServerAimDirection = Character->GetBaseAimRotation().Vector().GetSafeNormal();
	const float AimDot = FMath::Clamp(
		FVector::DotProduct(NormalizedViewDirection, ServerAimDirection),
		-1.f,
		1.f);
	const float AimErrorDegrees = FMath::RadiansToDegrees(FMath::Acos(AimDot));
	if (CameraDistance > FMath::Max(100.f, MaxValidatedCameraDistance)
		|| AimErrorDegrees > FMath::Clamp(MaxValidatedAimAngleDegrees, 0.f, 89.f))
	{
		return false;
	}

	FCollisionQueryParams CameraValidationParams(
		SCENE_QUERY_STAT(OBCameraOriginValidation),
		/*bTraceComplex=*/true);
	CameraValidationParams.AddIgnoredActor(Character);
	CameraValidationParams.AddIgnoredActor(Weapon);
	FHitResult CameraObstruction;
	if (GetWorld()->LineTraceSingleByChannel(
		CameraObstruction,
		PawnViewOrigin,
		ViewOrigin,
		OB_TraceChannel_CameraProbe,
		CameraValidationParams)
		&& FVector::DistSquared(CameraObstruction.ImpactPoint, ViewOrigin) > FMath::Square(20.f))
	{
		return false;
	}
	
	const FVector CamEnd = ViewOrigin + NormalizedViewDirection * Stats.Range;
	
	FCollisionQueryParams AimParams(SCENE_QUERY_STAT(OBAimProbe), /*bTraceComplex=*/true);
	AimParams.AddIgnoredActor(Character);
	AimParams.AddIgnoredActor(Weapon);
	
	// 카메라~머즐 사이 벽에 맞으면 조준점을 그 지점으로. 아니면 먼 지점.
	FHitResult AimHit;
	FVector AimPoint = GetWorld()->LineTraceSingleByChannel(
		AimHit, ViewOrigin, CamEnd, OB_TraceChannel_Weapon, AimParams)
		? AimHit.ImpactPoint : CamEnd;
	
	// 2) 실제 탄환은 머즐에서 조준점으로. 여기에 스프레드 콘 적용.
	const FVector MuzzleLoc = Weapon->GetMuzzleLocation();
	
	// 카메라가 벽에 끼면 조준점이 머즐 뒤에 잡혀 탄이 뒤로 나간다. 카메라 전방으로 폴백.
	if (FVector::DotProduct(AimPoint - MuzzleLoc, NormalizedViewDirection) <= 0.f)
	{
		AimPoint = MuzzleLoc + NormalizedViewDirection * Stats.Range;
	}
	
	const FVector AimDir = (AimPoint - MuzzleLoc).GetSafeNormal();
	
	const float SpreadRadians = FMath::DegreesToRadians(GetCurrentSpreadAngle());
	
	// 탄도는 항상 총구에서 시작한다. 이전 코드는 총구가 벽을 통과하면 시작점을
	// Character::ActorLocation(골반 높이)으로 내렸고, 카메라 조준은 정상이어도
	// 탄환이 캐릭터 뒤쪽에서 발사되는 것처럼 보이는 회귀를 만들었다.
	//
	// 벽 관통 방지는 무기 본체(손/리시버 측)와 총구 사이를 별도로 검사한다.
	// 이 짧은 구간이 막혔으면 골반에서 새 trace를 쏘지 않고 그 차폐면을
	// authoritative immediate hit으로 사용한다.
	const USkeletalMeshComponent* WeaponMesh = Weapon->GetWeaponMesh();
	const FVector WeaponOrigin = WeaponMesh
		? WeaponMesh->GetComponentLocation()
		: Weapon->GetActorLocation();
	FHitResult MuzzleObstruction;
	const bool bMuzzleObstructed = !WeaponOrigin.Equals(MuzzleLoc, 1.f)
		&& GetWorld()->LineTraceSingleByChannel(
			MuzzleObstruction,
			WeaponOrigin,
			MuzzleLoc,
			OB_TraceChannel_Weapon,
			AimParams);
	const FVector TraceStart = MuzzleLoc;

	// 사격 트레이스: Weapon 채널(캐릭터/벽 Block, 카메라 프로브와 분리).
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(OBWeaponTrace), /*bTraceComplex=*/true);
	QueryParams.AddIgnoredActor(Character);
	QueryParams.AddIgnoredActor(Weapon);
	
	QueryParams.bReturnPhysicalMaterial = true;
	
	// 모든 검증과 조준점 산출이 끝난 뒤에만 승인된 한 발을 커밋한다.
	// 이 지점 전에는 탄약, 서버 Cue, 몽타주, 피해 중 어떤 것도 변경하지 않는다.
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!Weapon->HasAmmo() || !SourceASC)
	{
		return false;
	}

	const int32 AmmoBeforeShot = Weapon->GetCurrentAmmo();
	Weapon->ConsumeAmmo(1);

	// --- 발사 큐: 총구 화염 + 사격음. 탄자 수와 무관하게 1회 ---
	FGameplayCueParameters FireCueParams;
	FireCueParams.Location = MuzzleLoc;
	FireCueParams.Instigator = Character;
	FireCueParams.SourceObject = Weapon;
	SourceASC->ExecuteGameplayCue(OBGameplayTags::GameplayCue_Weapon_Fire, FireCueParams);
	
	// 발사 반동 몽타주(모든 클라에 복제). 명중 여부와 무관.
	UAnimMontage* AttackMontage = WeaponDefinition->Visual.AttackMontage.LoadSynchronous();
	if (AttackMontage)
	{
		Character->Multicast_PlayFireMontage(AttackMontage);
	}
	
	// --- 탄자 루프: 샷건은 1회 발사에 여러 발이 각자 퍼진다 ---
	// ponytail: 탄자마다 임팩트 큐를 개별 멀티캐스트. 8발이면 8회 — 대역폭이 문제되면 큐 1회로 묶을 것.
	const int32 Pellets = FMath::Max(1, Stats.PelletsPerShot);
	for (int32 PelletIndex = 0; PelletIndex < Pellets; ++PelletIndex)
	{
		const FVector ShotDirection = (SpreadRadians > 0.f) ? FMath::VRandCone(AimDir, SpreadRadians) : AimDir;
		const FVector TraceEnd = TraceStart + ShotDirection * Stats.Range;
		
		FHitResult Hit;
		const bool bHit = bMuzzleObstructed
			? (Hit = MuzzleObstruction, true)
			: GetWorld()->LineTraceSingleByChannel(
				Hit, TraceStart, TraceEnd, OB_TraceChannel_Weapon, QueryParams);
		
		AActor* HitActor = Hit.GetActor();

		if (IsValid(HitActor))
		{
			UGameplayStatics::ApplyPointDamage(
				HitActor,
				Stats.Damage,
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

		if (!bHit || !Hit.GetActor()) continue; // 빗나감: 발사 큐만 재생하고 종료.
	
		// --- 피격 큐: 탄착 이펙트 (명중 지점) ---
		if (SourceASC)
		{
			FGameplayCueParameters ImpactCueParams;
			ImpactCueParams.Location = Hit.ImpactPoint;      // 탄착 위치
			ImpactCueParams.Normal = Hit.ImpactNormal;			// 표면 방향(이펙트 회전용)
			ImpactCueParams.PhysicalMaterial = Hit.PhysMaterial;

			// Impact is authoritative-only: the client cannot know the validated
			// server hit when it predicts the trigger pull. Clear the target-data
			// prediction key for this multicast so the owning client does not discard
			// an impact cue it never predicted locally.
			FScopedPredictionWindow ServerImpactCueScope(SourceASC, FPredictionKey(), false);
			SourceASC->ExecuteGameplayCue(OBGameplayTags::GameplayCue_Weapon_Impact, ImpactCueParams);
		}
	
		// 같은 팀이면 무기 피해 무시(탄착 이펙트는 이미 재생됨).
		if (AOBPlayerStateBase::AreSameTeam(Character, Hit.GetActor())) continue;

		// --- 데미지 적용 ---
		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Hit.GetActor());
		if (!TargetASC || !WeaponDefinition->Common.DamageEffect || !SourceASC) continue;

		// 데미지 GE 스펙 생성 + SetByCaller로 무기 데미지 주입.
		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		Context.AddSourceObject(Weapon);
		Context.AddHitResult(Hit);

		FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(WeaponDefinition->Common.DamageEffect, GetAbilityLevel(), Context);

		if (SpecHandle.IsValid())
		{
			float FinalDamage = Stats.Damage;

			const bool bHeadshot =
				Stats.HeadshotMultiplier > 1.f && IsHeadBone(Hit.BoneName);
			if (bHeadshot)
			{
				FinalDamage *= Stats.HeadshotMultiplier;
			}

			SpecHandle.Data->SetSetByCallerMagnitude(OBGameplayTags::SetByCaller_Damage, FinalDamage);
			SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data, TargetASC);
			
			// 사망 시 래그돌이 맞은 방향으로 쓰러지도록 기록.
			// ponytail: 피격 대상 타입이 셋째로 늘어나면 인터페이스로 뺀다. 둘이면 캐스트가 더 싸다.
			if (AOBCharacterBase* HitChar = Cast<AOBCharacterBase>(Hit.GetActor()))
			{
				HitChar->NotifyHitForRagdoll(Hit.BoneName, ShotDirection);
			}
			else if (AEnemyCharacter* HitEnemy = Cast<AEnemyCharacter>(Hit.GetActor()))
			{
				HitEnemy->NotifyHitForRagdoll(Hit.BoneName, ShotDirection);
			}
		}
	}

	return true;
}
