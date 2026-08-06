// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Animation/OBAnimInstance.h"

#include "AbilitySystemComponent.h"
#include "Character/OBCharacterBase.h"
#include "Equipment/Components/OBEquipmentComponent.h"
#include "Weapon/OBWeaponBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Ability/Tags/OBGameplayTags.h"
#include "Weapon/Data/OBWeaponData.h"
#include "Animation/BlendSpace.h"
#include "GameFramework/CharacterMovementComponent.h"

void UOBAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	
	OwningCharacter = Cast<AOBCharacterBase>(TryGetPawnOwner());
}

void UOBAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);
	
	if (!OwningCharacter) return;

	bIsDead = OwningCharacter->IsDead();
	bIsDowned = OwningCharacter->IsDowned();
	bIsAiming = OwningCharacter->IsAiming();
}

void UOBAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	
	CurrentOverlayLocomotion = nullptr;
	GroundSpeed = 0.f;
	LocomotionDirection = 0.f;
	CurrentAimOffset = nullptr;
	CurrentADSPose = nullptr;
	CurrentSprintPose = nullptr;
	bool bFalling = false;

	if (OwningCharacter)
	{
		if (UOBEquipmentComponent* Equip = OwningCharacter->FindComponentByClass<UOBEquipmentComponent>())
			if (AOBWeaponBase* W = Equip->GetCurrentWeapon())
				if (const FOBWeaponDefinitionRow* Definition = W->GetWeaponDefinition())
				{
					CurrentOverlayLocomotion = Definition->Visual.OverlayLocomotion.LoadSynchronous();
					CurrentAimOffset         = Definition->Visual.AimOffset.LoadSynchronous();
					CurrentADSPose           = Definition->Visual.ADSPose.LoadSynchronous();
					CurrentSprintPose        = Definition->Visual.SprintPose.LoadSynchronous();
				}

		// 블렌드스페이스 입력: 수직 속도는 제외(점프 중 상체가 질주 자세 되는 것 방지).
		const FVector Velocity = OwningCharacter->GetVelocity();
		const FVector GroundVelocity(Velocity.X, Velocity.Y, 0.f);
		GroundSpeed = GroundVelocity.Size();
		
		// 조준 각도. GetBaseAimRotation은 시뮬레이티드 프록시에도 복제되어 다른 플레이어의 상하 조준이 올바르게 보인다.
		const FRotator AimDelta = (OwningCharacter->GetBaseAimRotation() - OwningCharacter->GetActorRotation()).GetNormalized();
		AimYaw   = FMath::Clamp(AimDelta.Yaw, -50.f, 50.f);
		AimPitch = AimDelta.Pitch;
		
		if (GroundSpeed > 1.f)   // 정지 시 방향값 튐 방지
		{
			LocomotionDirection = (GroundVelocity.Rotation() - OwningCharacter->GetActorRotation()).GetNormalized().Yaw;
		}
		
		// 공중에서는 상체 오버레이를 정지 포즈로 고정(팔 좌우 흔들림 제거).
		if (const UCharacterMovementComponent* Move = OwningCharacter->GetCharacterMovement())
		{
			bFalling = Move->IsFalling();
			if (bFalling)
			{
				GroundSpeed = 0.f;
				LocomotionDirection = 0.f;
			}
		}
	}
	
	bHasOverlay = CurrentOverlayLocomotion != nullptr;
	bHasWeaponAimOffset = CurrentAimOffset != nullptr;
	bUseADSPose = CurrentADSPose && OwningCharacter && OwningCharacter->IsAiming();
	const bool bAiming = OwningCharacter && OwningCharacter->IsAiming();
	bInCombat = OwningCharacter && (OwningCharacter->IsAiming() || OwningCharacter->IsRecentlyFired());
	
	// 진입은 즉시, 해제만 지연. Gait가 Run↔Sprint를 오가도 상체가 한 방향으로만 반응한다.
	if (bIsSprintingFromGait && !bFalling)
	{
		SprintOffDelayRemaining = SprintReleaseDelay;
	}
	else
	{
		SprintOffDelayRemaining = FMath::Max(0.f, SprintOffDelayRemaining - DeltaSeconds);
	}
	const bool bSprinting = SprintOffDelayRemaining > 0.f;

	if (bUseADSPose)							OverlayPoseIndex = 1;
	else if (bSprinting && CurrentSprintPose)	OverlayPoseIndex = 2;
	else										OverlayPoseIndex = 0;

	// 스프린트인데 전용 포즈가 없으면(권총 등) 오버레이·IK 해제. 단 조준 중이면 유지 —
	const float TargetOverlayAlpha   = (bHasOverlay && (bAiming || !bSprinting || CurrentSprintPose)) ? 1.f : 0.f;
	// 조준 중이면 스프린트여도 조준 오프셋을 켜서 상체가 시야를 따라가게 한다.
	const float TargetAimOffsetAlpha = (bHasWeaponAimOffset && (bAiming || !bSprinting)) ? 1.f : 0.f;

	// 0/1 스냅은 경계에서 팝으로 보인다. 왼손 IK와 같은 방식으로 보간.
	OverlayAlpha   = FMath::FInterpTo(OverlayAlpha,   TargetOverlayAlpha,   DeltaSeconds, OverlayBlendSpeed);
	AimOffsetAlpha = FMath::FInterpTo(AimOffsetAlpha, TargetAimOffsetAlpha, DeltaSeconds, OverlayBlendSpeed);

	// 무기 소켓 읽기는 게임 스레드에서(스레드 안전 업데이트에서 하면 위험).
	UpdateLeftHandIK(DeltaSeconds);
}

void UOBAnimInstance::UpdateLeftHandIK(float DeltaSeconds)
{
	float TargetAlpha = 0.f;   // 기본: IK 끔

	if (OwningCharacter)
	{
		UOBEquipmentComponent* Equipment = OwningCharacter->FindComponentByClass<UOBEquipmentComponent>();
		AOBWeaponBase* Weapon = Equipment ? Equipment->GetCurrentWeapon() : nullptr;

		USkeletalMeshComponent* WeaponMesh = Weapon ? Cast<USkeletalMeshComponent>(Weapon->GetRootComponent()) : nullptr;
		USkeletalMeshComponent* CharacterMesh = OwningCharacter->GetMesh();

		if (Weapon && WeaponMesh && CharacterMesh && WeaponMesh->DoesSocketExist(WeaponLeftHandSocket))
		{
			// 무기가 실제로 붙어 있는 메시. 인덱스 하드코딩 대신 부착 부모에서 역추적.
			USkeletalMeshComponent* HolderMesh = Cast<USkeletalMeshComponent>(WeaponMesh->GetAttachParent());
			if (!HolderMesh) HolderMesh = CharacterMesh;

			// W: 무기 그립 월드.
			const FTransform GripWorld = WeaponMesh->GetSocketTransform(WeaponLeftHandSocket, RTS_World);

			// O: HandGrip_L 소켓이 hand_l 본에 대해 갖는 고정 오프셋.
			const FTransform HandBoneWorld = CharacterMesh->GetSocketTransform(TEXT("hand_l"), RTS_World);
			const FTransform HandGripWorld = CharacterMesh->GetSocketTransform(HandGripSocket, RTS_World);
			const FTransform GripRelToHand = HandGripWorld.GetRelativeTransform(HandBoneWorld);

			// 무기 그립을 hand_r(무기 부착 본) 기준 상대값으로 환산.
			// 무기와 hand_r은 강체로 붙어 있어, 둘 다 1프레임 전 값이어도 상대 오프셋의 오차는 0.
			const FTransform HandRWorld   = HolderMesh->GetSocketTransform(TEXT("hand_r"), RTS_World);
			const FTransform GripRelHandR = GripWorld.GetRelativeTransform(HandRWorld);

			// hand_l이 있어야 할 위치를 hand_r 본 스페이스로.
			LeftHandIKTransform = GripRelToHand.Inverse() * GripRelHandR;

			// 장착/해제/재장전 몽타주가 왼팔을 움직이는 동안엔 IK를 끈다.
			TargetAlpha = ShouldDisableLeftHandIK() ? 0.f : 1.f;

#if ENABLE_DRAW_DEBUG
			if (bDebugLeftHandIK && GetWorld())
			{
				DrawDebugSphere(GetWorld(), HandGripWorld.GetLocation(), 4.0f, 8, FColor::Cyan, false, -1.0f);
			}
#endif
		}
	}

	if (TargetAlpha < LeftHandIKAlpha)
	{
		LeftHandIKAlpha = TargetAlpha;
	}
	else
	{
		LeftHandIKAlpha = FMath::FInterpTo(LeftHandIKAlpha, TargetAlpha, DeltaSeconds, LeftHandIKBlendSpeed);
	}
	bEnableLeftHandIK = LeftHandIKAlpha > KINDA_SMALL_NUMBER;
}

bool UOBAnimInstance::ShouldDisableLeftHandIK() const
{
	if (!OwningCharacter) return false;

	UOBEquipmentComponent* Equipment = OwningCharacter->FindComponentByClass<UOBEquipmentComponent>();
	AOBWeaponBase* Weapon = Equipment ? Equipment->GetCurrentWeapon() : nullptr;
	const FOBWeaponDefinitionRow* Definition = Weapon ? Weapon->GetWeaponDefinition() : nullptr;
	if (!Definition) return false;
	UAnimMontage* EquipMontage = Definition->Visual.EquipMontage.LoadSynchronous();
	UAnimMontage* ReloadMontage = Definition->Visual.ReloadMontage.LoadSynchronous();

	// 꺼내기(Equip)/재장전 몽타주가 재생 중이면 왼팔을 자유롭게(IK off).
	if (EquipMontage && Montage_IsPlaying(EquipMontage)) return true;
	if (ReloadMontage && Montage_IsPlaying(ReloadMontage)) return true;
	if (UAbilitySystemComponent* ASC = OwningCharacter->GetAbilitySystemComponent())
	{
		if (ASC->HasMatchingGameplayTag(OBGameplayTags::State_UsingConsumable)) return true;
	}

	return false;
}
