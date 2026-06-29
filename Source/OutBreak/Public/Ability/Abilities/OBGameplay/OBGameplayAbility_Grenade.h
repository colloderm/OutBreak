// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OBGameplayAbility_Consumable.h"
#include "OBGameplayAbility_Grenade.generated.h"

class AOBGrenadeProjectile;

UCLASS()
class OUTBREAK_API UOBGameplayAbility_Grenade : public UOBGameplayAbility_Consumable
{
	GENERATED_BODY()
	
public:
	// 던지기 몽타주 끝에서 투사체 스폰
	virtual void ApplyConsumableEffect() override;
	
protected:
	UPROPERTY(EditDefaultsOnly, Category = "Grenade")
	TSubclassOf<AOBGrenadeProjectile> GrenadeClass;

	// 폭발 데미지 GE(무기 GE_Damage 재사용 가능).
	UPROPERTY(EditDefaultsOnly, Category = "Grenade")
	TSubclassOf<UGameplayEffect> DamageEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Grenade", Meta = (ClampMin = "0.0"))
	float Damage = 80.f;

	UPROPERTY(EditDefaultsOnly, Category = "Grenade", Meta = (ClampMin = "0.0"))
	float ThrowSpeed = 1500.f;

	// 던지는 시작 소켓(오른손).
	UPROPERTY(EditDefaultsOnly, Category = "Grenade")
	FName ThrowSocketName = TEXT("hand_r_Socket");
};
