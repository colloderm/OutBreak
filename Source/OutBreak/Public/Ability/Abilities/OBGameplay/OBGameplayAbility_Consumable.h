// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Ability/Abilities/OBGameplayAbility.h"
#include "OBGameplayAbility_Consumable.generated.h"

class UOBInventoryComponent;

UCLASS(Abstract)
class OUTBREAK_API UOBGameplayAbility_Consumable : public UOBGameplayAbility
{
	GENERATED_BODY()
	
public:
	UOBGameplayAbility_Consumable(const FObjectInitializer& ObjectInitializer);
	
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	
protected:
	// 파생에서 실제 효과 구현(서버에서 호출)
	virtual void ApplyConsumableEffect() {}
	
	UOBInventoryComponent* GetInventory() const;
	
	UFUNCTION()
	void OnUseCompleted();
	
	UFUNCTION()
	void OnUseCancelled();
	
protected:
	// 소모할 아이템 태그
	UPROPERTY(EditDefaultsOnly, Category = "Consumable", meta = (Categories = "Item"))
	FGameplayTag ItemTag;
	
	UPROPERTY(EditDefaultsOnly, Category = "Consumable")
	TObjectPtr<UAnimMontage> UseMontage;
	
	UPROPERTY(EditDefaultsOnly, Category = "Consumable")
	float DefaultUseTime = 1.5f;
};
