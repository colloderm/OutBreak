// Fill out your copyright notice in the Description page of Project Settings.

#include "Equipment/Components/OBEquipmentComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/OBCharacterBase.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Weapon/OBWeaponBase.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Weapon/Data/OBWeaponData.h"

UOBEquipmentComponent::UOBEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// 현재 무기 상태를 모두에게 알리기 위해 컴포넌트 복제 활성화.
	SetIsReplicatedByDefault(true);
}

void UOBEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 현재 무기를 모든 클라이언트로 복제.
	DOREPLIFETIME(UOBEquipmentComponent, CurrentWeapon);
}

void UOBEquipmentComponent::EquipWeapon(TSubclassOf<AOBWeaponBase> WeaponClass)
{
	// 스폰/부착은 서버 권위에서만.
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !WeaponClass) return;

	// 기존 무기가 있으면 먼저 해제.
	if (CurrentWeapon)
	{
		UnequipWeapon();
	}

	ACharacter* OwnerCharacter = Cast<ACharacter>(OwnerActor);
	if (!OwnerCharacter) return;

	// 무기 스폰: Owner/Instigator를 캐릭터로 설정(권위 검사·소유자 RPC 대비).
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerCharacter;
	SpawnParams.Instigator = OwnerCharacter;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AOBWeaponBase* NewWeapon = GetWorld()->SpawnActor<AOBWeaponBase>(WeaponClass, SpawnParams);
	if (!NewWeapon) return;

	CurrentWeapon = NewWeapon;

	// 서버는 OnRep이 호출되지 않으므로 여기서 직접 부착(리슨 서버 포함).
	AttachWeaponToOwner();
	
	// 레이어 링크 + draw 몽타주(서버 로컬)
	ApplyCosmeticEquip();
	
	// 무기 데이터의 AbilitySet을 캐릭터 ASC에 부여(발사 능력 등).
	if (UOBWeaponData* Data = NewWeapon->GetWeaponData())
	{
		if (Data->AbilitySet)
		{
			if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerCharacter))
			{
				Data->AbilitySet->GiveToAbilitySystem(ASC, &GrantedAbilityHandles, NewWeapon);
				
				OnWeaponChanged.Broadcast(CurrentWeapon);
			}
		}
	}
	

	// [확장] 장착 시 발사 Ability 부여:
	// UOBAbilitySet이 Ability 부여를 지원하면 여기서 PlayerState ASC에 적용한다.
	// (현재 미지원 → AbilitySet 확장 단계에서 활성화)
	// if (UOBAbilitySet* Set = WeaponData->GetAbilitySet()) { Set->GiveToAbilitySystem(ASC, NewWeapon); }
}

void UOBEquipmentComponent::UnequipWeapon()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority()) return;
	
	// 부여했던 능력/효과 회수.
	if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
	{
		GrantedAbilityHandles.TakeFromAbilitySystem(ASC);
	}

	if (CurrentWeapon)
	{
		RemoveCosmeticEquip();
		
		CurrentWeapon->Destroy();
		CurrentWeapon = nullptr;
		
		OnWeaponChanged.Broadcast(nullptr);
	}
}

void UOBEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 폰이 파괴될 때 부여했던 능력을 확실히 회수(grant 누수 방지).
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		UnequipWeapon();
	}	
	Super::EndPlay(EndPlayReason);
}

void UOBEquipmentComponent::OnRep_CurrentWeapon()
{
	// 클라이언트: 복제된 무기를 소켓에 부착.
	if (CurrentWeapon)
	{
		// 클라이언트: 복제된 무기 부착 + 레이어/draw.
		AttachWeaponToOwner();
		ApplyCosmeticEquip();
		OnWeaponChanged.Broadcast(CurrentWeapon);
	}
	else
	{
		// 무기 제거(사망/해제) → 레이어 해제.
		RemoveCosmeticEquip();
		OnWeaponChanged.Broadcast(nullptr);
	}
}

void UOBEquipmentComponent::AttachWeaponToOwner()
{
	if (!CurrentWeapon) return;

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter || !OwnerCharacter->GetMesh()) return;
	auto ChildMesh = Cast<USkeletalMeshComponent>(OwnerCharacter->GetMesh()->GetChildComponent(0));
	const FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, true);
	CurrentWeapon->AttachToComponent(ChildMesh, AttachRules, AttachSocketName);
}

void UOBEquipmentComponent::ApplyCosmeticEquip()
{
	AOBCharacterBase* Char = Cast<AOBCharacterBase>(GetOwner());
	if (!Char || !CurrentWeapon) return;

	// 몽타주/레이어는 슬롯 보유 소스 메시에 적용(자식 메시는 리타깃으로 반영).
	USkeletalMeshComponent* MontageMesh = Char->GetMontageMesh();
	UAnimInstance* Anim = MontageMesh ? MontageMesh->GetAnimInstance() : nullptr;
	if (!Anim) return;

	UOBWeaponData* Data = CurrentWeapon->GetWeaponData();
	if (!Data) return;

	// 1) 카테고리 포즈 레이어 교체(이전 레이어 해제 후 신규 링크).
	if (LinkedAnimLayer)
	{
		Anim->UnlinkAnimClassLayers(LinkedAnimLayer);
	}
	LinkedAnimLayer = Data->EquippedAnimLayer;
	if (LinkedAnimLayer)
	{
		Anim->LinkAnimClassLayers(LinkedAnimLayer);
	}

	// 2) 꺼내기(draw) 몽타주.
	if (Data->EquipMontage)
	{
		Anim->Montage_Play(Data->EquipMontage);
	}
}

void UOBEquipmentComponent::RemoveCosmeticEquip()
{
	AOBCharacterBase* Char = Cast<AOBCharacterBase>(GetOwner());
	if (!Char) return;

	USkeletalMeshComponent* MontageMesh = Char->GetMontageMesh();
	UAnimInstance* Anim = MontageMesh ? MontageMesh->GetAnimInstance() : nullptr;
	if (!Anim) return;

	if (LinkedAnimLayer)
	{
		Anim->UnlinkAnimClassLayers(LinkedAnimLayer);
		LinkedAnimLayer = nullptr;
	}
}
