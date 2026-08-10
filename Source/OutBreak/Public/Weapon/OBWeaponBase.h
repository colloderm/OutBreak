// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Inventory/Data/InventoryData.h"
#include "Weapon/Data/OBWeaponDefinition.h"
#include "OBWeaponBase.generated.h"

DECLARE_MULTICAST_DELEGATE(FOBOnAmmoChanged);

class UStaticMesh;
class USkeletalMeshComponent;
class UStaticMeshComponent;
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

	const FOBWeaponDefinitionRow* GetWeaponDefinition() const;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	const FOBResolvedWeaponStats& GetResolvedStats() const { return ResolvedStats; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	FGameplayTag GetItemTag() const { return ItemTag; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	FGuid GetItemInstanceId() const { return ItemInstanceId; }

	void InitializeFromItemInstance(const FInventoryData& ItemInstance);
	
	UFUNCTION(BlueprintPure, Category = "Weapon")
	USkeletalMeshComponent* GetWeaponMesh() const { return WeaponMesh; }
	
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
	UFUNCTION() void OnRep_WeaponInstance();
	void RebuildAttachmentVisuals();
	
	// 정의(DT)의 메시·머즐 소켓을 적용하고 부착물을 재구성한다.
	// 서버/클라 양쪽에서 불러야 한다. InitializeFromItemInstance는 서버 전용이다.
	void ApplyWeaponVisuals();

	// 스태틱 메시 하나를 소켓에 붙이고 정리 목록에 등록.
	void SpawnAttachmentMesh(UStaticMesh* Mesh, FName Socket);
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UOBWeaponData> WeaponData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FName MuzzleSocketName = TEXT("Muzzle");

	UPROPERTY(ReplicatedUsing = OnRep_WeaponInstance, BlueprintReadOnly, Category = "Weapon|Instance")
	FGameplayTag ItemTag;

	UPROPERTY(ReplicatedUsing = OnRep_WeaponInstance, BlueprintReadOnly, Category = "Weapon|Instance")
	FGuid ItemInstanceId;

	UPROPERTY(ReplicatedUsing = OnRep_WeaponInstance, BlueprintReadOnly, Category = "Weapon|Instance")
	FOBResolvedWeaponStats ResolvedStats;

	UPROPERTY(ReplicatedUsing = OnRep_WeaponInstance, BlueprintReadOnly, Category = "Weapon|Instance")
	TArray<FOBInstalledAttachment> InstalledAttachments;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> AttachmentMeshComponents;
	
	// 현재 탄창 탄약.
	UPROPERTY(ReplicatedUsing = OnRep_Ammo)
	int32 CurrentAmmo = 0;
};
