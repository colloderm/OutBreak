// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OBGameplayAbility.h"
#include "OBGameplayAbility_ADS.generated.h"



UCLASS()
class OUTBREAK_API UOBGameplayAbility_ADS : public UOBGameplayAbility
{
	GENERATED_BODY()
	
public:
	UOBGameplayAbility_ADS(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

};
