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
#include "Weapon/Data/OBWeaponData.h"

AOBCharacterBase::AOBCharacterBase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

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

void AOBCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		DefaultWalkSpeed = MoveComp->MaxWalkSpeed;
	}
	if (FollowCamera)
	{
		DefaultCameraFOV = FollowCamera->FieldOfView;
		TargetCameraFOV = DefaultCameraFOV;
		
		BaseVignette = FollowCamera->PostProcessSettings.VignetteIntensity;
		BaseMotionBlur = FollowCamera->PostProcessSettings.MotionBlurAmount;
	}
}

UAbilitySystemComponent* AOBCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AOBCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	// 상태에 대한 내용을 모든 클라이언트로 복제.
	DOREPLIFETIME(AOBCharacterBase, bIsDead);
	DOREPLIFETIME(AOBCharacterBase, bIsAiming);
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
	StartDeath();
	
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
	// 클라이언트: 사망 연출(래그돌)
	if (bIsDead)
	{
		StartDeath();
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

void AOBCharacterBase::StartDeath()
{
	DisablePawnForDeath();
	StartRagdoll();
}

void AOBCharacterBase::StartRagdoll()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp) return;
	
	// 메시를 래그돌 프로파일로 전환(월드 충돌, 폰끼리 무시)
	MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));
	MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	
	// 전체 본을 물리 시뮬레이션으로
	MeshComp->SetAllBodiesSimulatePhysics(true);
	MeshComp->SetSimulatePhysics(true);
	MeshComp->WakeAllRigidBodies();
}

void AOBCharacterBase::SetAiming(bool bnewAiming)
{
	if (!HasAuthority()) return;
	bIsAiming = bnewAiming;
	UpdateAimingState(); // 서버 반영
}

void AOBCharacterBase::OnRep_isAiming()
{
	UpdateAimingState(); // 클라 반영
}

void AOBCharacterBase::UpdateAimingState()
{
	// 현재 무기 데이터
	UOBWeaponData* Data = nullptr;
	if (EquipmentComponent)
	{
		if (AOBWeaponBase* Weapon = EquipmentComponent->GetCurrentWeapon())
		{
			Data = Weapon->GetWeaponData();
		}
	}
	
	// 이동 감속(모든 머신: 복제된 bIsAiming + 공유 WeaponData)
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		const float Mult = (bIsAiming && Data) ? Data->ADSSpeedMultiplier : 1.0f;
		MoveComp->MaxWalkSpeed = DefaultWalkSpeed * Mult;
	}
	
	// 카메라 FOV 블렌드(조준하는 본인만)
	if (IsLocallyControlled() && FollowCamera)
	{
		TargetCameraFOV = (bIsAiming && Data) ? Data->ADSFOV : DefaultCameraFOV;
		CameraBlendSpeed = Data ? Data->ADSBlendSpeed : 12.f;
		
		// 조준 중이면 은은한 집중 유지.
		CombatFocusTarget = bIsAiming ? AimFocusBaseline : 0.f;
		
		SetActorTickEnabled(true);
	}
}

void AOBCharacterBase::AddFireFocusPulse(float PulseAmount)
{
	// 로컬 전용
	if (!IsLocallyControlled()) return;
	
	CombatFocus = FMath::Min(1.f, CombatFocus + PulseAmount);
	SetActorTickEnabled(true);
}

void AOBCharacterBase::ApplyCombatFocusPostProcess()
{
	if (!FollowCamera) return;
	
	FPostProcessSettings& PP = FollowCamera->PostProcessSettings;
	
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = FMath::Lerp(BaseVignette, FocusVignette, CombatFocus);
	
	PP.bOverride_MotionBlurAmount = true;
	PP.MotionBlurAmount = FMath::Lerp(BaseMotionBlur, FocusMotionBlur, CombatFocus);
	
	PP.bOverride_SceneFringeIntensity = true;
	PP.SceneFringeIntensity = FMath::Lerp(0.f, FocusFringe, CombatFocus);
}

void AOBCharacterBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	bool bStillBlending = false;

	if (FollowCamera)
	{
		const float NewFOV = FMath::FInterpTo(FollowCamera->FieldOfView, TargetCameraFOV, DeltaSeconds, CameraBlendSpeed);
		FollowCamera->SetFieldOfView(NewFOV);
		if (!FMath::IsNearlyEqual(NewFOV, TargetCameraFOV, 0.1f))
		{
			bStillBlending = true;
		}
		else
		{
			FollowCamera->SetFieldOfView(TargetCameraFOV);
		}
	}
	
	const float NewFocus = FMath::FInterpTo(CombatFocus, CombatFocusTarget, DeltaSeconds, FocusRecoverySpeed);
	if (!FMath::IsNearlyEqual(NewFocus, CombatFocusTarget, 0.001f) || CombatFocus > 0.001f)
	{
		bStillBlending = true;
	}
	CombatFocus = NewFocus;
	ApplyCombatFocusPostProcess();
	
	if (!bStillBlending)
	{
		SetActorTickEnabled(false);
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
