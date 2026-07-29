// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GameplayTagContainer.h"
#include "OBAnimInstance.generated.h"

class UAnimSequence;
class UBlendSpace;
class AOBCharacterBase;

UCLASS()
class OUTBREAK_API UOBAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
protected:
	//~ UAnimInstance interface
	virtual void NativeInitializeAnimation() override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	//~ End interface

	UPROPERTY(BlueprintReadOnly, Category = "State", Meta = (AllowPrivateAccess = "true"))
	bool bIsDead = false;
	
	UPROPERTY(BlueprintReadOnly, Category = "State", Meta = (AllowPrivateAccess = "true"))
	bool bIsDowned = false;
	
	UPROPERTY(BlueprintReadOnly, Category = "State", Meta = (AllowPrivateAccess = "true"))
	bool bIsAiming = false;
	
	// 현재 무기의 상체 오버레이 로코모션(없으면 null). AnimGraph의 BlendSpace Player 입력.
	UPROPERTY(BlueprintReadOnly, Category = "Weapon", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBlendSpace> CurrentOverlayLocomotion;

	// 오버레이 적용 여부 → Layered Blend 알파(1/0).
	UPROPERTY(BlueprintReadOnly, Category = "Weapon", Meta = (AllowPrivateAccess = "true"))
	bool bHasOverlay = false;
	
	// 오버레이 블렌드스페이스 입력: 지면 속도(cm/s).
	UPROPERTY(BlueprintReadOnly, Category = "Weapon", Meta = (AllowPrivateAccess = "true"))
	float GroundSpeed = 0.f;

	// 오버레이 블렌드스페이스 입력: 액터 정면 기준 진행 방향(-180~180).
	UPROPERTY(BlueprintReadOnly, Category = "Weapon", Meta = (AllowPrivateAccess = "true"))
	float LocomotionDirection = 0.f;
	
	// 조준 오프셋 입력(액터 정면 기준, -180~180 / -90~90).
	UPROPERTY(BlueprintReadOnly, Category = "Weapon", Meta = (AllowPrivateAccess = "true"))
	float AimYaw = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon", Meta = (AllowPrivateAccess = "true"))
	float AimPitch = 0.f;

	// 현재 무기의 조준 오프셋(없으면 null).
	UPROPERTY(BlueprintReadOnly, Category = "Weapon", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBlendSpace> CurrentAimOffset;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon", Meta = (AllowPrivateAccess = "true"))
	bool bHasWeaponAimOffset = false;

	// ADS 상체 포즈. bUseADSPose가 true일 때만 사용.
	UPROPERTY(BlueprintReadOnly, Category = "Weapon", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimSequence> CurrentADSPose;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon", Meta = (AllowPrivateAccess = "true"))
	bool bUseADSPose = false;
	
	// 전투 중(조준/발사 직후): Offset Root Bone 상쇄를 끄고 상체가 조준을 즉시 따르게 한다.
	UPROPERTY(BlueprintReadOnly, Category = "Weapon", Meta = (AllowPrivateAccess = "true"))
	bool bInCombat = false;
	
	// 스프린트 상체 포즈(없으면 null).
	UPROPERTY(BlueprintReadOnly, Category = "Weapon", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimSequence> CurrentSprintPose;

	// 오버레이 포즈 선택: 0=로코모션, 1=ADS, 2=스프린트.
	UPROPERTY(BlueprintReadOnly, Category = "Weapon", Meta = (AllowPrivateAccess = "true"))
	int32 OverlayPoseIndex = 0;

	// 오버레이 + 왼손 IK 공통 가중치.
	UPROPERTY(BlueprintReadOnly, Category = "Weapon", Meta = (AllowPrivateAccess = "true"))
	float OverlayAlpha = 0.f;
	
	// 조준 오프셋 가중치. 스프린트 중엔 0(상체가 스프린트 포즈에서 비틀리는 것 방지).
	UPROPERTY(BlueprintReadOnly, Category = "Weapon", Meta = (AllowPrivateAccess = "true"))
	float AimOffsetAlpha = 0.f;
	
	// GASP Gait에서 ABP가 매 프레임 설정. 속도 임계값보다 정확하다.
	UPROPERTY(BlueprintReadWrite, Category = "Overlay", Meta = (AllowPrivateAccess = "true"))
	bool bIsSprintingFromGait = false;
	
	// 스프린트 해제 지연(초). Gait가 경계에서 떨려도 오버레이가 깜빡이지 않게 한다.
	UPROPERTY(EditDefaultsOnly, Category = "Overlay")
	float SprintReleaseDelay = 0.15f;

	// 오버레이/조준오프셋 알파 블렌드 속도.
	UPROPERTY(EditDefaultsOnly, Category = "Overlay")
	float OverlayBlendSpeed = 10.f;
	
private:	
	void UpdateLeftHandIK(float DeltaSeconds);
	
	bool ShouldDisableLeftHandIK() const; 
	
private:
	// 스프린트 해제까지 남은 유예 시간.
	float SprintOffDelayRemaining = 0.f;
	
	UPROPERTY()
	TObjectPtr<AOBCharacterBase> OwningCharacter;
	
	//~ 왼손 무기 그립 IK ---------------------------------
	
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
