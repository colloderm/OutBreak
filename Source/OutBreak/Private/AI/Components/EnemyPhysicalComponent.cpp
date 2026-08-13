// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Components/EnemyPhysicalComponent.h"

#include "AI/EnemyCharacter.h"
#include "AI/Components/EnemyMovementComponent.h"
#include "AI/Components/EnemyStatusComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/StaticMeshActor.h"

#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace
{
const FName HeadLimbName(TEXT("Head"));
const FName RightArmLimbName(TEXT("upperarm_r"));
const FName LeftArmLimbName(TEXT("upperarm_l"));
const FName RightLegLimbName(TEXT("thigh_r"));
const FName LeftLegLimbName(TEXT("thigh_l"));

bool IsCrawlingLocomotionState(
	const ELocomotionWalkRunState State)
{
	return State == ELocomotionWalkRunState::Crawling ||
		State == ELocomotionWalkRunState::SlowCrawling;
}

bool IsUpperBodyHitReactRegion(const EEnemyHitReactRegion Region)
{
	return Region == EEnemyHitReactRegion::Head ||
		Region == EEnemyHitReactRegion::Torso ||
		Region == EEnemyHitReactRegion::ArmRight ||
		Region == EEnemyHitReactRegion::ArmLeft;
}
}

// Sets default values for this component's properties
UEnemyPhysicalComponent::UEnemyPhysicalComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	
	// 부위 파괴 상태를 클라에 전달한다.
	SetIsReplicatedByDefault(true);
}


// Called when the game starts
void UEnemyPhysicalComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeHealthStateFromSettings();

	AEnemyCharacter* OwnerCharacter = GetEnemyCharacter();
	if (!IsValid(OwnerCharacter) || !IsValid(EnemyAsset))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s::%s: Invalid enemy context. Owner=%s EnemyAsset=%s."),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__),
			*GetNameSafe(OwnerCharacter),
			*GetNameSafe(EnemyAsset));
		SetComponentTickEnabled(false);
		return;
	}

	TargetMesh = OwnerCharacter->GetMesh();
	ProxyMesh = OwnerCharacter->GetChildActorSkeletalMesh();
	if (!IsValid(TargetMesh))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s::%s: Target mesh is invalid on '%s'."),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__),
			*GetNameSafe(OwnerCharacter));
		SetComponentTickEnabled(false);
		return;
	}

	const FEnemyPhysicalReact* PhysicalReact = EnemyAsset->GetPhysicalReact();
	if (PhysicalReact == nullptr)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s::%s: PhysicalReact is unavailable in '%s'."),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__),
			*GetNameSafe(EnemyAsset));
		SetComponentTickEnabled(false);
		return;
	}
	
	if (IsValid(PhysicalReact->ReactCurveFloat))
	{
		FOnTimelineFloat UpdateDelegate;
		UpdateDelegate.BindUFunction(
			this,
			FName(TEXT("HandleReactTimeline")));
		
		
		FOnTimelineEvent FinishedDelegate;
		FinishedDelegate.BindUFunction(
			this,
			FName(TEXT("HandleReactTimelineFinished")));
		
		ReactTimeline.AddInterpFloat(PhysicalReact->ReactCurveFloat, UpdateDelegate);
		ReactTimeline.SetTimelineFinishedFunc(FinishedDelegate);
		
		ReactTimeline.SetTimelineLengthMode(TL_LastKeyFrame);
		ReactTimeline.SetLooping(false);
		ReactTimeline.SetPlayRate(1.f);
	}
	
	if (!ensureAlwaysMsgf(
		PhysicalReact->PM_Head &&
		PhysicalReact->PM_Torso &&
		PhysicalReact->PM_Arm_R &&
		PhysicalReact->PM_Arm_L &&
		PhysicalReact->PM_Leg_R &&
		PhysicalReact->PM_Leg_L,
		TEXT("%s::%s: One or more physical materials are invalid."),
		*GetClass()->GetName(),
		TEXT(__FUNCTION__)))
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s: Physical Material is not set."), *GetClass()->GetName(), TEXT(__FUNCTION__));
	}

	// ...
	
}


// Called every frame
void UEnemyPhysicalComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                 FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ReactTimeline.TickTimeline(DeltaTime);
	DrawDebugLimb();
	// ...
}

void UEnemyPhysicalComponent::SetHealth(const float NewHealth)
{
	Health = FMath::Clamp(
		NewHealth,
		0.0f,
		FMath::Max(1.0f, MaxHealth));
}

float UEnemyPhysicalComponent::GetLimbDurability(
	const FName LimbBoneName) const
{
	const FLimbData* LimbData = Limbes.Find(LimbBoneName);
	return LimbData != nullptr ? LimbData->Durability : 0.0f;
}

float UEnemyPhysicalComponent::GetLimbMaxDurability(
	const FName LimbBoneName) const
{
	const FLimbData* LimbData = Limbes.Find(LimbBoneName);
	return LimbData != nullptr ? LimbData->MaxDurability : 0.0f;
}

void UEnemyPhysicalComponent::InitializeHealthStateFromSettings()
{
	MaxHealth = FMath::Max(1.0f, MaxHealth);
	Head_MaxDurability = FMath::Max(1.0f, Head_MaxDurability);
	Arm_R_MaxDurability = FMath::Max(1.0f, Arm_R_MaxDurability);
	Arm_L_MaxDurability = FMath::Max(1.0f, Arm_L_MaxDurability);
	Leg_R_MaxDurability = FMath::Max(1.0f, Leg_R_MaxDurability);
	Leg_L_MaxDurability = FMath::Max(1.0f, Leg_L_MaxDurability);

	Health = MaxHealth;
	InitializeLimbStateFromSettings();
}

void UEnemyPhysicalComponent::InitializeLimbStateFromSettings()
{
	Limbes.Empty(5);
	Limbes.Emplace(HeadLimbName, FLimbData(Head_MaxDurability));
	Limbes.Emplace(RightArmLimbName, FLimbData(Arm_R_MaxDurability));
	Limbes.Emplace(LeftArmLimbName, FLimbData(Arm_L_MaxDurability));
	Limbes.Emplace(RightLegLimbName, FLimbData(Leg_R_MaxDurability));
	Limbes.Emplace(LeftLegLimbName, FLimbData(Leg_L_MaxDurability));
}

bool UEnemyPhysicalComponent::IsLimbPresent(
	const FName LimbBoneName) const
{
	const FLimbData* LimbData = Limbes.Find(LimbBoneName);
	return LimbData == nullptr || LimbData->bIsHas;
}

void UEnemyPhysicalComponent::SynchronizeLocomotionState()
{
	AEnemyCharacter* OwnerCharacter = GetEnemyCharacter();
	if (!IsValid(OwnerCharacter))
	{
		return;
	}

	if (UEnemyMovementComponent* MovementComponent =
		Cast<UEnemyMovementComponent>(
			OwnerCharacter->GetMovementComponent()))
	{
		MovementComponent->SetLocomotationState(
			EvaluateLocomotionState());
	}
}

void UEnemyPhysicalComponent::ApplyDamage(const float DamageAmount)
{
	AEnemyCharacter* OwnerCharacter = GetEnemyCharacter();
	if (DamageAmount <= 0.0f ||
		!IsValid(OwnerCharacter) ||
		!OwnerCharacter->HasAuthority() ||
		OwnerCharacter->IsDead())
	{
		return;
	}

	Health = FMath::Max(0.0f, Health - DamageAmount);
	if (Health <= 0.0f)
	{
		Action_Dead();
	}
}

void UEnemyPhysicalComponent::ResetForPool()
{
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		return;
	}

	InitializeHealthStateFromSettings();
	StopHitReactPresentation(false);
	bOwnsHitReactActionLock = false;
	DestroyedLimbs.Reset();

	for (const TPair<FName, FLimbData>& Pair : Limbes)
	{
		if (IsValid(ProxyMesh))
		{
			ProxyMesh->UnHideBoneByName(Pair.Key);
		}
	}

	if (IsValid(TargetMesh))
	{
		TargetMesh->SetSimulatePhysics(false);
		TargetMesh->SetAllBodiesSimulatePhysics(false);
		TargetMesh->SetAllBodiesPhysicsBlendWeight(0.0f);
	}

	SynchronizeLocomotionState();
}

void UEnemyPhysicalComponent::ActionPhysical(
	const FHitResult& HitResult,
	const float DamageAmount,
	const FVector& ShotDirection)
{
	AEnemyCharacter* OwnerCharacter = GetEnemyCharacter();
	if (DamageAmount <= 0.0f ||
		!IsValid(OwnerCharacter) ||
		OwnerCharacter->IsDead() ||
		!OwnerCharacter->HasAuthority() ||
		!IsValid(EnemyAsset) ||
		!IsValid(TargetMesh))
	{
		return;
	}
	
	// 서버에서만 판정하고, 연출은 모두에게 보낸다.
	Multicast_BloodVFX(HitResult.ImpactPoint, HitResult.ImpactNormal);

	const FEnemyPhysicalReact* PhysicalReact =
		EnemyAsset->GetPhysicalReact();
	if (PhysicalReact == nullptr)
	{
		return;
	}

	const FName BoneName = HitResult.BoneName;
	const EEnemyHitReactRegion HitRegion =
		ResolveHitReactRegion(HitResult, *PhysicalReact);
	const FName PhysicsBoneName =
		ResolvePhysicsBoneName(HitRegion, BoneName);
	const FVector HitDirection = !ShotDirection.IsNearlyZero()
		? ShotDirection.GetSafeNormal()
		: -HitResult.ImpactNormal.GetSafeNormal();
	
	
	ApplyDamage(DamageAmount);
	if (OwnerCharacter->IsDead())
	{
		return;
	}

	const FName LimbBoneName = ResolveLimbBoneName(HitRegion);
	if (LimbBoneName != NAME_None)
	{
		ActionLimb(LimbBoneName, DamageAmount);
	}

	// Head destruction or a lethal limb combination can enter the dead state
	// inside ActionLimb. Do not restart the temporary hit-reaction timeline,
	// because its finished callback disables skeletal simulation.
	if (OwnerCharacter->IsDead())
	{
		return;
	}
	if (ShouldSkipHitReactPresentation())
	{
		UE_LOG(
			LogTemp,
			VeryVerbose,
			TEXT("%s::%s: Hit presentation skipped during traversal. Actor=%s"),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__),
			*GetNameSafe(OwnerCharacter));
		return;
	}

	if (bIsHit && !PhysicalReact->bRefreshLockOnRepeatedHit)
	{
		UE_LOG(
			LogTemp,
			VeryVerbose,
			TEXT("%s::%s: Active hit-react montage retained."),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__));
		return;
	}

	const bool bIsCrawling = IsCrawlingLocomotionState(
		EvaluateLocomotionState());
	const bool bRequiresMovementLock =
		bIsCrawling || !IsUpperBodyHitReactRegion(HitRegion);
	if (bRequiresMovementLock)
	{
		if (UEnemyStatusComponent* StatusComponent =
			OwnerCharacter->GetEnemyStatusComponent())
		{
			// Lower-body and crawling reactions remain movement-locked. The
			// montage/timeline completion callback owns the normal unlock.
			const EEnemyActionState PreviousActionState =
				StatusComponent->GetActionState();
			StatusComponent->ApplyActionState(EEnemyActionState::Stunned);
			bOwnsHitReactActionLock =
				bOwnsHitReactActionLock ||
				(PreviousActionState != EEnemyActionState::Stunned &&
					StatusComponent->GetActionState() ==
					EEnemyActionState::Stunned);
		}
		OwnerCharacter->StopCharacterMovement();
	}
	else
	{
		// Head, torso and arm montages play through the Upper Body slot, so
		// locomotion is intentionally allowed to continue underneath them.
		ReleaseHitReactActionLock();
	}
	const bool bAllowMontage = !bIsCrawling;

	Multicast_PlayHitReact(
		HitRegion,
		PhysicsBoneName,
		HitDirection,
		bAllowMontage);

	// A missing montage and missing physics curve must never leave the actor
	// permanently action-locked.
	if (!bIsHit)
	{
		ReleaseHitReactActionLock();
	}
}

void UEnemyPhysicalComponent::Multicast_BloodVFX_Implementation(
	const FVector_NetQuantize ImpactPoint,
	const FVector_NetQuantizeNormal ImpactNormal)
{
	// 기존 BloodVFX가 HitResult에서 쓰는 값은 이 둘뿐이다.
	FHitResult Hit;
	Hit.ImpactPoint = ImpactPoint;
	Hit.ImpactNormal = ImpactNormal;

	BloodVFX(Hit);
}

void UEnemyPhysicalComponent::Multicast_PlayHitReact_Implementation(
	const EEnemyHitReactRegion Region,
	const FName PhysicsBoneName,
	const FVector_NetQuantizeNormal ImpulseDirection,
	const bool bAllowMontage)
{
	PlayHitReactPresentation(
		Region,
		PhysicsBoneName,
		ImpulseDirection,
		bAllowMontage);
}

EEnemyHitReactRegion UEnemyPhysicalComponent::ResolveHitReactRegion(
	const FHitResult& HitResult,
	const FEnemyPhysicalReact& PhysicalReact) const
{
	UPhysicalMaterial* HitMaterial = HitResult.PhysMaterial.Get();
	if (IsValid(HitMaterial))
	{
		if (HitMaterial == PhysicalReact.PM_Head.Get())
		{
			return EEnemyHitReactRegion::Head;
		}
		if (HitMaterial == PhysicalReact.PM_Torso.Get())
		{
			return EEnemyHitReactRegion::Torso;
		}
		if (HitMaterial == PhysicalReact.PM_Arm_R.Get())
		{
			return EEnemyHitReactRegion::ArmRight;
		}
		if (HitMaterial == PhysicalReact.PM_Arm_L.Get())
		{
			return EEnemyHitReactRegion::ArmLeft;
		}
		if (HitMaterial == PhysicalReact.PM_Leg_R.Get())
		{
			return EEnemyHitReactRegion::LegRight;
		}
		if (HitMaterial == PhysicalReact.PM_Leg_L.Get())
		{
			return EEnemyHitReactRegion::LegLeft;
		}
	}

	const EEnemyHitReactRegion BoneRegion =
		ResolveHitReactRegionFromBone(HitResult.BoneName);
	return BoneRegion != EEnemyHitReactRegion::None
		? BoneRegion
		: EEnemyHitReactRegion::Torso;
}

EEnemyHitReactRegion
UEnemyPhysicalComponent::ResolveHitReactRegionFromBone(
	const FName BoneName) const
{
	if (BoneName == NAME_None)
	{
		return EEnemyHitReactRegion::None;
	}

	const FString Bone = BoneName.ToString().ToLower();
	if (Bone.Contains(TEXT("head")) || Bone.StartsWith(TEXT("neck")))
	{
		return EEnemyHitReactRegion::Head;
	}
	if (Bone.StartsWith(TEXT("clavicle_r")) ||
		Bone.StartsWith(TEXT("upperarm_r")) ||
		Bone.StartsWith(TEXT("lowerarm_r")) ||
		Bone.StartsWith(TEXT("hand_r")))
	{
		return EEnemyHitReactRegion::ArmRight;
	}
	if (Bone.StartsWith(TEXT("clavicle_l")) ||
		Bone.StartsWith(TEXT("upperarm_l")) ||
		Bone.StartsWith(TEXT("lowerarm_l")) ||
		Bone.StartsWith(TEXT("hand_l")))
	{
		return EEnemyHitReactRegion::ArmLeft;
	}
	if (Bone.StartsWith(TEXT("thigh_r")) ||
		Bone.StartsWith(TEXT("calf_r")) ||
		Bone.StartsWith(TEXT("foot_r")))
	{
		return EEnemyHitReactRegion::LegRight;
	}
	if (Bone.StartsWith(TEXT("thigh_l")) ||
		Bone.StartsWith(TEXT("calf_l")) ||
		Bone.StartsWith(TEXT("foot_l")))
	{
		return EEnemyHitReactRegion::LegLeft;
	}
	if (Bone.StartsWith(TEXT("spine")) || Bone == TEXT("pelvis"))
	{
		return EEnemyHitReactRegion::Torso;
	}

	return EEnemyHitReactRegion::None;
}

UAnimMontage* UEnemyPhysicalComponent::ResolveHitReactMontage(
	const EEnemyHitReactRegion Region,
	const FEnemyPhysicalReact& PhysicalReact) const
{
	switch (Region)
	{
	case EEnemyHitReactRegion::Head:
		return PhysicalReact.Hit_Montage_Head;
	case EEnemyHitReactRegion::ArmRight:
		return PhysicalReact.Hit_Montage_RightShoulder;
	case EEnemyHitReactRegion::ArmLeft:
		return PhysicalReact.Hit_Montage_LeftSholder;
	case EEnemyHitReactRegion::LegRight:
		return PhysicalReact.Hit_Montage_RightLeg;
	case EEnemyHitReactRegion::LegLeft:
		return PhysicalReact.Hit_Montage_LeftLeg;
	case EEnemyHitReactRegion::Torso:
		return PhysicalReact.Hit_Montage_Spine;
	case EEnemyHitReactRegion::None:
	default:
		return nullptr;
	}
}

FName UEnemyPhysicalComponent::ResolvePhysicsBoneName(
	const EEnemyHitReactRegion Region,
	const FName HitBoneName) const
{
	FName Candidate = NAME_None;
	switch (Region)
	{
	case EEnemyHitReactRegion::Head:
		Candidate = HitBoneName != NAME_None
			? HitBoneName
			: FName(TEXT("Head"));
		break;
	case EEnemyHitReactRegion::Torso:
		Candidate =
			HitBoneName.ToString().StartsWith(TEXT("spine"))
				? HitBoneName
				: FName(TEXT("spine_01"));
		break;
	case EEnemyHitReactRegion::ArmRight:
		Candidate = FName(TEXT("upperarm_r"));
		break;
	case EEnemyHitReactRegion::ArmLeft:
		Candidate = FName(TEXT("upperarm_l"));
		break;
	case EEnemyHitReactRegion::LegRight:
		Candidate = FName(TEXT("thigh_r"));
		break;
	case EEnemyHitReactRegion::LegLeft:
		Candidate = FName(TEXT("thigh_l"));
		break;
	case EEnemyHitReactRegion::None:
	default:
		return NAME_None;
	}

	return IsValid(TargetMesh) &&
		TargetMesh->GetBoneIndex(Candidate) != INDEX_NONE
			? Candidate
			: NAME_None;
}

FName UEnemyPhysicalComponent::ResolveLimbBoneName(
	const EEnemyHitReactRegion Region) const
{
	switch (Region)
	{
	case EEnemyHitReactRegion::Head:
		return FName(TEXT("Head"));
	case EEnemyHitReactRegion::ArmRight:
		return FName(TEXT("upperarm_r"));
	case EEnemyHitReactRegion::ArmLeft:
		return FName(TEXT("upperarm_l"));
	case EEnemyHitReactRegion::LegRight:
		return FName(TEXT("thigh_r"));
	case EEnemyHitReactRegion::LegLeft:
		return FName(TEXT("thigh_l"));
	case EEnemyHitReactRegion::Torso:
	case EEnemyHitReactRegion::None:
	default:
		return NAME_None;
	}
}

bool UEnemyPhysicalComponent::ShouldSkipHitReactPresentation() const
{
	const AEnemyCharacter* OwnerCharacter = GetEnemyCharacter();
	const UEnemyMovementComponent* MovementComponent =
		IsValid(OwnerCharacter)
			? Cast<UEnemyMovementComponent>(
				OwnerCharacter->GetMovementComponent())
			: nullptr;
	return IsValid(MovementComponent) &&
		MovementComponent->IsTraversingNavLink();
}

void UEnemyPhysicalComponent::PlayHitReactPresentation(
	const EEnemyHitReactRegion Region,
	const FName PhysicsBoneName,
	const FVector& ImpulseDirection,
	const bool bAllowMontage)
{
	AEnemyCharacter* OwnerCharacter = GetEnemyCharacter();
	if (!IsValid(OwnerCharacter) || OwnerCharacter->IsDead() ||
		!IsValid(EnemyAsset) || !IsValid(TargetMesh))
	{
		return;
	}

	const FEnemyPhysicalReact* PhysicalReact =
		EnemyAsset->GetPhysicalReact();
	if (PhysicalReact == nullptr)
	{
		return;
	}

	if (bIsHit)
	{
		if (bAllowMontage &&
			!PhysicalReact->bRefreshLockOnRepeatedHit)
		{
			return;
		}

		// Repeated hits replace rather than stack the presentation. Crawling
		// always overrides the refresh option so a standing montage cannot
		// survive the locomotion transition on any client.
		StopHitReactPresentation(false);
	}

	bool bMontageStarted = false;
	float MontageExpectedDuration = 0.0f;
	UAnimMontage* HitMontage = bAllowMontage
		? ResolveHitReactMontage(Region, *PhysicalReact)
		: nullptr;
	if (IsValid(HitMontage))
	{
		if (UAnimInstance* AnimInstance = TargetMesh->GetAnimInstance())
		{
			AnimInstance->StopAllMontages(
				FMath::Max(
					0.0f,
					PhysicalReact->HitReactMontageBlendOutTime));
			const float MontageLength = OwnerCharacter->PlayAnimMontage(
				HitMontage,
				FMath::Max(0.01f, PhysicalReact->HitReactPlayRate));
			bMontageStarted = MontageLength > 0.0f;
			if (bMontageStarted)
			{
				ActiveHitReactMontage = HitMontage;
				const float EffectivePlayRate = FMath::Abs(
					AnimInstance->Montage_GetPlayRate(HitMontage) *
					HitMontage->RateScale);
				MontageExpectedDuration = MontageLength /
					FMath::Max(0.01f, EffectivePlayRate);
				FOnMontageEnded MontageEndedDelegate;
				MontageEndedDelegate.BindUObject(
					this,
					&UEnemyPhysicalComponent::HandleHitReactMontageEnded);
				AnimInstance->Montage_SetEndDelegate(
					MontageEndedDelegate,
					HitMontage);
			}
		}
	}
	else if (bAllowMontage)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("%s::%s: Hit montage is missing for region %s. Asset=%s"),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__),
			*UEnum::GetValueAsString(Region),
			*GetNameSafe(EnemyAsset));
	}
	else
	{
		UE_LOG(
			LogTemp,
			VeryVerbose,
			TEXT("%s::%s: Crawling enemy uses physics-only hit react. Actor=%s"),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__),
			*GetNameSafe(OwnerCharacter));
	}

	const bool bCanStartPhysics =
		PhysicsBoneName != NAME_None &&
		IsValid(PhysicalReact->ReactCurveFloat);
	if (bCanStartPhysics)
	{
		CacheBoneName = PhysicsBoneName;
		const float MaxBlendWeight = FMath::Clamp(
			PhysicalReact->BlendWeight_Anim_Physics,
			0.0f,
			1.0f);
		TargetMesh->SetAllBodiesBelowSimulatePhysics(
			PhysicsBoneName,
			true,
			true);
		TargetMesh->SetAllBodiesBelowPhysicsBlendWeight(
			PhysicsBoneName,
			MaxBlendWeight);

		if (!ImpulseDirection.IsNearlyZero() &&
			!FMath::IsNearlyZero(PhysicalReact->ReactScale))
		{
			TargetMesh->AddImpulse(
				ImpulseDirection.GetSafeNormal() *
				PhysicalReact->ReactScale,
				PhysicsBoneName,
				true);
		}
		ReactTimeline.PlayFromStart();
	}

	bIsHit = bMontageStarted || bCanStartPhysics;
	ActiveHitReactRegion = bIsHit
		? Region
		: EEnemyHitReactRegion::None;

	if (bIsHit)
	{
		if (UWorld* World = GetWorld())
		{
			// Montage-ended is the normal walking completion signal; the physics
			// timeline owns physics-only crawling completion. This timer is only
			// a watchdog for a missing completion callback.
			const float FallbackDuration = bMontageStarted
				? MontageExpectedDuration + 1.0f
				: FMath::Max(
					PhysicalReact->HitReactMovementLockDuration,
					ReactTimeline.GetTimelineLength() + 1.0f);
			World->GetTimerManager().SetTimer(
				HitReactFallbackTimerHandle,
				this,
				&UEnemyPhysicalComponent::HandleHitReactFallbackFinished,
				FallbackDuration,
				false);
		}
	}
}

void UEnemyPhysicalComponent::StopHitReactPresentation(
	const bool bPreservePhysics)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(
			HitReactFallbackTimerHandle);
	}
	HitReactFallbackTimerHandle.Invalidate();
	ReactTimeline.Stop();

	if (IsValid(TargetMesh))
	{
		if (IsValid(ActiveHitReactMontage))
		{
			if (UAnimInstance* AnimInstance =
				TargetMesh->GetAnimInstance())
			{
				// External cleanup (death, pooling, replacement hit) must not let
				// the old montage callback release the new state/presentation.
				FOnMontageEnded EmptyMontageEndedDelegate;
				AnimInstance->Montage_SetEndDelegate(
					EmptyMontageEndedDelegate,
					ActiveHitReactMontage);
				AnimInstance->Montage_Stop(
					0.0f,
					ActiveHitReactMontage);
			}
		}

		if (!bPreservePhysics)
		{
			TargetMesh->SetAllBodiesPhysicsBlendWeight(0.0f, false);
			TargetMesh->SetAllBodiesSimulatePhysics(false);
		}
	}

	bIsHit = false;
	CacheBoneName = NAME_None;
	ActiveHitReactRegion = EEnemyHitReactRegion::None;
	ActiveHitReactMontage = nullptr;
}

void UEnemyPhysicalComponent::ReleaseHitReactActionLock()
{
	AEnemyCharacter* OwnerCharacter = GetEnemyCharacter();
	if (!bOwnsHitReactActionLock ||
		!IsValid(OwnerCharacter) ||
		!OwnerCharacter->HasAuthority())
	{
		return;
	}
	bOwnsHitReactActionLock = false;

	if (UEnemyStatusComponent* StatusComponent =
		OwnerCharacter->GetEnemyStatusComponent())
	{
		// Expected-state clearing prevents a late montage callback from
		// clearing a higher-priority Knockdown or Dead state.
		StatusComponent->ClearActionState(EEnemyActionState::Stunned);
	}
}

void UEnemyPhysicalComponent::HandleHitReactMontageEnded(
	UAnimMontage* Montage,
	const bool /*bInterrupted*/)
{
	if (!IsValid(Montage) || Montage != ActiveHitReactMontage)
	{
		return;
	}

	// Clear the tracked montage first so presentation cleanup cannot trigger
	// this completion path recursively.
	ActiveHitReactMontage = nullptr;
	ReleaseHitReactActionLock();
	StopHitReactPresentation(false);
}

void UEnemyPhysicalComponent::HandleHitReactFallbackFinished()
{
	HitReactFallbackTimerHandle.Invalidate();
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("%s::%s: Hit-react completion watchdog fired. Actor=%s Montage=%s"),
		*GetClass()->GetName(),
		TEXT(__FUNCTION__),
		*GetNameSafe(GetEnemyCharacter()),
		*GetNameSafe(ActiveHitReactMontage));

	ReleaseHitReactActionLock();
	StopHitReactPresentation(false);
}

void UEnemyPhysicalComponent::BloodVFX(const FHitResult& HitResult)
{
	if (!IsValid(EnemyAsset))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s::%s: EnemyAsset is invalid."),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__)
		);
		return;
	}

	const FEnemyPhysicalReact* PhysicalReact = EnemyAsset->GetPhysicalReact();
	if (PhysicalReact == nullptr)
	{
		return;
	}

	UNiagaraSystem* BulletHit                  = PhysicalReact->Blood_BulletHit;
	UNiagaraSystem* BloodSplatter              = PhysicalReact->Blood_Splatter;
	UNiagaraSystem* BloodSplatterDirection     = PhysicalReact->Blood_Splatter_Direction;

	const FVector SpawnLocation =
		HitResult.ImpactPoint + HitResult.ImpactNormal * 2.0f;

	const FVector SplatterPosition =
		SpawnLocation + FVector(10.010032f, 21.110564f, 15.012697f);

	const FVector DirectionSplatterPosition =
		SpawnLocation + FVector(4.536247f, -42.886109f, 15.012695f);

	auto SpawnConfiguredNiagara =
		[this](
			UNiagaraSystem* System,
			const FVector& Location,
			const FRotator& Rotation,
			const int32 SpawnCount,
			const float LifetimeMultiplier,
			const float ScaleMultiplier
		) -> UNiagaraComponent*
	{
		if (!IsValid(System))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("%s::%s: Niagara System is invalid."),
				*GetClass()->GetName(),
				TEXT(__FUNCTION__)
			);
			return nullptr;
		}

		UNiagaraComponent* Component =
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				GetWorld(),
				System,
				Location,
				Rotation,
				FVector::OneVector,

				/* bAutoDestroy   */ true,
				/* bAutoActivate  */ false,
				/* PoolingMethod  */ ENCPoolMethod::None,
				/* bPreCullCheck  */ false
			);

		if (!IsValid(Component))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("%s::%s: Failed to spawn Niagara component: %s"),
				*GetClass()->GetName(),
				TEXT(__FUNCTION__),
				*GetNameSafe(System)
			);
			return nullptr;
		}

		/*
		 * Niagara System에서 아래 값들이
		 * User Parameter로 노출되어 있어야 합니다.
		 *
		 * User.SpawnCount          : int32
		 * User.LifetimeMultiplier  : float
		 * User.ScaleMultiplier     : float
		 */
		Component->SetVariableInt(
			TEXT("User.SpawnCount"),
			SpawnCount
		);

		Component->SetVariableFloat(
			TEXT("User.LifetimeMultiplier"),
			LifetimeMultiplier
		);

		Component->SetVariableFloat(
			TEXT("User.ScaleMultiplier"),
			ScaleMultiplier
		);

		Component->Activate(true);

		return Component;
	};

	SpawnConfiguredNiagara(
		BulletHit,
		SpawnLocation,
		FRotator(0.0f, 90.0f, 0.0f),
		8,
		1.0f,
		2.0f
	);

	SpawnConfiguredNiagara(
		BloodSplatter,
		SplatterPosition,
		FRotator(0.0f, 90.0f, 0.0f),
		4,
		1.0f,
		1.0f
	);

	SpawnConfiguredNiagara(
		BloodSplatterDirection,
		DirectionSplatterPosition,
		FRotator(0.0f, -90.0f, 0.0f),
		2,
		1.0f,
		1.0f
	);
}

void UEnemyPhysicalComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UEnemyPhysicalComponent, DestroyedLimbs);
}

UStaticMesh* UEnemyPhysicalComponent::GetLimbMesh(const FName BoneName) const
{
	if (!IsValid(EnemyAsset))
	{
		return nullptr;
	}

	const FEnemyLimbMesh* LimbMeshes = EnemyAsset->GetLimbMeshes();
	if (LimbMeshes == nullptr)
	{
		return nullptr;
	}

	if (BoneName == FName(TEXT("upperarm_r"))) return LimbMeshes->SM_Arm_R;
	if (BoneName == FName(TEXT("upperarm_l"))) return LimbMeshes->SM_Arm_L;
	if (BoneName == FName(TEXT("thigh_r")))    return LimbMeshes->SM_Leg_R;
	if (BoneName == FName(TEXT("thigh_l")))    return LimbMeshes->SM_Leg_L;

	// 머리는 떨어지는 조각이 없다.
	return nullptr;
}

void UEnemyPhysicalComponent::ApplyLimbDestruction(const FName BoneName)
{
	const ELocomotionWalkRunState NewLocomotionState =
		EvaluateLocomotionState();
	if (IsCrawlingLocomotionState(NewLocomotionState) &&
		(bIsHit || IsValid(ActiveHitReactMontage)))
	{
		// Losing a leg switches the locomotion graph to crawling. A standing
		// hit montage must not keep controlling that pose, and its old end
		// callback must not outlive the transition.
		StopHitReactPresentation(false);
		ReleaseHitReactActionLock();
	}

	if (UStaticMesh* MeshAsset = GetLimbMesh(BoneName))
	{
		UWorld* World = GetWorld();
		if (IsValid(World) && IsValid(ProxyMesh))
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = GetOwner();
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			const FTransform SpawnTransform = ProxyMesh->GetSocketTransform(BoneName);
			AStaticMeshActor* MeshPart = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), SpawnTransform, SpawnParams);
			if (IsValid(MeshPart))
			{
				MeshPart->SetLifeSpan(10.f);
				UStaticMeshComponent* MeshComp =
					Cast<UStaticMeshComponent>(MeshPart->GetRootComponent());
				if (IsValid(MeshComp))
				{
					MeshComp->SetMobility(EComponentMobility::Movable);
					MeshComp->SetStaticMesh(MeshAsset);
					MeshComp->SetCollisionProfileName(TEXT("PhysicsActor"));
					MeshComp->SetGenerateOverlapEvents(false);
					MeshComp->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
					MeshComp->SetCanEverAffectNavigation(false);
					MeshComp->SetMassOverrideInKg(NAME_None, 300.f);
					MeshComp->SetSimulatePhysics(true);
					MeshComp->WakeAllRigidBodies();
				}
			}
		}
	}

	if (IsValid(TargetMesh))
	{
		UAnimInstance* AnimInstance = TargetMesh->GetAnimInstance();
		if (IsValid(AnimInstance) && AnimInstance->IsAnyMontagePlaying())
		{
			AnimInstance->StopAllMontages(0.f);
		}
	}

	if (IsValid(ProxyMesh))
	{
		ProxyMesh->HideBoneByName(BoneName, PBO_Term);
	}

	SynchronizeLocomotionState();
}

void UEnemyPhysicalComponent::OnRep_DestroyedLimbs()
{
	if (Limbes.Num() == 0)
	{
		InitializeLimbStateFromSettings();
	}

	for (TPair<FName, FLimbData>& Pair : Limbes)
	{
		const bool bShouldBeDestroyed =
			DestroyedLimbs.Contains(Pair.Key);
		if (bShouldBeDestroyed)
		{
			if (Pair.Value.bIsHas)
			{
				Pair.Value.bIsHas = false;
				Pair.Value.Durability = 0.0f;
				ApplyLimbDestruction(Pair.Key);
			}
			continue;
		}

		Pair.Value.bIsHas = true;
		Pair.Value.Durability = Pair.Value.MaxDurability;
		if (IsValid(ProxyMesh))
		{
			ProxyMesh->UnHideBoneByName(Pair.Key);
		}
	}

	for (const FName& BoneName : DestroyedLimbs)
	{
		if (!Limbes.Contains(BoneName))
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("%s::%s: Unknown replicated limb bone '%s'."),
				*GetClass()->GetName(),
				TEXT(__FUNCTION__),
				*BoneName.ToString());
		}
	}

	// An empty replicated array is also meaningful: it restores a pooled
	// enemy to walking on clients.
	SynchronizeLocomotionState();
}

void UEnemyPhysicalComponent::ActionLimb(const FName BoneName, const float DamageAmount)
{
	if (BoneName == NAME_None)
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s: BoneName is Name_None."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}

	FLimbData* Data = Limbes.Find(BoneName);
	if (Data == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s: Limb data was not found for bone %s."), *GetClass()->GetName(), TEXT(__FUNCTION__), *BoneName.ToString());
		return;
	}

	if (Data->bIsHas)
	{
		Data->Durability -= DamageAmount;
		Data->Durability = FMath::Clamp(Data->Durability, 0.f, Data->MaxDurability);

		if (Data->Durability <= 0.f)
		{
			Data->bIsHas = false;

			// 복제 트리거. 클라는 OnRep_DestroyedLimbs에서 같은 연출을 재생한다.
			DestroyedLimbs.AddUnique(BoneName);
			if (AActor* Owner = GetOwner())
			{
				Owner->ForceNetUpdate();
			}

			ApplyLimbDestruction(BoneName);
		}
	}

	if (!IsLimbPresent(HeadLimbName))
	{
		Action_Dead();
	}
}

ELocomotionWalkRunState UEnemyPhysicalComponent::EvaluateLocomotionState() const
{
	const int32 MissingArmCount =
		static_cast<int32>(!IsLimbPresent(RightArmLimbName)) +
		static_cast<int32>(!IsLimbPresent(LeftArmLimbName));
	const bool bMissingLeg =
		!IsLimbPresent(RightLegLimbName) ||
		!IsLimbPresent(LeftLegLimbName);

	if (bMissingLeg)
	{
		if (MissingArmCount == 2)
		{
			return ELocomotionWalkRunState::Dead;
		}
		if (MissingArmCount == 1)
		{
			return ELocomotionWalkRunState::SlowCrawling;
		}

		return ELocomotionWalkRunState::Crawling;
	}

	return ELocomotionWalkRunState::Walking;
}

EEnemyMissingArmState UEnemyPhysicalComponent::GetMissingArmState() const
{
	const bool bRightArmMissing =
		!IsLimbPresent(RightArmLimbName);
	const bool bLeftArmMissing =
		!IsLimbPresent(LeftArmLimbName);

	if (bLeftArmMissing && bRightArmMissing)
	{
		return EEnemyMissingArmState::Both;
	}

	if (bLeftArmMissing)
	{
		return EEnemyMissingArmState::Left;
	}

	if (bRightArmMissing)
	{
		return EEnemyMissingArmState::Right;
	}

	return EEnemyMissingArmState::None;
}

void UEnemyPhysicalComponent::Action_Dead()
{
	Health = 0.0f;
	StopHitReactPresentation(true);
	bOwnsHitReactActionLock = false;
	if (AEnemyCharacter* OwnerCharacter = GetEnemyCharacter())
	{
		OwnerCharacter->Dead();
	}
}

void UEnemyPhysicalComponent::DrawDebugLimb()
{
	if (!bIsDrawDebug)
	{
		return;
	}

	UWorld* World = GetWorld();
	AEnemyCharacter* Character = GetEnemyCharacter();
	USkeletalMeshComponent* MeshComp = IsValid(Character)
		? Character->GetChildActorSkeletalMesh()
		: nullptr;
	if (!IsValid(World) || !IsValid(Character) || !IsValid(MeshComp))
	{
		return;
	}

	DrawDebugString(
		World,
		MeshComp->GetSocketLocation(FName(TEXT("spine_02"))),
		FString::Printf(
			TEXT("Health : %.1f / %.1f"),
			Health,
			MaxHealth),
		nullptr,
		FColor::Yellow,
		0.0f);

	const auto DrawLimb =
		[this, World, MeshComp](
			const FName LimbName,
			const TCHAR* Label)
		{
			const FLimbData* LimbData = Limbes.Find(LimbName);
			if (LimbData == nullptr || !LimbData->bIsHas)
			{
				return;
			}

			DrawDebugString(
				World,
				MeshComp->GetSocketLocation(LimbName),
				FString::Printf(
					TEXT("%s : %.1f / %.1f"),
					Label,
					LimbData->Durability,
					LimbData->MaxDurability),
				nullptr,
				FColor::White,
				0.0f);
		};

	DrawLimb(HeadLimbName, TEXT("Head"));
	DrawLimb(RightArmLimbName, TEXT("Arm_R"));
	DrawLimb(LeftArmLimbName, TEXT("Arm_L"));
	DrawLimb(RightLegLimbName, TEXT("Leg_R"));
	DrawLimb(LeftLegLimbName, TEXT("Leg_L"));
}

void UEnemyPhysicalComponent::HandleReactTimeline(float value)
{
	if (CacheBoneName == NAME_None || !IsValid(TargetMesh) ||
		!IsValid(EnemyAsset))
	{
		return;
	}

	const FEnemyPhysicalReact* PhysicalReact =
		EnemyAsset->GetPhysicalReact();
	if (PhysicalReact == nullptr)
	{
		return;
	}

	const float FinalBlendWeight =
		FMath::Clamp(value, 0.0f, 1.0f) *
		FMath::Clamp(
			PhysicalReact->BlendWeight_Anim_Physics,
			0.0f,
			1.0f);
	TargetMesh->SetAllBodiesBelowPhysicsBlendWeight(
		CacheBoneName,
		FinalBlendWeight);
}

void UEnemyPhysicalComponent::HandleReactTimelineFinished()
{
	AEnemyCharacter* OwnerCharacter = GetEnemyCharacter();
	if (IsValid(TargetMesh) &&
		(!IsValid(OwnerCharacter) || !OwnerCharacter->IsDead()))
	{
		TargetMesh->SetAllBodiesPhysicsBlendWeight(0.0f, false);
		TargetMesh->SetAllBodiesSimulatePhysics(false);
	}
	CacheBoneName = NAME_None;

	// Physics can finish before the montage. In that case the montage-ended
	// callback remains the sole owner of the Stunned -> Active transition.
	if (!IsValid(ActiveHitReactMontage))
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(
				HitReactFallbackTimerHandle);
		}
		HitReactFallbackTimerHandle.Invalidate();
		bIsHit = false;
		ActiveHitReactRegion = EEnemyHitReactRegion::None;
		ReleaseHitReactActionLock();
	}
}
