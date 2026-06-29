// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "OBAnimInstance.generated.h"

class APawn;
class AOBCharacterBase;
class UCharacterMovementComponent;

UCLASS()
class OUTBREAK_API UOBAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
protected:
	//~ UAnimInstance interface
	// 소유 폰/컴포넌트 캐싱(게임 스레드).
	virtual void NativeInitializeAnimation() override;
	// 매 프레임 상태 계산(워커 스레드, 안전한 읽기만).
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;
	// 게임 스레드: 무기 소켓 읽기(크로스 컴포넌트 안전).
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	//~ End interface

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion", Meta = (AllowPrivateAccess = "true"))
	float GroundSpeed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion", Meta = (AllowPrivateAccess = "true"))
	float Direction = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion", Meta = (AllowPrivateAccess = "true"))
	bool bIsInAir = false;

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion", Meta = (AllowPrivateAccess = "true"))
	bool bIsAccelerating = false;

	UPROPERTY(BlueprintReadOnly, Category = "Aim", Meta = (AllowPrivateAccess = "true"))
	float AimPitch = 0.0f;
	
	UPROPERTY(BlueprintReadOnly, Category = "Aim", Meta = (AllowPrivateAccess = "true"))
	float AimYaw = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "State", Meta = (AllowPrivateAccess = "true"))
	bool bIsDead = false;
	
	UPROPERTY(BlueprintReadOnly, Category = "State", Meta = (AllowPrivateAccess = "true"))
	bool bIsAiming = false;
	
private:
	// 방향 계산 헬퍼(외부 모듈 의존 회피).
	float CalculateDirectionAngle(const FVector& Velocity, const FRotator& BaseRotation) const;
	
	void UpdateLeftHandIK(float DeltaSeconds);
	
	bool ShouldDisableLeftHandIK() const; 
	
private:
	UPROPERTY()
	TObjectPtr<APawn> OwningPawn;

	UPROPERTY()
	TObjectPtr<AOBCharacterBase> OwningCharacter;

	UPROPERTY()
	TObjectPtr<UCharacterMovementComponent> MovementComponent;
	
	// 왼손이 따라갈 무기 그립의 위치(컴포넌트 스페이스).
	UPROPERTY(BlueprintReadOnly, Category = "IK", Meta = (AllowPrivateAccess = "true"))
	FTransform LeftHandIKTransform;

	// 무기 장착 시에만 IK 적용.
	UPROPERTY(BlueprintReadOnly, Category = "IK", Meta = (AllowPrivateAccess = "true"))
	bool bEnableLeftHandIK = false;
	
	// 왼손 IK 가중치(0~1). 장착/재장전 중 0으로 부드럽게 빠짐.
	UPROPERTY(BlueprintReadOnly, Category = "IK", Meta = (AllowPrivateAccess = "true"))
	float LeftHandIKAlpha = 0.f;

	// IK 알파 블렌드 속도.
	UPROPERTY(EditDefaultsOnly, Category = "IK")
	float LeftHandIKBlendSpeed = 12.f;

	UPROPERTY(EditDefaultsOnly, Category = "IK")
	FName WeaponLeftHandSocket = TEXT("LeftHandGrip");
	
	// 손바닥 그립 접점 소켓(hand_l에 있음).
	UPROPERTY(EditDefaultsOnly, Category = "IK")
	FName HandGripSocket = TEXT("HandGrip_L");
	
	UPROPERTY(EditDefaultsOnly, Category = "IK")
	bool bDebugLeftHandIK = false;
};
