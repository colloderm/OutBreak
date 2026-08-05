// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OBGameplayAbility_Consumable.h"
#include "OBGameplayAbility_Heal.generated.h"



UCLASS()
class OUTBREAK_API UOBGameplayAbility_Heal : public UOBGameplayAbility_Consumable
{
	GENERATED_BODY()
	
protected:
	// 풀피면 발동 자체를 막는다. 여기서 막아야 몽타주·소모가 아예 일어나지 않는다.
	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	
	virtual void ApplyConsumableEffect() override;
	
protected:
	// 회복 GE
	UPROPERTY(EditDefaultsOnly, Category = "Heal")
	TSubclassOf<UGameplayEffect> HealEffect;
	
	UPROPERTY(EditDefaultsOnly, Category = "Heal", meta = (ClampMin = 0.0))
	float HealAmount = 40.f;
};
