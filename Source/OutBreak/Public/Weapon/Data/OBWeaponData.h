// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "OBWeaponData.generated.h"

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

	// 장착 시 사용할 무기 스켈레탈 메시.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<USkeletalMesh> WeaponMesh;

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
	EOBWeaponFireMode FireMode = EOBWeaponFireMode::FullAuto;

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
    	
};
