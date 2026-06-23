// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/OBCharacterBase.h"

#include "Core/OBCollisionChannels.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/State/OBPlayerStateBase.h"
#include "Ability/Attributes/OBAttributeSetBase.h"
#include "Character/Data/OBPawnData.h"
#include "Equipment/Components/OBEquipmentComponent.h"
#include "Ability/Data/OBAbilitySet.h"
#include "Ability/Tags/OBGameplayTags.h"
#include "Game/GameMode/OBGameModeBase.h"
#include "Net/UnrealNetwork.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Weapon/OBWeaponBase.h"
#include "DrawDebugHelpers.h"

AOBCharacterBase::AOBCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.0f;
	CameraBoom->SocketOffset = FVector(0.f, 50.f, 60.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = true;
	CameraBoom->ProbeChannel = OB_TraceChannel_CameraProbe;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	
	EquipmentComponent = CreateDefaultSubobject<UOBEquipmentComponent>(TEXT("EquipmentComponent"));

	bUseControllerRotationYaw = false;
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bOrientRotationToMovement = true;
		MoveComp->RotationRate = FRotator(0.f, 500.f, 0.f);
	}
	
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(OB_TraceChannel_CameraProbe, ECR_Ignore);
	}
	if (GetMesh())
	{
		GetMesh()->SetCollisionResponseToChannel(OB_TraceChannel_CameraProbe, ECR_Ignore);
	}

}

UAbilitySystemComponent* AOBCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AOBCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	// 사망 상태를 모든 클라이언트로 복제.
	DOREPLIFETIME(AOBCharacterBase, bIsDead);
}

void AOBCharacterBase::HandleDeath()
{
	// 서버 권위 + 중복 방지.
	if (!HasAuthority() || bIsDead) return;
	
	bIsDead = true;
	
	// 능력 정리 + 사망 태그(능력 발동 차단용)
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelAbilities();
		AbilitySystemComponent->AddLooseGameplayTag(OBGameplayTags::State_Dead);
	}
	
	// 무기 해제(부여 능력 회수 + 부기 파괴)
	if (EquipmentComponent)
	{
		EquipmentComponent->UnequipWeapon();
	}
	
	// 서버측 물리/이동 비활성
	DisablePawnForDeath();
	
	// 컨트롤러를 먼저 캡처(리스폰 시 UnPossess로 null 될 수 있음)
	AController* DyingController = GetController();
	if (AOBGameModeBase* GameMode = GetWorld()->GetAuthGameMode<AOBGameModeBase>())
	{
		GameMode->RequestRespawn(DyingController, this);
	}
}

void AOBCharacterBase::Multicast_DrawFireTrace_Implementation(const FVector& Start, const FVector& End, bool bHit)
{
#if ENABLE_DRAW_DEBUG
	DrawDebugLine(GetWorld(), Start, End, bHit ? FColor::Red : FColor::Green, false, 1.0f, 0, 1.0f);
	if (bHit)
	{
		DrawDebugPoint(GetWorld(), End, 8.0f, FColor::Red, false, 1.0f);
	}
#endif
}

void AOBCharacterBase::OnRep_IsDead()
{
	// 클라이언트: 사망 연출(충돌 끄기 등). 래그돌/몽타주는 확wkd
	if (bIsDead)
	{
		DisablePawnForDeath();
		// [확장] 사망 몽타주/래그돌/무기 숨김 → GameplayCue 권장.
	}
}

void AOBCharacterBase::DisablePawnForDeath()
{
	// 충돌 제거
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	
	// 이동 정지/비활성
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
	}
}

void AOBCharacterBase::Multicast_PlayFireMontage_Implementation(UAnimMontage* MontageToPlay)
{
	if (!MontageToPlay || !GetMesh()) return;
	
	// 각 머신의 AnimInstance에서 몽타주 재생(상체 슬롯).
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->Montage_Play(MontageToPlay);
	}
}

void AOBCharacterBase::PossessedBy(AController* NewController)
{
	// [서버 전용] 컨트롤러가 이 폰을 빙의하는 순간 호출.
	Super::PossessedBy(NewController);

	// 1) ASC 초기화
	InitAbilitySystemComponent();
	
	// 부활: 이전 생애의 사망 태그를 제거(ASC는 PlayerState에 영속).
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(OBGameplayTags::State_Dead);
	}
	
	// 2) 기본 AbilitySet 적용(체력 등)
	if (AbilitySystemComponent && DefaultAbilitySet)
	{
		DefaultAbilitySet->GiveToAbilitySystem(AbilitySystemComponent, nullptr, this);
	}
	
	// 3) PawnData의 기본 무기 지급(서버). 로비 캐릭터별로 무기가 달라지는 진입점.
	if (PawnData && EquipmentComponent && PawnData->DefaultWeapon)
	{
		EquipmentComponent->EquipWeapon(PawnData->DefaultWeapon);
	}
}

void AOBCharacterBase::OnRep_PlayerState()
{
	// [클라이언트 전용] 서버로부터 PlayerState가 복제되어 도착한 순간 호출.
	Super::OnRep_PlayerState();

	InitAbilitySystemComponent();
}

void AOBCharacterBase::InitAbilitySystemComponent()
{	
	// 이미 초기화됐으면 중복 실행 방지(OnRep이 여러 번 와도 안전).
	if (bAbilitySystemInitialized) return;
	
	AOBPlayerStateBase* PS = GetPlayerState<AOBPlayerStateBase>();
	if (!PS) return;

	AbilitySystemComponent = PS->GetAbilitySystemComponent();
	AttributeSet = PS->GetAttributeSet();
	if (!AbilitySystemComponent) return;

	// GAS 필수: Owner=PlayerState, Avatar=this. 서버/클라 양쪽 호출.
	AbilitySystemComponent->InitAbilityActorInfo(PS, this);
	
	bAbilitySystemInitialized = true;
	
	// UI 등 구독자에게 ASC 준비 완료를 알린다.
	OnAbilitySystemInitialized.Broadcast();
	
}
