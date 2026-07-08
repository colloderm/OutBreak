// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "OBWeaponData.generated.h"

class UNiagaraSystem;
class USoundBase;
class UTexture2D;
class UCameraShakeBase;
class UOBAbilitySet;
class USkeletalMesh;
class UGameplayEffect;
class UAnimInstance;

UENUM(BlueprintType)
enum class EOBWeaponCategory : uint8
{
	// 주무기
	AssaultRifle UMETA(DisplayName = "Assault Rifle"),
	SniperRifle  UMETA(DisplayName = "Sniper Rifle"),
	SMG          UMETA(DisplayName = "SMG"),
	Shotgun      UMETA(DisplayName = "Shotgun"),
	// 보조무기
	Pistol		 UMETA(DisplayName = "Pistol"),
	// 근접무기
	Melee		 UMETA(DisplayName = "Melee"),
};

UENUM(BlueprintType)
enum class EOBWeaponFireMode : uint8
{
	Single		UMETA(DisplayName = "Single"),   // 단발
	Burst		UMETA(DisplayName = "Burst"),    // 점사
	FullAuto	UMETA(DisplayName = "Full Auto") // 연사
};

UENUM(BlueprintType)
enum class EOBWeaponType : uint8
{
	Ranged UMETA(DisplayName = "Ranged"),
	Melee  UMETA(DisplayName = "Melee")
};

UENUM(BlueprintType)
enum class EOBWeaponSlot : uint8
{
	Primary   UMETA(DisplayName = "Primary"),
	Secondary UMETA(DisplayName = "Secondary"),
	Melee     UMETA(DisplayName = "Melee")
};

UCLASS(BlueprintType, Const)
class OUTBREAK_API UOBWeaponData : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	// ===== 공통(항상 표시) =====

	// UI/줍기 등에 표시할 무기 이름.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText DisplayName;
	
	// 로비 무기 버튼 아이콘
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	TObjectPtr<UTexture2D> WeaponIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<USkeletalMesh> WeaponMesh;
	
	// 무기 타입
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	EOBWeaponType WeaponType = EOBWeaponType::Ranged;
	
	// 무기가 들어갈 로드아웃 슬롯(키 1/2/3).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	EOBWeaponSlot WeaponSlot = EOBWeaponSlot::Primary;
	
	// 무기 카테고리(슬롯/애님 레이어 분류).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	EOBWeaponCategory WeaponCategory = EOBWeaponCategory::AssaultRifle;

	// 1발당 기본 데미지(데미지 GE가 SetByCaller 등으로 참조 가능).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", Meta = (ClampMin = "0.0"))
	float BaseDamage = 20.0f;

	// 유효 사거리(cm). 히트스캔 트레이스 최대 거리.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", Meta = (ClampMin = "0.0"))
	float Range = 10000.0f;

	// --- GAS 연동 ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|GAS")
	TSubclassOf<UGameplayEffect> DamageEffect;
	
	// 무기를 장착하면 부여할 능력
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|GAS")
	TObjectPtr<UOBAbilitySet> AbilitySet;
	
	// 원거리=발사 몽타주 / 근접=스윙 몽타주.	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;
	
	// 무기 꺼내기(draw) 몽타주. 슬롯 보유 메시 스켈레톤 기준.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Animation")
	TObjectPtr<UAnimMontage> EquipMontage;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Animation",
		Meta = (EditCondition = "WeaponType == EOBWeaponType::Ranged", EditConditionHides))
	TObjectPtr<UAnimMontage> ReloadMontage;
	
	// 무기 장착 시 링크할 카테고리 포즈 레이어(Linked Anim Layer).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Animation")
	TSubclassOf<UAnimInstance> EquippedAnimLayer;
	
	
	// ===== 원거리 전용 (WeaponType == Ranged 일 때만 표시) =====
	
	// 탄약 타입
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo", Meta = (Categories = "Ammo",
		EditCondition = "WeaponType == EOBWeaponType::Ranged", EditConditionHides))
	FGameplayTag AmmoType;
	
	// 탄창 1개 용량.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo", Meta = (ClampMin = "1",
		EditCondition = "WeaponType == EOBWeaponType::Ranged", EditConditionHides))
	int32 MagazineSize = 30;

	// 예비 탄약 최대치.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo", Meta = (ClampMin = "0",
		EditCondition = "WeaponType == EOBWeaponType::Ranged", EditConditionHides))
	int32 MaxReserveAmmo = 120;
	
	// 분당 발사 수(RPM). 연사 간격 = 60 / RPM.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", Meta = (ClampMin = "1.0",
		EditCondition = "WeaponType == EOBWeaponType::Ranged", EditConditionHides))
	float RoundsPerMinute = 600.0f;

	// 발사 방식(단발/점사/연사).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat",
		Meta = (EditCondition = "WeaponType == EOBWeaponType::Ranged", EditConditionHides))
	EOBWeaponFireMode FireMode = EOBWeaponFireMode::Single;
	
	// 점사(Burst) 모드에서 한 번 누름당 발사 수.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", Meta = (ClampMin = "1",
		EditCondition = "WeaponType == EOBWeaponType::Ranged", EditConditionHides))
	int32 BurstCount = 3;
	
	// 발사음(무기별).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Audio",
		Meta = (EditCondition = "WeaponType == EOBWeaponType::Ranged", EditConditionHides))
	TObjectPtr<USoundBase> FireSound;

	// --- 반동 3종 + 카메라쉐이크 ---
	
	// 발당 수직 반동(Pitch, deg).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Recoil",
		Meta = (EditCondition = "WeaponType == EOBWeaponType::Ranged", EditConditionHides))
	float VerticalRecoil = 0.6f;

	// 발당 수평 흔들림(Yaw, deg).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Recoil",
		Meta = (EditCondition = "WeaponType == EOBWeaponType::Ranged", EditConditionHides))
	float HorizontalRecoil = 0.3f;

	// 사격 멈춘 뒤 시야 복귀 속도.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Recoil",
		Meta = (EditCondition = "WeaponType == EOBWeaponType::Ranged", EditConditionHides))
	float RecoilRecoverySpeed = 8.0f;

	// 발사 카메라 쉐이크(로컬 전용).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Recoil",
		Meta = (EditCondition = "WeaponType == EOBWeaponType::Ranged", EditConditionHides))
	TSubclassOf<UCameraShakeBase> FireCameraShake;
	
	// ADS 4종
	
	// 조준 시 카메라 FOV(작을수록 줌인).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|ADS",
		Meta = (EditCondition = "WeaponType == EOBWeaponType::Ranged", EditConditionHides))
	float ADSFOV = 50.0f;
	
	// 조준 시 이동 속도 배율.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|ADS",
		Meta = (EditCondition = "WeaponType == EOBWeaponType::Ranged", EditConditionHides))
	float ADSSpeedMultiplier = 0.5f;
	
	// FOV/상태 블렌드 속도.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|ADS",
		Meta = (EditCondition = "WeaponType == EOBWeaponType::Ranged", EditConditionHides))
	float ADSBlendSpeed = 12.0f;
	
	// 조준 시 반동 배율(1=동일, 0.5=절반, 0=무반동).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|ADS",
		Meta = (EditCondition = "WeaponType == EOBWeaponType::Ranged", EditConditionHides))
	float ADSRecoilMultiplier = 0.5f;
	
	// --- 탄퍼짐 3종 ---
	
	// 기본 탄퍼짐(반각, 도). 0이면 정확 사격.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Spread", Meta = (ClampMin = "0.0",
		EditCondition = "WeaponType == EOBWeaponType::Ranged", EditConditionHides))
	float BaseSpreadDegrees = 1.5f;

	// 조준(ADS) 시 퍼짐 배율(작을수록 정밀).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Spread", Meta = (ClampMin = "0.0",
		EditCondition = "WeaponType == EOBWeaponType::Ranged", EditConditionHides))
	float ADSSpreadMultiplier = 0.3f;

	// 이동 중 퍼짐 배율(클수록 부정확).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Spread", Meta = (ClampMin = "0.0",
		EditCondition = "WeaponType == EOBWeaponType::Ranged", EditConditionHides))
	float MovingSpreadMultiplier = 1.8f;
	
	// 발사 시 화면 집중 펄스(연사는 작게).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Feel",
		Meta = (EditCondition = "WeaponType == EOBWeaponType::Melee", EditConditionHides))
	float FireFocusPulse = 0.12f;
	
	// ===== 근접 전용 (WeaponType == Melee 일 때만 표시) =====
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Melee",
		Meta = (EditCondition = "WeaponType == EOBWeaponType::Melee", EditConditionHides))
	TObjectPtr<USoundBase> SwingSound;
	
	// 근접 스윙 블레이드 트레일(무기별).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Melee",
		Meta = (EditCondition = "WeaponType == EOBWeaponType::Melee", EditConditionHides))
	TObjectPtr<UNiagaraSystem> SwingTrailVFX;

	// 트레일을 붙일 무기 메시 소켓(칼날 시작 등).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Melee",
		Meta = (EditCondition = "WeaponType == EOBWeaponType::Melee", EditConditionHides))
	FName TrailSocketName = TEXT("TrailSocket");
};
