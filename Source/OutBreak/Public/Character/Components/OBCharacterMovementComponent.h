// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "OBCharacterMovementComponent.generated.h"


/**
 * 이동속도 배율을 GetMaxSpeed()에서 곱한다.
 * GASP BP가 gait 전환마다 MaxWalkSpeed를 덮어쓰므로 그 변수에 직접 쓰면 유지되지 않는다.
 * 여기서 곱하면 BP가 정한 gait 속도 위에 무기 기동성이 항상 얹힌다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class OUTBREAK_API UOBCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	virtual float GetMaxSpeed() const override;

	// 기동성과 조준 감속을 따로 저장한다. 스프린트에선 조준 감속만 빼야 하므로 합쳐 둘 수 없다.
	void SetSpeedMultipliers(float InMobility, float InAim);
	
	// 이번 프레임에 실제로 적용되는 배율(발사 차단 임계값 스케일용).
	float GetEffectiveSpeedMultiplier() const;

private:
	// GASP BP가 gait마다 쓰는 MaxWalkSpeed 원본으로 스프린트 여부를 읽는다.
	bool IsSprintGait() const;

private:
	UPROPERTY(VisibleInstanceOnly, Category = "Character Movement (General Settings)")
	float MobilityMultiplier = 1.f;

	UPROPERTY(VisibleInstanceOnly, Category = "Character Movement (General Settings)")
	float AimMultiplier = 1.f;

	// 이 값 이상이면 스프린트 게이트로 간주. GASP의 달리기(500)와 스프린트(700) 사이로 맞출 것.
	UPROPERTY(EditDefaultsOnly, Category = "Character Movement (General Settings)")
	float SprintGaitSpeedThreshold = 650.f;
};
