// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon/OBWeaponBase.h"

#include "Components/SkeletalMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Inventory/Components/OBInventoryComponent.h"
#include "Weapon/Data/OBWeaponData.h"

AOBWeaponBase::AOBWeaponBase()
{
	PrimaryActorTick.bCanEverTick = false;
	
	// 모든 클라이언트가 무기 외형을 봐야 하므로 복제 활성화.
	bReplicates = true;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	SetRootComponent(WeaponMesh);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AOBWeaponBase::BeginPlay()
{
	Super::BeginPlay();
	
	if (HasAuthority())
	{
		InitializeAmmo();
	}
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

void AOBWeaponBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AOBWeaponBase, CurrentAmmo);
}

void AOBWeaponBase::InitializeAmmo()
{
	if (WeaponData)
	{
		CurrentAmmo = WeaponData->MagazineSize;
	}
	OnAmmoChanged.Broadcast();
}

bool AOBWeaponBase::CanReload() const
{
	if (!WeaponData || CurrentAmmo >= WeaponData->MagazineSize) return false;
	
	if (UOBInventoryComponent* Inv = GetOwner() ? GetOwner()->FindComponentByClass<UOBInventoryComponent>() : nullptr)
	{
		return Inv->GetAmmo(WeaponData->AmmoType) > 0;   // 풀에 해당 타입 탄 있어야
	}
	return false;
}

void AOBWeaponBase::ConsumeAmmo(int32 Amount)
{
	if (!HasAuthority()) return;
	CurrentAmmo = FMath::Max(0, CurrentAmmo - Amount);
	OnAmmoChanged.Broadcast();
}

void AOBWeaponBase::PerformReload()
{
	if (!HasAuthority() || !WeaponData) return;

	const int32 Needed = WeaponData->MagazineSize - CurrentAmmo;
	if (Needed <= 0) return;
	
	if (UOBInventoryComponent* Inv = GetOwner() ? GetOwner()->FindComponentByClass<UOBInventoryComponent>() : nullptr)
	{
		const int32 ToLoad = Inv->ConsumeAmmoFromPool(WeaponData->AmmoType, Needed);
		CurrentAmmo += ToLoad;
		OnAmmoChanged.Broadcast();
	}	
}

void AOBWeaponBase::OnRep_Ammo()
{
	// 클라이언트: UI 갱신.
	OnAmmoChanged.Broadcast();
}

void AOBWeaponBase::SetCurrentAmmo(int32 NewAmmo)
{
	if (!HasAuthority() || !WeaponData) return;
	CurrentAmmo = FMath::Clamp(NewAmmo, 0, WeaponData->MagazineSize);
	OnAmmoChanged.Broadcast();
}
