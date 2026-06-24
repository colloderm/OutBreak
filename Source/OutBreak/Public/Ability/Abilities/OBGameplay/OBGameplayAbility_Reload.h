// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Ability/Abilities/OBGameplayAbility.h"
#include "OBGameplayAbility_Reload.generated.h"

class AOBWeaponBase;

UCLASS()
class OUTBREAK_API UOBGameplayAbility_Reload : public UOBGameplayAbility
{
	GENERATED_BODY()
	
public:
	UOBGameplayAbility_Reload(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	// 몽타주 완료 → 탄약 충전 후 종료.
	UFUNCTION()
	void OnReloadCompleted();
	// 몽타주 중단/취소 → 충전 없이 종료.
	UFUNCTION()
	void OnReloadCancelled();

	AOBWeaponBase* GetEquippedWeapon() const;
};
