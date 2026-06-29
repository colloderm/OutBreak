// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Animation/OBAnimInstance.h"

#include "AbilitySystemComponent.h"
#include "Character/OBCharacterBase.h"
#include "Equipment/Components/OBEquipmentComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Weapon/OBWeaponBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Ability/Tags/OBGameplayTags.h"
#include "Weapon/Data/OBWeaponData.h"

void UOBAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	
	OwningPawn = TryGetPawnOwner();
	if (OwningPawn)
	{
		OwningCharacter = Cast<AOBCharacterBase>(OwningPawn);
		MovementComponent = OwningPawn->FindComponentByClass<UCharacterMovementComponent>();
	}
}

void UOBAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);
	
	if (!OwningPawn) return;
	
	const FVector Velocity = OwningPawn->GetVelocity();
	GroundSpeed = Velocity.Size2D();

	Direction = CalculateDirectionAngle(Velocity, OwningPawn->GetActorRotation());

	if (MovementComponent)
	{
		bIsInAir = MovementComponent->IsFalling();
		bIsAccelerating = !MovementComponent->GetCurrentAcceleration().IsNearlyZero();
	}

	const FRotator BaseAimRotation = OwningPawn->GetBaseAimRotation();
	AimPitch = FRotator::NormalizeAxis(BaseAimRotation.Pitch);
	AimYaw = FRotator::NormalizeAxis(BaseAimRotation.Yaw);

	if (OwningCharacter)
	{
		bIsDead = OwningCharacter->IsDead();
		bIsAiming = OwningCharacter->IsAiming();
	}
}

void UOBAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	
	// 무기 소켓 읽기는 게임 스레드에서(스레드 안전 업데이트에서 하면 위험).
	UpdateLeftHandIK(DeltaSeconds);
}

float UOBAnimInstance::CalculateDirectionAngle(const FVector& Velocity, const FRotator& BaseRotation) const
{
	if (Velocity.IsNearlyZero()) return 0.0f;

	const FVector ForwardDir = BaseRotation.Vector();
	const FVector RightDir = FRotationMatrix(BaseRotation).GetScaledAxis(EAxis::Y);
	const FVector NormalizedVel = Velocity.GetSafeNormal2D();

	const float ForwardDot = FVector::DotProduct(ForwardDir, NormalizedVel);
	const float RightDot = FVector::DotProduct(RightDir, NormalizedVel);

	// 전방과의 각도(0~180), 우측이면 양수/좌측이면 음수.
	const float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(ForwardDot, -1.0f, 1.0f)));
	return (RightDot < 0.0f) ? -Angle : Angle;
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
			// W: 무기 그립(목표) 월드.
			const FTransform GripWorld = WeaponMesh->GetSocketTransform(WeaponLeftHandSocket, RTS_World);

			// O: HandGrip_L 소켓이 hand_l 본에 대해 갖는 고정 오프셋.
			const FTransform HandBoneWorld = CharacterMesh->GetSocketTransform(TEXT("hand_l"), RTS_World);
			const FTransform HandGripWorld = CharacterMesh->GetSocketTransform(HandGripSocket, RTS_World);
			const FTransform GripRelToHand = HandGripWorld.GetRelativeTransform(HandBoneWorld);

			const FTransform DesiredHandWorld = GripRelToHand.Inverse() * GripWorld;
			LeftHandIKTransform = DesiredHandWorld.GetRelativeTransform(CharacterMesh->GetComponentTransform());

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
	UOBWeaponData* Data = Weapon ? Weapon->GetWeaponData() : nullptr;
	if (!Data) return false;

	// 꺼내기(Equip)/재장전 몽타주가 재생 중이면 왼팔을 자유롭게(IK off).
	if (Data->EquipMontage  && Montage_IsPlaying(Data->EquipMontage))  return true;
	if (Data->ReloadMontage && Montage_IsPlaying(Data->ReloadMontage)) return true;
	if (UAbilitySystemComponent* ASC = OwningCharacter->GetAbilitySystemComponent())
	{
		if (ASC->HasMatchingGameplayTag(OBGameplayTags::State_UsingConsumable)) return true;
	}

	return false;
}
