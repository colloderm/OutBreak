// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "OBGrenadeProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UGameplayEffect;
class UAbilitySystemComponent;

UCLASS()
class OUTBREAK_API AOBGrenadeProjectile : public AActor
{
	GENERATED_BODY()

public:
	AOBGrenadeProjectile();

	
	// 스폰 직후 발사자 정보 주입
	void InitGrenade(UAbilitySystemComponent* InSourceASC, TSubclassOf<UGameplayEffect> InDamageEffect, float InDamage);
	
	UProjectileMovementComponent* GetProjectileMovement() const { return ProjectileMovement; }
protected:
	virtual void BeginPlay() override; 
	
	// 서버: 반경 데미지 + 연출 + 파괴.
	void Explode();

	// 폭발 연출(모든 머신). BP에서 VFX/SFX 구현.
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_OnExploded(FVector Location);

	UFUNCTION(BlueprintImplementableEvent, Category = "Grenade")
	void OnExplodedCosmetic(const FVector& Location);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Grenade")
	TObjectPtr<USphereComponent> CollisionComp;

	UPROPERTY(VisibleAnywhere, Category = "Grenade")
	TObjectPtr<UStaticMeshComponent> MeshComp;

	UPROPERTY(VisibleAnywhere, Category = "Grenade")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	// 던진 뒤 폭발까지(초).
	UPROPERTY(EditDefaultsOnly, Category = "Grenade", Meta = (ClampMin = "0.0"))
	float FuseTime = 2.5f;

	// 폭발 반경(cm).
	UPROPERTY(EditDefaultsOnly, Category = "Grenade", Meta = (ClampMin = "0.0"))
	float ExplosionRadius = 400.f;

	// 발사자 ASC(데미지 소스).
	TWeakObjectPtr<UAbilitySystemComponent> SourceASC;

	// 데미지 GE(무기와 동일한 GE_Damage 재사용 가능).
	UPROPERTY()
	TSubclassOf<UGameplayEffect> DamageEffect;

	float Damage = 80.f;

	FTimerHandle FuseTimer;
};
