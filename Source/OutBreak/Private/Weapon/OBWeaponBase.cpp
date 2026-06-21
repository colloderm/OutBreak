// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon/OBWeaponBase.h"

#include "Components/SkeletalMeshComponent.h"

AOBWeaponBase::AOBWeaponBase()
{
	PrimaryActorTick.bCanEverTick = false;
	
	// 모든 클라이언트가 무기 외형을 봐야 하므로 복제 활성화.
	bReplicates = true;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	SetRootComponent(WeaponMesh);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

FVector AOBWeaponBase::GetMuzzleLocation() const
{
	// 머즐 소켓이 있으면 그 위치를, 없으면 메시 원점을 반환(안전 폴백).
	if (WeaponMesh && WeaponMesh->DoesSocketExist(MuzzleSocketName))
	{
		return WeaponMesh->GetSocketLocation(MuzzleSocketName);
	}
	return WeaponMesh ? WeaponMesh->GetComponentLocation() : GetActorLocation();
}
