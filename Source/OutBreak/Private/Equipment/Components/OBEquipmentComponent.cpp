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
	
	// [수정] 장착 즉시 탄약 초기화(WeaponData 기준) → CurrentAmmo 세팅 + OnRep_Ammo 복제.
	//        이게 없으면 첫 장전이 한 박자 밀림(속성 미세팅/복제 타이밍).
	NewWeapon->InitializeAmmo();
	
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

	// 총은 "보이는 손"에 있어야 하므로 시각용 자식 메시에 부착한다.
	// 메인(소스) 메시에 붙이면 리타겟 비율 차이만큼 총이 손에서 벗어난다.
	// ponytail: 첫 자식 스켈레탈 메시를 시각용으로 간주. 자식 메시가 2개 이상 되면 이름/태그 지정으로 승급.
	USkeletalMeshComponent* TargetMesh = OwnerCharacter->GetMesh();
	TArray<USceneComponent*> Children;
	OwnerCharacter->GetMesh()->GetChildrenComponents(false, Children);
	for (USceneComponent* C : Children)
	{
		if (USkeletalMeshComponent* SK = Cast<USkeletalMeshComponent>(C)) { TargetMesh = SK; break; }
	}

	// 무기별 소켓(비어 있으면 컴포넌트 기본값).
	const UOBWeaponData* Data = CurrentWeapon->GetWeaponData();
	const FName SocketToUse = (Data && !Data->AttachSocket.IsNone()) ? Data->AttachSocket : AttachSocketName;

	// 소켓이 없으면 컴포넌트 원점(발밑)에 붙어 총이 바닥에 떨어진다. 조용히 실패하지 않도록 경고.
	if (!TargetMesh->DoesSocketExist(SocketToUse))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Equip] 소켓 '%s' 이(가) %s 에 없습니다. 무기가 컴포넌트 원점에 붙습니다."),
			*SocketToUse.ToString(), *TargetMesh->GetName());
	}

	const FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, true);
	CurrentWeapon->AttachToComponent(TargetMesh, AttachRules, SocketToUse);
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

	// 꺼내기(draw) 몽타주.
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
}
