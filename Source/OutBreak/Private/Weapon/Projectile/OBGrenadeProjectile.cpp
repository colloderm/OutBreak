// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon/Projectile/OBGrenadeProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Ability/Tags/OBGameplayTags.h"
#include "TimerManager.h"
#include "AI/EnemyCharacter.h"
#include "AI/Components/EnemyPhysicalComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"


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
	
	// 반경 데미지
	{
		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);

		// 채널 응답 설정에 좌우되지 않도록 오브젝트 타입으로 찾는다.
		FCollisionObjectQueryParams ObjectParams;
		ObjectParams.AddObjectTypesToQuery(ECC_Pawn);

		GetWorld()->OverlapMultiByObjectType(
			Overlaps, Center, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(ExplosionRadius), Params);

		TSet<AActor*> AlreadyHit;
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* Actor = Overlap.GetActor();
			if (!Actor || AlreadyHit.Contains(Actor)) continue;
			AlreadyHit.Add(Actor);

			// 제곱근 감쇠. 선형이면 반경 끝에서 계수가 0에 수렴해
			// 파편이 무력해진다(400 반경, 거리 393 → 계수 0.017).
			const float Dist = FVector::Dist(Center, Actor->GetActorLocation());
			const float Falloff = FMath::Sqrt(FMath::Clamp(1.f - (Dist / ExplosionRadius), 0.f, 1.f));
			const float FinalDamage = Damage * Falloff;

			UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor);

			if (FinalDamage <= 0.f) continue;

			if (TargetASC && DamageEffect && SourceASC.IsValid())
			{
				FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
				Ctx.AddInstigator(GetInstigator(), this);

				FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(DamageEffect, 1.f, Ctx);
				if (Spec.IsValid())
				{
					Spec.Data->SetSetByCallerMagnitude(OBGameplayTags::SetByCaller_Damage, FinalDamage);
					SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
				}
			}
			else
			{
				// 좀비는 ASC가 없다. TakeDamage → EnemyPhysicalComponent 경로로 보낸다.
				UGameplayStatics::ApplyDamage(Actor, FinalDamage, GetInstigatorController(), this, UDamageType::StaticClass());

				// 총격과 동일한 피격 연출. 폭심 쪽 표면에서 피가 튀도록 방향을 잡는다.
				if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Actor))
				{
					if (UEnemyPhysicalComponent* Physical =
						Enemy->FindComponentByClass<UEnemyPhysicalComponent>())
					{
						const FVector ToCenter =
							(Center - Actor->GetActorLocation()).GetSafeNormal();

						// 발밑이 아니라 몸통 높이에서 터지도록 살짝 올린다.
						const FVector ImpactPoint =
							Actor->GetActorLocation() + ToCenter * 40.f + FVector(0.f, 0.f, 60.f);

						// 총알과 같은 규약: 노멀은 표면에서 가해자 쪽을 향한다.
						Physical->Multicast_BloodVFX(ImpactPoint, ToCenter);
					}
				}
			}
		}
	}
	
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

