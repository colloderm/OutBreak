// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon/Projectile/OBGrenadeProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Weapon/OBExplosionLibrary.h"


// Sets default values
AOBGrenadeProjectile::AOBGrenadeProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	
	bReplicates = true;
	SetReplicateMovement(true);
	
	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
	CollisionComp->InitSphereRadius(8.f);
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComp->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionComp->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	SetRootComponent(CollisionComp);
	
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(CollisionComp);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComp;
	ProjectileMovement->InitialSpeed = 1500.f;
	ProjectileMovement->MaxSpeed = 2000.f;
	ProjectileMovement->bShouldBounce = true;
	ProjectileMovement->Bounciness = 0.3f;
	ProjectileMovement->Friction = 0.4f;
	ProjectileMovement->ProjectileGravityScale = 1.f;
}

void AOBGrenadeProjectile::InitGrenade(UAbilitySystemComponent* InSourceASC, TSubclassOf<UGameplayEffect> InDamageEffect, float InDamage)
{
	SourceASC = InSourceASC;
	DamageEffect = InDamageEffect;
	Damage = InDamage;
	
	// 발사자와 충돌 무시(즉시 폭발/막힘 방지)
	if (AActor* MyInstigator = GetInstigator())
	{
		CollisionComp->IgnoreActorWhenMoving(MyInstigator, true);
	}
}

void AOBGrenadeProjectile::BeginPlay()
{
	Super::BeginPlay();
	
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(FuseTimer, this, &AOBGrenadeProjectile::Explode, FuseTime, false);
	}
}

void AOBGrenadeProjectile::Explode()
{
	if (!HasAuthority()) return;
	
	const FVector Center = GetActorLocation();
	
	// 연출(서버/클라)
	Multicast_OnExploded(Center);
	
	// 반경 데미지. 차량 폭발과 같은 경로를 쓴다.
	UOBExplosionLibrary::ApplyExplosion(this, Center, ExplosionRadius, Damage, DamageEffect,
		GetInstigatorController(), this, ExplosionImpulseStrength, TArray<AActor*>(), SourceASC.Get());
	
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
	}
	SetLifeSpan(2.0f);
}

void AOBGrenadeProjectile::Multicast_OnExploded_Implementation(FVector Location)
{
	OnExplodedCosmetic(Location);
}

