// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "OBPawnData.generated.h"

class AOBWeaponBase;

UCLASS(BlueprintType, Const)
class OUTBREAK_API UOBPawnData : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	// 로비 UI에 표시할 캐릭터 이름.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	TSubclassOf<AOBWeaponBase> DefaultWeapon;

	// [확장] 캐릭터별 외형 메시:
	// UPROPERTY(EditDefaultsOnly, Category = "Visual")
	// TObjectPtr<USkeletalMesh> CharacterMesh;

	// [확장] 캐릭터별 기본 능력 세트(AbilitySet이 Ability 지원 시):
	// UPROPERTY(EditDefaultsOnly, Category = "Ability")
	// TObjectPtr<UOBAbilitySet> DefaultAbilitySet;
};
