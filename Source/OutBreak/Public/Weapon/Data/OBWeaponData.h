// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "OBWeaponData.generated.h"

class UCameraShakeBase;
class UOBAbilitySet;
class USkeletalMesh;
class UGameplayEffect;

UENUM(BlueprintType)
enum class EOBWeaponFireMode : uint8
{
	Single		UMETA(DisplayName = "Single"),   // 단발
	Burst		UMETA(DisplayName = "Burst"),    // 점사
	FullAuto	UMETA(DisplayName = "Full Auto") // 연사
};

UCLASS(BlueprintType, Const)
class OUTBREAK_API UOBWeaponData : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	// --- 표시/외형 ---

	// UI/줍기 등에 표시할 무기 이름.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<USkeletalMesh> WeaponMesh;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Animation")
	TObjectPtr<UAnimMontage> FireMontage;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Animation")
	TObjectPtr<UAnimMontage> ReloadMontage;

	// --- 발사 스탯 ---

	// 1발당 기본 데미지(데미지 GE가 SetByCaller 등으로 참조 가능).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", Meta = (ClampMin = "0.0"))
	float BaseDamage = 20.0f;

	// 분당 발사 수(RPM). 연사 간격 = 60 / RPM.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", Meta = (ClampMin = "1.0"))
	float RoundsPerMinute = 600.0f;

	// 유효 사거리(cm). 히트스캔 트레이스 최대 거리.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", Meta = (ClampMin = "0.0"))
	float Range = 10000.0f;

	// 발사 방식(단발/점사/연사).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	EOBWeaponFireMode FireMode = EOBWeaponFireMode::Single;
	
	// 점사(Burst) 모드에서 한 번 누름당 발사 수.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", Meta = (ClampMin = "1"))
	int32 BurstCount = 3;

	// --- 탄약(최대치 정의) ---
	
	// 탄창 1개 용량.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo", Meta = (ClampMin = "1"))
	int32 MagazineSize = 30;

	// 예비 탄약 최대치.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo", Meta = (ClampMin = "0"))
	int32 MaxReserveAmmo = 120;

	// --- GAS 연동 ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|GAS")
	TSubclassOf<UGameplayEffect> DamageEffect;
	
	// 무기를 장착하면 부여할 능력
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|GAS")
	TObjectPtr<UOBAbilitySet> AbilitySet;
	
	// 발당 수직 반동(Pitch, deg).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Recoil")
	float VerticalRecoil = 0.6f;

	// 발당 수평 흔들림(Yaw, deg).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Recoil")
	float HorizontalRecoil = 0.3f;

	// 사격 멈춘 뒤 시야 복귀 속도.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Recoil")
	float RecoilRecoverySpeed = 8.0f;
	
	// 조준 시 카메라 FOV(작을수록 줌인).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|ADS")
	float ADSFOV = 50.0f;
	
	// 조준 시 이동 속도 배율.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|ADS")
	float ADSSpeedMultiplier = 0.5f;
	
	// FOV/상태 블렌드 속도.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|ADS")
	float ADSBlendSpeed = 12.0f;
	
	// 조준 시 반동 배율(1=동일, 0.5=절반, 0=무반동).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|ADS")
	float ADSRecoilMultiplier = 0.5f;

	// 발사 카메라 쉐이크(로컬 전용).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Recoil")
	TSubclassOf<UCameraShakeBase> FireCameraShake;
	
	// 기본 탄퍼짐(반각, 도). 0이면 정확 사격.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Spread", Meta = (ClampMin = "0.0"))
	float BaseSpreadDegrees = 1.5f;

	// 조준(ADS) 시 퍼짐 배율(작을수록 정밀).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Spread", Meta = (ClampMin = "0.0"))
	float ADSSpreadMultiplier = 0.3f;

	// 이동 중 퍼짐 배율(클수록 부정확).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Spread", Meta = (ClampMin = "0.0"))
	float MovingSpreadMultiplier = 1.8f;
	
	// 발사 시 화면 집중 펄스(연사는 작게).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Feel")
	float FireFocusPulse = 0.12f;
};
