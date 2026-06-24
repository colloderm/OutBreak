// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Ability/Abilities/OBGameplayAbility.h"
#include "Weapon/Data/OBWeaponData.h"
#include "OBGameplayAbility_RangedWeapon.generated.h"

class AOBWeaponBase;

/*
 * 왜 존재하는가?
 - 히트스캔 무기 발사 행위를 담당한다. 트레이스 → 명중 대상에 데미지 GE 적용.
무엇을 저장하는가?
 - 자체 상태는 없음(무기 데이터에서 수치를 읽는다). 디버그 옵션만 보유.
멀티플레이에서 어떤 역할을 하는가?
 - 서버 권위로 판정/데미지를 처리하고, 결과(Health)는 AttributeSet이 복제한다.
 */
UCLASS()
class OUTBREAK_API UOBGameplayAbility_RangedWeapon : public UOBGameplayAbility
{
	GENERATED_BODY()
public:
	UOBGameplayAbility_RangedWeapon(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	
	// 타이머/태스크 정리 후 종료.
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	// 단발 1회: 반동(클라) + 트레이스/데미지/큐/몽타주(서버).
	void FireOneShot();

	// 점사/연사 타이머 콜백.
	void FireLoop();

	// 연사: 입력 뗌 시 종료.
	UFUNCTION()
	void OnFireInputReleased(float TimeHeld);

	// 서버 트레이스
	void PerformServerWeaponTrace();
	AOBWeaponBase* GetEquippedWeapon() const;
	
	// 무기 데이터 조회 헬퍼.
	EOBWeaponFireMode GetFireMode() const;
	int32 GetBurstCount() const;
	float GetFireInterval() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OB|Debug")
	bool bDrawDebugTrace = false;
	
private:
	// 점사/연사 반복 타이머.
	FTimerHandle FireTimerHandle;
	// 이번 활성화에서 쏜 발 수.
	int32 ShotsFired = 0;
	// 이번 활성화의 발사 모드.
	EOBWeaponFireMode CurrentFireMode = EOBWeaponFireMode::Single;
};
