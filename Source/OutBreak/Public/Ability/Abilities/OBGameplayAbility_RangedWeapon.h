// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Ability/Abilities/OBGameplayAbility.h"
#include "Weapon/Data/OBWeaponData.h"
#include "OBGameplayAbility_RangedWeapon.generated.h"

class AOBWeaponBase;
struct FGameplayAbilityTargetDataHandle;

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
	
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	// 단발 1회 요청: 로컬 예측 반동 + 서버 TargetData 승인 대기.
	bool FireOneShot();

	// 점사/연사 타이머 콜백.
	void FireLoop();

	// 연사: 입력 뗌 시 종료.
	UFUNCTION()
	void OnFireInputReleased(float TimeHeld);

	/** Captures the owning client's evaluated PlayerCameraManager view for one shot. */
	bool BuildLocalAimTargetData(FGameplayAbilityTargetDataHandle& OutTargetData) const;

	/** Sends predicted client aim through GAS' prediction-key target-data channel. */
	void SubmitLocalAimTargetData(const FGameplayAbilityTargetDataHandle& TargetData);

	/** Server callback for both local-host and replicated client shot views. */
	void HandleAimTargetData(const FGameplayAbilityTargetDataHandle& TargetData, FGameplayTag ActivationTag);

	/** Cancels a remote authoritative ability whose client never supplied aim data. */
	void HandleAimTargetDataTimeout();

	/**
	 * Validates the submitted view, then atomically commits ammo, fire presentation,
	 * muzzle-origin ballistics, and damage on the server.
	 */
	bool CommitServerShot(const FVector& ViewOrigin, const FVector& ViewDirection);
	AOBWeaponBase* GetEquippedWeapon() const;
	
	// 무기 데이터 조회 헬퍼.
	EOBWeaponFireMode GetFireMode() const;
	int32 GetBurstCount() const;
	float GetFireInterval() const;
	
	// 현재 상태(조준/이동) 기반 퍼짐 각도(도).
	float GetCurrentSpreadAngle() const;
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OB|Debug")
	bool bDrawDebugTrace = false;

	/** Maximum distance allowed between the server pawn and a client camera origin. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OB|Targeting|Validation", meta = (ClampMin = "100.0"))
	float MaxValidatedCameraDistance = 1200.f;

	/** Maximum angle between submitted camera direction and replicated control aim. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OB|Targeting|Validation", meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float MaxValidatedAimAngleDegrees = 30.f;

	/** Maximum time a remote server shot may wait for its matching client view. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OB|Targeting|Validation", meta = (ClampMin = "0.1"))
	float AimTargetDataTimeoutSeconds = 1.f;
	
private:
	// 소유 캐릭터가 스프린트(고속) 중인지. 발사 게이트 공용.
	bool IsOwnerSprinting() const;
	
private:
	// 점사/연사 반복 타이머.
	FTimerHandle FireTimerHandle;
	// 서버가 가장 오래 대기 중인 TargetData를 감시하는 타이머.
	FTimerHandle AimTargetDataTimeoutHandle;
	// 이번 활성화에서 쏜 발 수.
	int32 ShotsFired = 0;
	// 이번 활성화의 발사 모드.
	EOBWeaponFireMode CurrentFireMode = EOBWeaponFireMode::Single;

	/** Server-issued shots waiting for exactly one matching client view each. */
	int32 PendingServerAimShots = 0;
	bool bRemoteAimDelegateBound = false;
};
