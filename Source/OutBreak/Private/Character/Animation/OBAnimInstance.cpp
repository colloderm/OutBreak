// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Animation/OBAnimInstance.h"

#include "Character/OBCharacterBase.h"
#include "Equipment/Components/OBEquipmentComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Weapon/OBWeaponBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"

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
	UpdateLeftHandIK();
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

void UOBAnimInstance::UpdateLeftHandIK()
{
	bEnableLeftHandIK = false;
	
	if (!OwningCharacter) return;
	
	// 현재 장착 무기 조회.
	UOBEquipmentComponent* Equipment = OwningCharacter->FindComponentByClass<UOBEquipmentComponent>();
	AOBWeaponBase* Weapon = Equipment ? Equipment->GetCurrentWeapon() : nullptr;
	if (!Weapon)
	{
		return;
	}

	USkeletalMeshComponent* WeaponMesh = Cast<USkeletalMeshComponent>(Weapon->GetRootComponent());
	USkeletalMeshComponent* CharacterMesh = OwningCharacter->GetMesh();
	if (!WeaponMesh || !CharacterMesh || !WeaponMesh->DoesSocketExist(WeaponLeftHandSocket))
	{
		return;
	}

	// W: 무기 그립(목표) 월드.
	const FTransform GripWorld = WeaponMesh->GetSocketTransform(WeaponLeftHandSocket, RTS_World);

	// O: HandGrip_L 소켓이 hand_l 본에 대해 갖는 고정 오프셋(손목→손바닥).
	const FTransform HandBoneWorld = CharacterMesh->GetSocketTransform(TEXT("hand_l"), RTS_World);
	const FTransform HandGripWorld = CharacterMesh->GetSocketTransform(HandGripSocket, RTS_World);
	const FTransform GripRelToHand = HandGripWorld.GetRelativeTransform(HandBoneWorld);

	// hand_l이 가야 할 위치 = 오프셋 역보정 → HandGrip_L이 무기 그립에 정확히 닿음.
	const FTransform DesiredHandWorld = GripRelToHand.Inverse() * GripWorld;

	LeftHandIKTransform = DesiredHandWorld.GetRelativeTransform(CharacterMesh->GetComponentTransform());
	
#if ENABLE_DRAW_DEBUG
	if (bDebugLeftHandIK && GetWorld())
	{
		// 왼손 IK 타깃(소켓) 위치를 시안색 구로 표시.
		DrawDebugSphere(GetWorld(), HandGripWorld.GetLocation(), 4.0f, 8, FColor::Cyan, false, -1.0f);
	}
#endif

	bEnableLeftHandIK = true;
}
