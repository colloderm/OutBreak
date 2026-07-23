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
#include "Inventory/Components/OBInventoryComponent.h"
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
#include "Character/Animation/OBAnimInstance.h"
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
	
	InventoryComponent = CreateDefaultSubobject<UOBInventoryComponent>(TEXT("InventoryComponent"));

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
	DOREPLIFETIME(AOBCharacterBase, bIsDowned);
}

void AOBCharacterBase::HandleDeath()
{
	// 서버 권위 + 중복 방지.
	if (!HasAuthority() || bIsDead || bIsDowned) return;
	
	// 컨트롤러를 먼저 캡처(리스폰 시 UnPossess로 null 될 수 있음)
	AController* C = GetController();
	AOBGameModeBase* GM = GetWorld()->GetAuthGameMode<AOBGameModeBase>();

	// 팀에 생존자 있으면 다운(부활 여지), 없으면 즉시 사망.
	if (GM && GM->ShouldEnterDownedState(C))
	{
		EnterDownedState();
		GM->NotifyPlayerDowned(C);
		return;
	}
	
	// --- 즉시 사망 ---
	bIsDead = true;
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelAbilities();
		AbilitySystemComponent->AddLooseGameplayTag(OBGameplayTags::State_Dead);
	}
	
	if (EquipmentComponent)
		EquipmentComponent->UnequipWeapon();
	
	// 서버측 물리/이동 비활성
	StartDeath();
	
	if (GM)
		GM->RequestRespawn(C, this);
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
	if (DeathMontage)   // 죽는 몽타주 재생 후
	{
		float Dur = 0.f;
		if (USkeletalMeshComponent* M = GetMesh())
			if (UAnimInstance* Anim = M->GetAnimInstance())
				Dur = Anim->Montage_Play(DeathMontage);
		// 몽타주 끝나면 래그돌(타이머).
		FTimerHandle H;
		GetWorldTimerManager().SetTimer(H, this, &AOBCharacterBase::StartRagdoll, FMath::Max(0.1f, Dur - 0.1f), false);
	}
	else
	{
		StartRagdoll(); // 기존: 즉시 래그돌
	}
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
	
	UpdateCombatOrientation();   // 조준 변화 시 지향 갱신
}

float AOBCharacterBase::GetCurrentSpreadAngle() const
{
	const UOBEquipmentComponent* Equip = FindComponentByClass<UOBEquipmentComponent>();
	const AOBWeaponBase* Weapon = Equip ? Equip->GetCurrentWeapon() : nullptr;
	const UOBWeaponData* Data = Weapon ? Weapon->GetWeaponData() : nullptr;
	if (!Data) return 0.f;
	
	float Spread = Data->BaseSpreadDegrees;
	if (bIsAiming)
		Spread *= Data->ADSSpeedMultiplier;
	if (GetVelocity().SizeSquared2D() > FMath::Square(10.f))
		Spread *= Data->MovingSpreadMultiplier;
	
	return Spread;
}

void AOBCharacterBase::AddFireFocusPulse(float PulseAmount)
{
	// 로컬 전용
	if (!IsLocallyControlled()) return;
	
	CombatFocus = FMath::Min(1.f, CombatFocus + PulseAmount);
	SetActorTickEnabled(true);
}

USkeletalMeshComponent* AOBCharacterBase::GetMontageMesh() const
{
	// 메인 Mesh가 게임플레이 AnimInstance(슬롯)를 돌리면 그것 사용(소스 메시).
	if (GetMesh() && GetMesh()->GetAnimInstance() && GetMesh()->GetAnimInstance()->IsA<UOBAnimInstance>())
	{
		return GetMesh();
	}
	// 아니면 자식 중에서 UOBAnimInstance 가진 메시 탐색.
	TArray<USkeletalMeshComponent*> Meshes;
	GetComponents<USkeletalMeshComponent>(Meshes);
	for (USkeletalMeshComponent* Comp : Meshes)
	{
		if (Comp && Comp->GetAnimInstance() && Comp->GetAnimInstance()->IsA<UOBAnimInstance>())
		{
			return Comp;
		}
	}
	return GetMesh(); // 폴백
}

void AOBCharacterBase::NotifyFired()
{
	bRecentlyFired = true;
	UpdateCombatOrientation();
	GetWorldTimerManager().SetTimer(
		CombatOrientTimer, this, &AOBCharacterBase::ClearRecentlyFired, CombatOrientHoldTime, false
	);
}

void AOBCharacterBase::HandleExtracted()
{
	if (!HasAuthority()) return;

	// 진행 중 능력 즉시 취소 → 발사 트레이스/데미지/큐 중단.
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelAbilities();
	}
	
	// 무기 해제(부여된 발사 능력 회수 → 재발사 불가).
	if (EquipmentComponent)
	{
		EquipmentComponent->UnequipWeapon();
	}
	// (폰 숨김/충돌·이동 정지는 GameMode가 처리)
}

void AOBCharacterBase::EnterDownedState()
{
	if (!HasAuthority()) return;
	bIsDowned = true; // 복제 -> OnRep_IsDowned(다운 연출)
	
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelAbilities();
		AbilitySystemComponent->AddLooseGameplayTag(OBGameplayTags::State_Downed);
	}
	if (EquipmentComponent)
		EquipmentComponent->UnequipWeapon();
	
	// 이동만 정지(충돌은 유지 -> 부활 상호작용 가능)
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}
}

void AOBCharacterBase::ReviveFromDowned(float HealthFraction)
{
	if (!HasAuthority() || !bIsDowned) return;
	bIsDowned = false;
	
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(OBGameplayTags::State_Downed);
		// 체력 일부 회복(프로젝트 AttributeSet 접근자에 맞게).
		const float NewHealth = FMath::Max(1.f, UOBAttributeSetBase::GetMaxHealthAttribute().GetNumericValue(
			AbilitySystemComponent->GetAttributeSet(UOBAttributeSetBase::StaticClass())) * HealthFraction);
		AbilitySystemComponent->SetNumericAttributeBase(UOBAttributeSetBase::GetHealthAttribute(), NewHealth);
	}
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
		Move->SetMovementMode(MOVE_Walking);
}

void AOBCharacterBase::FinishDeathFromDowned()
{
	if (!HasAuthority()) return;
	bIsDowned = false;
	bIsDead = true;
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(OBGameplayTags::State_Downed);
		AbilitySystemComponent->AddLooseGameplayTag(OBGameplayTags::State_Dead);
	}
	StartDeath(); // 래그돌
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

void AOBCharacterBase::ClearRecentlyFired()
{
	bRecentlyFired = false;
	UpdateCombatOrientation();
}

void AOBCharacterBase::UpdateCombatOrientation()
{
	// 조준 중이거나 방금 발사했으면 조준 방향(컨트롤 회전)을 바라봄.
	const bool bCombat = bIsAiming || bRecentlyFired;

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bOrientRotationToMovement = !bCombat;   // 평소엔 이동 방향
	}
	
	// GASP 애님 측 회전 모드도 같이 전환. 이걸 빼면 캡슐만 돌고 몸통은 천천히 따라온다.
	if (bCombat != bLastCombatOrientation)
	{
		bLastCombatOrientation = bCombat;
		OnCombatOrientationChanged(bCombat);
	}
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
	
	USkeletalMeshComponent* MontageMesh = GetMontageMesh();

	if (MontageMesh)
	{
		if (UAnimInstance* AnimInstance = MontageMesh->GetAnimInstance())
		{
			AnimInstance->Montage_Play(MontageToPlay);
		}
	}
}

void AOBCharacterBase::PossessedBy(AController* NewController)
{
	// [서버 전용] 컨트롤러가 이 폰을 빙의하는 순간 호출.
	Super::PossessedBy(NewController);

	// 1) ASC 초기화
	InitAbilitySystemComponent();
	
	// 2) 부활 사망 태그 제거
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(OBGameplayTags::State_Dead);
	}
	
	// 3) 기본 AbilitySet(체력 등)
	if (AbilitySystemComponent && DefaultAbilitySet)
	{
		DefaultAbilitySet->GiveToAbilitySystem(AbilitySystemComponent, nullptr, this);
	}
	
	// 4) 로드아웃: 로비 선택 무기 우선, 없으면 PawnData 기본.
	TArray<TSubclassOf<AOBWeaponBase>> Loadout;
	if (AOBPlayerStateBase* PS = GetPlayerState<AOBPlayerStateBase>())
	{
		Loadout = PS->GetSelectedWeapons();
	}
	if (Loadout.Num() == 0 && PawnData)
	{
		Loadout = PawnData->DefaultWeapons;
	}

	if (InventoryComponent)
	{
		for (const TSubclassOf<AOBWeaponBase>& WeaponClass : Loadout)
		{
			if (WeaponClass) InventoryComponent->AddWeapon(WeaponClass);
		}
		if (PawnData)
		{
			for (const TPair<FGameplayTag, int32>& Item : PawnData->StartingItems)
				InventoryComponent->AddItem(Item.Key, Item.Value);
		}
		InventoryComponent->EquipDefaultSlot();
	}
}

void AOBCharacterBase::OnRep_PlayerState()
{
	// [클라이언트 전용] 서버로부터 PlayerState가 복제되어 도착한 순간 호출.
	Super::OnRep_PlayerState();

	InitAbilitySystemComponent();
}

void AOBCharacterBase::OnRep_IsDowned()
{
	if (bIsDowned && DownedEnterMontage)      // EditDefaultsOnly UAnimMontage*
	{
		if (USkeletalMeshComponent* M = GetMesh())
			if (UAnimInstance* Anim = M->GetAnimInstance())
				Anim->Montage_Play(DownedEnterMontage);
	}
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
	
	// 몽타주가 재생될 메시를 슬롯 보유 메시로 고정(리타깃 자식 메시 셋업 대응).
	if (AbilitySystemComponent && AbilitySystemComponent->AbilityActorInfo.IsValid())
	{
		AbilitySystemComponent->AbilityActorInfo->SkeletalMeshComponent = GetMontageMesh();
	}
	
	bAbilitySystemInitialized = true;
	
	// UI 등 구독자에게 ASC 준비 완료를 알린다.
	OnAbilitySystemInitialized.Broadcast();
	
}
