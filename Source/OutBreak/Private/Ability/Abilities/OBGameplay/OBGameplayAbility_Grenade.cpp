// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Abilities/OBGameplay/OBGameplayAbility_Grenade.h"

#include "Character/OBCharacterBase.h"
#include "Weapon/Projectile/OBGrenadeProjectile.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "AbilitySystemComponent.h"

void UOBGameplayAbility_Grenade::ApplyConsumableEffect()
{
	if (!GrenadeClass) return;

	AOBCharacterBase* Character = GetOBCharacterFromActorInfo();
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!Character || !World) return;

	// 던질 시작 위치(손 소켓 우선, 없으면 캐릭터 앞 위쪽).
	FVector SpawnLoc = Character->GetActorLocation() + Character->GetActorForwardVector() * 50.f + FVector(0, 0, 50.f);
	if (USkeletalMeshComponent* Mesh = Character->GetMontageMesh())
	{
		if (Mesh->DoesSocketExist(ThrowSocketName))
		{
			SpawnLoc = Mesh->GetSocketLocation(ThrowSocketName);
		}
	}

	// 던지는 방향: 조준(에임) 방향 + 약간 위로(포물선).
	FVector ThrowDir = Character->GetBaseAimRotation().Vector();
	ThrowDir = (ThrowDir + FVector(0, 0, 0.35f)).GetSafeNormal();

	const FTransform SpawnTM(ThrowDir.Rotation(), SpawnLoc);

	AOBGrenadeProjectile* Grenade = World->SpawnActorDeferred<AOBGrenadeProjectile>(
		GrenadeClass, SpawnTM, Character, Character, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	
	if (!Grenade) return;

	Grenade->InitGrenade(GetAbilitySystemComponentFromActorInfo(), DamageEffect, Damage);
	Grenade->FinishSpawning(SpawnTM);

	if (UProjectileMovementComponent* Move = Grenade->GetProjectileMovement())
	{
		Move->Velocity = ThrowDir * ThrowSpeed;
	}
}
