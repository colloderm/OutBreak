// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon/OBWeaponBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/OBGameDataSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Inventory/Components/PlayerInventoryComponent.h"
#include "Item/OBItemRegistry.h"
#include "Weapon/Data/OBWeaponData.h"
#include "Weapon/Data/OBWeaponStatResolver.h"

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
		FInventoryData DefaultInstance;
		DefaultInstance.ItemTag = UOBItemRegistry::FindTagForWeaponClass(GetClass());
		DefaultInstance.ItemStack = 1;
		DefaultInstance.InstanceId = FGuid::NewGuid();
		if (DefaultInstance.ItemTag.IsValid())
		{
			InitializeFromItemInstance(DefaultInstance);
		}
		else
		{
			InitializeAmmo();
		}
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
	DOREPLIFETIME(AOBWeaponBase, ItemTag);
	DOREPLIFETIME(AOBWeaponBase, ItemInstanceId);
	DOREPLIFETIME(AOBWeaponBase, ResolvedStats);
	DOREPLIFETIME(AOBWeaponBase, InstalledAttachments);
}

const FOBWeaponDefinitionRow* AOBWeaponBase::GetWeaponDefinition() const
{
	const UOBGameDataSubsystem* GameData = UOBGameDataSubsystem::Get();
	return GameData ? GameData->FindWeapon(ItemTag) : nullptr;
}

void AOBWeaponBase::InitializeFromItemInstance(const FInventoryData& ItemInstance)
{
	if (!HasAuthority())
	{
		return;
	}

	FOBResolvedWeaponStats NewStats;
	if (!UOBWeaponStatResolver::ResolveWeaponStats(
		ItemInstance,
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()),
		NewStats))
	{
		return;
	}

	ItemTag = ItemInstance.ItemTag;
	ItemInstanceId = ItemInstance.InstanceId.IsValid() ? ItemInstance.InstanceId : FGuid::NewGuid();
	ResolvedStats = NewStats;
	InstalledAttachments = ItemInstance.Attachments;
	
	CurrentAmmo = ResolvedStats.WeaponType == EOBWeaponType::Ranged
		? FMath::Clamp(
			ItemInstance.MagazineAmmo >= 0 ? ItemInstance.MagazineAmmo : ResolvedStats.MagazineSize,
			0,
			ResolvedStats.MagazineSize)
		: 0;
	ApplyWeaponVisuals();
	OnAmmoChanged.Broadcast();
}

void AOBWeaponBase::InitializeAmmo()
{
	if (ItemTag.IsValid())
	{
		CurrentAmmo = ResolvedStats.WeaponType == EOBWeaponType::Ranged
			? ResolvedStats.MagazineSize
			: 0;
	}
	else if (WeaponData && WeaponData->WeaponType == EOBWeaponType::Ranged)
	{
		CurrentAmmo = WeaponData->MagazineSize;
	}
	else
	{
		CurrentAmmo = 0;
	}
	OnAmmoChanged.Broadcast();
}

bool AOBWeaponBase::CanReload() const
{
	const bool bResolved = ItemTag.IsValid();
	const EOBWeaponType WeaponType = bResolved
		? ResolvedStats.WeaponType
		: (WeaponData ? WeaponData->WeaponType : EOBWeaponType::Melee);
	const int32 MagazineSize = bResolved
		? ResolvedStats.MagazineSize
		: (WeaponData ? WeaponData->MagazineSize : 0);
	const FGameplayTag AmmoType = bResolved
		? ResolvedStats.AmmoType
		: (WeaponData ? WeaponData->AmmoType : FGameplayTag());
	if (WeaponType != EOBWeaponType::Ranged || CurrentAmmo >= MagazineSize || !AmmoType.IsValid()) return false;
	
	if (UPlayerInventoryComponent* Inv = GetOwner()
		? GetOwner()->FindComponentByClass<UPlayerInventoryComponent>()
		: nullptr)
	{
		return Inv->GetAmmo(AmmoType) > 0;
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
	if (!HasAuthority() || !CanReload()) return;

	const int32 MagazineSize = ItemTag.IsValid() ? ResolvedStats.MagazineSize : WeaponData->MagazineSize;
	const FGameplayTag AmmoType = ItemTag.IsValid() ? ResolvedStats.AmmoType : WeaponData->AmmoType;
	const int32 Needed = MagazineSize - CurrentAmmo;
	if (Needed <= 0) return;
	
	if (UPlayerInventoryComponent* Inv = GetOwner()
		? GetOwner()->FindComponentByClass<UPlayerInventoryComponent>()
		: nullptr)
	{
		const int32 ToLoad = Inv->ConsumeAmmoFromPool(AmmoType, Needed);
		CurrentAmmo += ToLoad;
		OnAmmoChanged.Broadcast();
	}	
}

void AOBWeaponBase::OnRep_Ammo()
{
	// 클라이언트: UI 갱신.
	OnAmmoChanged.Broadcast();
}

void AOBWeaponBase::OnRep_WeaponInstance()
{
	ApplyWeaponVisuals();
	OnAmmoChanged.Broadcast();
}

void AOBWeaponBase::ApplyWeaponVisuals()
{
	if (const FOBWeaponDefinitionRow* Definition = GetWeaponDefinition())
	{
		if (WeaponMesh)
		{
			if (USkeletalMesh* Mesh = Definition->Visual.WeaponMesh.LoadSynchronous())
			{
				WeaponMesh->SetSkeletalMeshAsset(Mesh);
			}
		}
		MuzzleSocketName = Definition->Ranged.MuzzleSocket;
	}
	RebuildAttachmentVisuals();
}

void AOBWeaponBase::RebuildAttachmentVisuals()
{
	for (UStaticMeshComponent* Component : AttachmentMeshComponents)
	{
		if (Component)
		{
			Component->DestroyComponent();
		}
	}
	AttachmentMeshComponents.Reset();

	const FOBWeaponDefinitionRow* WeaponDefinition = GetWeaponDefinition();
	if (!WeaponDefinition || !WeaponMesh) return;

	// 고정 파츠 먼저. 부착물과 같은 배열로 관리해야 무기 교체 시 한 번에 정리된다.
	for (const FOBWeaponFixedPart& FixedPart : WeaponDefinition->Visual.FixedParts)
	{
		SpawnAttachmentMesh(FixedPart.Mesh.LoadSynchronous(), FixedPart.Socket);
	}

	const UOBGameDataSubsystem* GameData = UOBGameDataSubsystem::Get();
	if (!GameData) return;

	for (const FOBInstalledAttachment& Installed : InstalledAttachments)
	{
		const FOBAttachmentDefinitionRow* Attachment = GameData->FindAttachment(Installed.ItemTag);
		UStaticMesh* Mesh = Attachment ? Attachment->Mesh.LoadSynchronous() : nullptr;
		if (!Attachment || !Mesh) continue;

		// 부착물 행의 소켓이 슬롯 소켓보다 우선한다.
		// Squared Suppressor처럼 같은 슬롯이라도 다른 소켓을 쓰는 파츠가 있다.
		FName Socket = Attachment->AttachSocket;
		if (Socket.IsNone())
		{
			if (const FOBAttachmentSlotSpec* Slot = WeaponDefinition->AttachmentSlots.FindByPredicate(
				[&Installed](const FOBAttachmentSlotSpec& Candidate)
				{
					return Candidate.SlotTag == Installed.SlotTag;
				}))
			{
				Socket = Slot->MeshSocket;
			}
		}

		SpawnAttachmentMesh(Mesh, Socket);
	}
}

void AOBWeaponBase::SpawnAttachmentMesh(UStaticMesh* Mesh, FName Socket)
{
	if (!Mesh || !WeaponMesh) return;

	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
	Component->SetStaticMesh(Mesh);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetIsReplicated(false);
	Component->RegisterComponent();
	Component->AttachToComponent(WeaponMesh, FAttachmentTransformRules::SnapToTargetIncludingScale, Socket);
	AttachmentMeshComponents.Add(Component);
}

void AOBWeaponBase::SetCurrentAmmo(int32 NewAmmo)
{
	if (!HasAuthority()) return;
	const int32 MagazineSize = ItemTag.IsValid() ? ResolvedStats.MagazineSize : (WeaponData ? WeaponData->MagazineSize : 0);
	CurrentAmmo = FMath::Clamp(NewAmmo, 0, MagazineSize);
	
	OnAmmoChanged.Broadcast();
}
