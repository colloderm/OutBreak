// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "OBWeaponCatalog.generated.h"


class AOBWeaponBase;
// 로비에서 선택 가능한 무기 목록.
UCLASS(BlueprintType, Const)
class OUTBREAK_API UOBWeaponCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "catalog")
	TArray<TSubclassOf<AOBWeaponBase>> AvailableWeapons;
};
