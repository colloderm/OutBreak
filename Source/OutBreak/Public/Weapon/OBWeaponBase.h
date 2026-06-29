// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OBWeaponBase.generated.h"

DECLARE_MULTICAST_DELEGATE(FOBOnAmmoChanged);

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
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Weapon|Ammo")
	int32 GetCurrentAmmo() const { return CurrentAmmo; }

	bool HasAmmo() const { return CurrentAmmo > 0; }
	bool CanReload() const;            // 탄창 안 참 && 예비탄 있음
	void ConsumeAmmo(int32 Amount = 1); // 서버
	void PerformReload();              // 서버: 예비탄→탄창
	void InitializeAmmo();             // 서버: WeaponData 기준 초기화
	
	void SetCurrentAmmo(int32 NewAmmo);

public:
	// UI 갱신용(탄약 변경 통지).
	FOBOnAmmoChanged OnAmmoChanged;

protected:
	virtual void BeginPlay() override;

	UFUNCTION() void OnRep_Ammo();
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UOBWeaponData> WeaponData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FName MuzzleSocketName = TEXT("Muzzle");
	
	// 현재 탄창 탄약.
	UPROPERTY(ReplicatedUsing = OnRep_Ammo)
	int32 CurrentAmmo = 0;
};
