// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OBWeaponBase.generated.h"

class USkeletalMeshComponent;
class UOBWeaponData;

UCLASS()
class OUTBREAK_API AOBWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	AOBWeaponBase();
	
	UFUNCTION(BlueprintPure, Category = "Weapon")
	FVector GetMuzzleLocation() const;

	// 무기 데이터 접근자(읽기 전용).
	UFUNCTION(BlueprintPure, Category = "Weapon")
	UOBWeaponData* GetWeaponData() const { return WeaponData; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UOBWeaponData> WeaponData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FName MuzzleSocketName = TEXT("Muzzle");
};
