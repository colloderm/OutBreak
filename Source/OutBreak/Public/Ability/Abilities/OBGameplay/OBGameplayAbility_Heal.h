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
	virtual void ApplyConsumableEffect() override;
	
protected:
	// 회복 GE
	UPROPERTY(EditDefaultsOnly, Category = "Heal")
	TSubclassOf<UGameplayEffect> HealEffect;
	
	UPROPERTY(EditDefaultsOnly, Category = "Heal", meta = (ClampMin = 0.0))
	float HealAmount = 40.f;
};
