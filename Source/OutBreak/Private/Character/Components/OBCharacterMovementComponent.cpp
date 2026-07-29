// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Components/OBCharacterMovementComponent.h"


bool UOBCharacterMovementComponent::IsSprintGait() const
{
	// MaxWalkSpeed는 GASP BP가 gait 전환마다 직접 쓰는 값이라 게이트 판정에 그대로 쓸 수 있다.
	return MaxWalkSpeed >= SprintGaitSpeedThreshold;
}

float UOBCharacterMovementComponent::GetEffectiveSpeedMultiplier() const
{
	// 질주 중 조준은 화면 확대만 하고 속도는 유지한다.
	return IsSprintGait() ? MobilityMultiplier : (MobilityMultiplier * AimMultiplier);
}

float UOBCharacterMovementComponent::GetMaxSpeed() const
{
	// 이동모드별 기본 속도는 부모가 정하고, 우리는 배율만 얹는다.
	return Super::GetMaxSpeed() * GetEffectiveSpeedMultiplier();
}

void UOBCharacterMovementComponent::SetSpeedMultipliers(float InMobility, float InAim)
{
	// 0이 되면 영구 정지라 하한만 건다(상한은 향후 버프 여지로 남김).
	MobilityMultiplier = FMath::Max(InMobility, 0.05f);
	AimMultiplier      = FMath::Max(InAim, 0.05f);
}
