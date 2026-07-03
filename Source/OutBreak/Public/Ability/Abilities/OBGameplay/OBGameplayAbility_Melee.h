// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Ability/Abilities/OBGameplayAbility.h"
#include "OBGameplayAbility_Melee.generated.h"

class AOBWeaponBase;
/**
 * 
 */
UCLASS()
class OUTBREAK_API UOBGameplayAbility_Melee : public UOBGameplayAbility
{
	GENERATED_BODY()
	
public:
	UOBGameplayAbility_Melee(const FObjectInitializer& ObjectInitializer);
	
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	
protected:
	UFUNCTION()
	void OnHitWindow(); // 서버: 타격 판정
	
	UFUNCTION()
	void OnAttackFinished(); // 몽타주 종료
	
	UFUNCTION()
	void OnAttackCancelled();
	
	void PerformMeleeTrace();
	AOBWeaponBase* GetEquippedWeapon() const;
	
protected:
	// 타격 스윕 반경
	UPROPERTY(EditDefaultsOnly, Category = "Melee")
	float MeleeRadius = 40.f;
	
	// 스윙 시작 후 타격 판정까지. 몽타주의 접촉 순간에 맞도록
	UPROPERTY(EditDefaultsOnly, Category = "Melee")
	float HitTime = 0.2f;
	
	// 몽타주 없을 때  기본 공격 시간
	UPROPERTY(EditDefaultsOnly, Category = "Melee")
	float DefaultAttackTime = 0.6f;
	
	// 시작 높이 오프셋(가슴 높이)
	UPROPERTY(EditDefaultsOnly, Category = "Melee")
	float TraceHeight = 40.f;
	
	// 전방 판정(내적 최소값). 0=정면 180°, 클수록 좁은 부채꼴. 뒤 타격 방지.
	UPROPERTY(EditDefaultsOnly, Category = "Melee") 
	float MinFacingDot = 0.f;
};
