// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/OBCharacterBase.h"

#include "Core/OBCollisionChannels.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/GameplayCameraComponentBase.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/State/OBPlayerStateBase.h"
#include "Ability/Attributes/OBAttributeSetBase.h"
#include "Inventory/Components/PlayerInventoryComponent.h"
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
#include "Character/Components/OBCharacterMovementComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "TimerManager.h"
#include "AI/EnemyController.h"
#include "Item/Loot/OBLootContainer.h"
#include "Item/Data/OBItemDefinition.h"
#include "Game/GameState/OBExpeditionGameState.h"
#include "Player/Controller/OBPlayerController.h"
#include "Data/OBGameDataSubsystem.h"
#include "Player/Data/OBPlayerStatData.h"

DEFINE_LOG_CATEGORY_STATIC(LogOBCameraProvider, Log, All);

FGenericTeamId AOBCharacterBase::GetGenericTeamId() const
{
    return TeamId;
}

AOBCharacterBase::AOBCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UOBCharacterMovementComponent>(
		ACharacter::CharacterMovementComponentName))
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
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = NormalCameraLagSpeed;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	
	EquipmentComponent = CreateDefaultSubobject<UOBEquipmentComponent>(TEXT("EquipmentComponent"));
	
	PlayerInventoryComponent = CreateDefaultSubobject<UPlayerInventoryComponent>(TEXT("PlayerInventoryComponent"));

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

	// Gameplay Cameras owns the evaluated view for player Blueprints that carry
	// this component. Disable the inherited native camera once, at provider setup,
	// so AActor::CalcCamera never selects two competing camera providers.
	if (FollowCamera && FindComponentByClass<UGameplayCameraComponentBase>())
	{
		FollowCamera->Deactivate();
		UE_LOG(LogOBCameraProvider, Log,
			TEXT("[CameraProvider] Gameplay Camera selected Character=%s NativeFollowCameraActive=false"),
			*GetName());
	}
	
	if (EquipmentComponent)
	{
		EquipmentComponent->OnWeaponChanged.AddUObject(this, &AOBCharacterBase::HandleWeaponChanged);
		HandleWeaponChanged(EquipmentComponent->GetCurrentWeapon()); // 이미 장착 중이면 즉시 반영
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
	DOREPLIFETIME(AOBCharacterBase, LastHitDirection);
	DOREPLIFETIME(AOBCharacterBase, LastHitBoneName);
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
	DropCorpseLoot();   // 장비를 잃는 것 = 다른 사람이 주울 수 있게 남기는 것
	
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelAbilities();
		AbilitySystemComponent->AddLooseGameplayTag(OBGameplayTags::State_Dead);
	}
	
	if (EquipmentComponent)
		EquipmentComponent->UnequipWeapon();

	// 죽은 플레이어는 AI에게 존재하지 않는다. 이게 없으면 시체를 계속 표적으로
	// 잡고, 표적이 유효하니 다른 자극도 무시한다.
	AEnemyController::ForgetActorForAll(GetWorld(), this);
	
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

void AOBCharacterBase::DropCorpseLoot()
{
	// 다운→사망 경로에서도 불릴 수 있어 중복을 막는다.
	if (!HasAuthority() || bCorpseDropped || !CorpseContainerClass) return;

	// 홈/로비에서의 사망(테스트 등)은 시체를 남기지 않는다.
	UWorld* W = GetWorld();
	if (!W || !W->GetGameState<AOBExpeditionGameState>()) return;

	bCorpseDropped = true;
	
	if (!PlayerInventoryComponent) return;

	// 장착 슬롯 + 가방 내용물 전부. 죽으면 다 잃는다.
	// 인스턴스째 넘기므로 무기의 탄창 잔탄과 InstanceId가 보존된다.
	TArray<FInventoryData> Items;
	PlayerInventoryComponent->GetLootableItemInstances(Items);
	if (Items.IsEmpty()) return;   // 빈 시체를 월드에 남기지 않는다.

	const float HalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
	const FVector DropLoc = GetActorLocation() - FVector(0.f, 0.f, HalfHeight);

	// 스폰에 실패하면 인벤토리를 비우지 않는다. 아이템이 증발하는 것보단 낫다.
	if (AOBLootContainer::SpawnWithItemInstances(W, CorpseContainerClass, FTransform(FRotator::ZeroRotator, DropLoc), Items))
	{
		PlayerInventoryComponent->ClearLootableItemInstances();
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
	
	// 맞은 방향으로 쓰러진다. 방향이 없으면(낙사·출혈 등) 그냥 무너진다.
	if (!LastHitDirection.IsNearlyZero() && RagdollImpulseStrength > 0.f)
	{
		const FName Bone = (LastHitBoneName != NAME_None) ? LastHitBoneName : MeshComp->GetBoneName(0);

		MeshComp->AddImpulse(LastHitDirection * RagdollImpulseStrength, Bone, /*bVelChange=*/false);
	}
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
	const AOBWeaponBase* Weapon = EquipmentComponent
		? EquipmentComponent->GetCurrentWeapon()
		: nullptr;
	const FOBResolvedWeaponStats* Stats = Weapon && Weapon->GetItemTag().IsValid()
		? &Weapon->GetResolvedStats()
		: nullptr;
	
	// 이동 감속(모든 머신: 복제된 bIsAiming + 공유 WeaponData)
	if (UOBCharacterMovementComponent* MoveComp = Cast<UOBCharacterMovementComponent>(GetCharacterMovement()))
	{
		// 기동성(무기 무게) × ADS 감속. 맨손 = 1.0(최고속).
		const float PlayerMoveMultiplier = AttributeSet
			? AttributeSet->GetMoveSpeedMultiplier()
			: 1.f;
		const float Mobility = (Stats ? Stats->MobilityMultiplier : 1.0f) * PlayerMoveMultiplier;
		const float AimMult  = (bIsAiming && Stats) ? Stats->ADSSpeedMultiplier : 1.0f;
		MoveComp->SetSpeedMultipliers(Mobility, AimMult);
	}
	
	// 카메라 FOV 블렌드(조준하는 본인만)
	if (IsLocallyControlled() && FollowCamera)
	{
		TargetCameraFOV = (bIsAiming && Stats) ? Stats->ADSFOV : DefaultCameraFOV;
		CameraBlendSpeed = Stats ? Stats->ADSBlendSpeed : 12.f;
		
		// 조준 중이면 은은한 집중 유지.
		CombatFocusTarget = bIsAiming ? AimFocusBaseline : 0.f;
		
		SetActorTickEnabled(true);
	}
	
	UpdateCombatOrientation();   // 조준 변화 시 지향 갱신
}

void AOBCharacterBase::HandleWeaponChanged(AOBWeaponBase* NewWeapon)
{
	// 속도·FOV·지향 갱신 로직이 전부 여기 모여 있으므로 그대로 재사용.
	UpdateAimingState();

	// 인벤토리 프리뷰 캡처는 ShowOnly 목록만 그린다. 무기는 캐릭터와 별개 액터라
	// 넣어주지 않으면 손이 빈 채로 찍힌다. 캐릭터 메시는 BP에서
	// ShowOnlyActorComponents로 이미 등록돼 있고, 그건 다른 배열이라 Reset해도 안 지워진다.
	if (USceneCaptureComponent2D* PreviewCapture =
		FindComponentByClass<USceneCaptureComponent2D>())
	{
		PreviewCapture->ShowOnlyActors.Reset();
		if (NewWeapon)
		{
			PreviewCapture->ShowOnlyActors.Add(NewWeapon);
		}
	}
}

float AOBCharacterBase::GetCurrentSpreadAngle() const
{
	const UOBEquipmentComponent* Equip = FindComponentByClass<UOBEquipmentComponent>();
	const AOBWeaponBase* Weapon = Equip ? Equip->GetCurrentWeapon() : nullptr;
	if (!Weapon || !Weapon->GetItemTag().IsValid()) return 0.f;
	const FOBResolvedWeaponStats& Stats = Weapon->GetResolvedStats();
	
	float Spread = Stats.BaseSpreadDegrees;
	if (bIsAiming)
		Spread *= Stats.ADSSpreadMultiplier;
	if (GetVelocity().SizeSquared2D() > FMath::Square(10.f))
		Spread *= Stats.MovingSpreadMultiplier;
	
	return Spread;
}

void AOBCharacterBase::SetSprintCameraLag(bool bSprinting)
{
	TargetCameraLagSpeed = bSprinting ? SprintCameraLagSpeed : NormalCameraLagSpeed;
	SetActorTickEnabled(true);
}

void AOBCharacterBase::SetSprintInput(bool bNewSprinting)
{
	if (bSprintInputHeld == bNewSprinting) return;

	bSprintInputHeld = bNewSprinting;
	SetSprintCameraLag(bNewSprinting);

	// 발사 차단은 서버에서도 같은 결론이 나와야 예측이 롤백되지 않는다.
	if (!HasAuthority() && IsLocallyControlled())
	{
		Server_SetSprintInput(bNewSprinting);
	}
}

void AOBCharacterBase::Server_SetSprintInput_Implementation(bool bNewSprinting)
{
	bSprintInputHeld = bNewSprinting;
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
	
	// 탈출한 플레이어는 AI에게 존재하지 않는다.
	// 폰을 숨기고 충돌을 꺼도 Sight는 그대로 보므로 인지 소스에서 빼야 한다.
	AEnemyController::ForgetActorForAll(GetWorld(), this);
	
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
	
	DropCorpseLoot();
	
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(OBGameplayTags::State_Downed);
		AbilitySystemComponent->AddLooseGameplayTag(OBGameplayTags::State_Dead);
	}
	
	// 다운 상태에서는 표적으로 남아야 하지만, 사망이 확정되면 지운다.
	AEnemyController::ForgetActorForAll(GetWorld(), this);
	
	StartDeath(); // 래그돌
}

void AOBCharacterBase::HoldUntilGrounded()
{
	if (!HasAuthority()) return;

	// 바닥이 이미 있으면 스냅만 하고 끝(에디터·비스트리밍 맵의 정상 경로).
	if (TryLandOnGround()) return;

	// 월드 파티션 셀이 아직 안 올라옴. 지금 떨어뜨리면 지형 밑으로 빠진다.
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->SetMovementMode(MOVE_None);   // 복제되므로 클라도 같이 멈춘다
	}

	GroundWaitElapsed = 0.f;
	GetWorldTimerManager().SetTimer(GroundWaitTimer, this, &AOBCharacterBase::PollGround, 0.25f, true);
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
	
	if (CameraBoom)
	{
		const float NewLag = FMath::FInterpTo(CameraBoom->CameraLagSpeed, TargetCameraLagSpeed, DeltaSeconds, CameraLagBlendSpeed);
		CameraBoom->CameraLagSpeed = NewLag;
		if (!FMath::IsNearlyEqual(NewLag, TargetCameraLagSpeed, 0.05f))
		{
			bStillBlending = true;
		}
	}
	
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
	if (AOBPlayerStateBase* OBPlayerState = GetPlayerState<AOBPlayerStateBase>())
	{
		OBPlayerState->InitializePlayerStatsFromData();
		if (const UOBGameDataSubsystem* GameData = UOBGameDataSubsystem::Get())
		{
			if (const FOBPlayerArchetypeRow* Archetype = GameData->FindPlayerArchetype(OBPlayerState->GetPlayerArchetypeId()))
			{
				if (UOBAbilitySet* ArchetypeAbilities = Archetype->DefaultAbilitySet.LoadSynchronous();
					ArchetypeAbilities && ArchetypeAbilities != DefaultAbilitySet)
				{
					ArchetypeAbilities->GiveToAbilitySystem(AbilitySystemComponent, nullptr, this);
				}
			}
		}
	}

	if (PlayerInventoryComponent)
	{
		if (PawnData && PawnData->DefaultBackpackTag.IsValid())
		{
			PlayerInventoryComponent->EquipStartingBackpack(PawnData->DefaultBackpackTag);
		}

		// 로비 로드아웃은 클라가 BeginPlay에서 push한다. 패키징 빌드에서는 폰이 먼저
		// 스폰되므로 여기서 비어 있을 수 있다. 도착 시점에 PS가 다시 부른다.
		FinalizeSpawnLoadout();

		// push가 끝내 안 오면(로드아웃이 빈 신규 플레이어 등) 기본 무기로 진행한다.
		GetWorldTimerManager().SetTimer(
			LoadoutWaitTimer, this, &AOBCharacterBase::FinalizeSpawnLoadoutFallback, LoadoutWaitSeconds, false);
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

void AOBCharacterBase::PollGround()
{
	GroundWaitElapsed += 0.25f;

	if (TryLandOnGround() || GroundWaitElapsed >= MaxGroundWaitSeconds)
	{
		GetWorldTimerManager().ClearTimer(GroundWaitTimer);
		if (UCharacterMovementComponent* Move = GetCharacterMovement())
		{
			Move->SetMovementMode(MOVE_Walking);
		}
	}
}

bool AOBCharacterBase::TryLandOnGround()
{
	UWorld* W = GetWorld();
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!W || !Capsule) return false;

	const FVector Start = GetActorLocation();
	const FVector End   = Start - FVector(0.f, 0.f, GroundTraceDistance);

	// 콜리전이 로드되지 않았으면 여기서 실패한다 = 셀 미도착 판정기.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SpawnGroundTrace), /*bTraceComplex=*/false, this);
	FHitResult Hit;
	if (!W->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params)) return false;

	SetActorLocation(
		Hit.ImpactPoint + FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight() + 5.f),
		/*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
	return true;
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

TArray<FOBItemStack> AOBCharacterBase::GetBagContentsAsStacks()
{
	TArray<FOBItemStack> Stacks;
	if (!PlayerInventoryComponent) return Stacks;

	// 사망 드랍과 같은 소스를 쓴다. 장착 무기를 한쪽만 세면
	// "상자에서 주운 무기를 장착하고 탈출" 시 조용히 증발한다.
	TArray<FInventoryData> Instances;
	PlayerInventoryComponent->GetLootableItemInstances(Instances);

	for (const FInventoryData& Item : Instances)
	{
		if (Item.ItemTag.IsValid() && Item.ItemStack > 0)
		{
			OBItemStacks::Add(Stacks, Item.ItemTag, Item.ItemStack);
		}
	}
	return Stacks;
}

TArray<FOBItemStack> AOBCharacterBase::GetBagGainsSinceSpawn()
{
	TArray<FOBItemStack> Gains = GetBagContentsAsStacks();

	for (const FOBItemStack& Base : SpawnBagSnapshot)
	{
		const int32 Index = Gains.IndexOfByPredicate(
			[&Base](const FOBItemStack& S)
			{
				return S.ItemTag == Base.ItemTag;
			});
		if (Index == INDEX_NONE) continue;

		Gains[Index].Count -= Base.Count;
	}

	// 쓰고 남은 양이 초기 지급보다 적으면 음수가 된다. 그건 획득이 아니다.
	// (탄약 300발 중 100발 쏘고 30발 주웠다 → 순증 0)
	Gains.RemoveAll(
		[](const FOBItemStack& S)
		{
			return S.Count <= 0;
		});
	
	return Gains;
}

TArray<FOBItemStack> AOBCharacterBase::GetExtractionStackGains()
{
	TArray<FOBItemStack> Gains = GetBagGainsSinceSpawn();
	Gains.RemoveAll(
		[](const FOBItemStack& Stack)
		{
			const FOBItemDefinitionRow* Definition = UOBItemRegistry::FindItem(Stack.ItemTag);
			return Definition &&
				(Definition->Category == EOBItemCategory::Weapon ||
				 Definition->Category == EOBItemCategory::Equipment ||
				 Definition->Category == EOBItemCategory::Attachment);
		});
	return Gains;
}

TArray<FInventoryData> AOBCharacterBase::GetLootedUniqueItemInstances()
{
	TArray<FInventoryData> Out;
	if (!PlayerInventoryComponent) return Out;
	TArray<FInventoryData> Items;
	PlayerInventoryComponent->GetLootableItemInstances(Items);
	for (const FInventoryData& Item : Items)
	{
		const FOBItemDefinitionRow* Definition = Item.GetDefinition();
		const bool bPersistent = Definition &&
			(Definition->Category == EOBItemCategory::Weapon ||
			 Definition->Category == EOBItemCategory::Equipment ||
			 Definition->Category == EOBItemCategory::Attachment);
		if (bPersistent && !SpawnInstanceIds.Contains(Item.InstanceId))
		{
			Out.Add(Item);
		}
	}
	return Out;
}

TArray<FInventoryData> AOBCharacterBase::GetReturnedLoadoutItemInstances()
{
	TArray<FInventoryData> Out;
	if (!PlayerInventoryComponent) return Out;
	TArray<FInventoryData> Items;
	PlayerInventoryComponent->GetLootableItemInstances(Items);
	for (const FInventoryData& Item : Items)
	{
		const FOBItemDefinitionRow* Definition = Item.GetDefinition();
		if (SpawnInstanceIds.Contains(Item.InstanceId) && Definition &&
			Definition->Category == EOBItemCategory::Weapon)
		{
			Out.Add(Item);
		}
	}
	return Out;
}

void AOBCharacterBase::ApplyCarryItems()
{
	if (!HasAuthority() || bCarryItemsApplied || !PlayerInventoryComponent) return;

	const AOBPlayerStateBase* PS = GetPlayerState<AOBPlayerStateBase>();
	if (!PS) return;

	const TArray<FOBItemStack>& Carry = PS->GetSelectedCarryItems();
	const TArray<FInventoryData>& CarryInstances = PS->GetSelectedCarryItemInstances();

	// 아직 클라 push가 안 왔을 수 있다. 플래그를 세우지 않고 다음 호출을 기다린다.
	if (Carry.IsEmpty() && CarryInstances.IsEmpty()) return;

	bCarryItemsApplied = true;

	TArray<FOBItemStack> Leftover;
	TArray<FInventoryData> LeftoverInstances;
	for (const FInventoryData& Item : CarryInstances)
	{
		const int32 Added = PlayerInventoryComponent->TryAddItemInstance(Item);
		if (Added < Item.ItemStack)
		{
			FInventoryData Remaining = Item;
			Remaining.ItemStack = Item.ItemStack - Added;
			LeftoverInstances.Add(MoveTemp(Remaining));
		}
	}
	for (const FOBItemStack& Stack : Carry)
	{
		const int32 Added = PlayerInventoryComponent->TryAddItem(Stack.ItemTag, Stack.Count);
		if (Added < Stack.Count)
		{
			Leftover.Emplace(Stack.ItemTag, Stack.Count - Added);
		}
	}

	// 창고에서 이미 빠진 물건이다. 안 되돌리면 조용히 증발한다.
	if (!Leftover.IsEmpty())
	{
		if (AOBPlayerController* PC = Cast<AOBPlayerController>(GetController()))
		{
			PC->Client_ReturnCarryLeftover(Leftover);
		}
	}
	if (!LeftoverInstances.IsEmpty())
	{
		if (AOBPlayerController* PC = Cast<AOBPlayerController>(GetController()))
		{
			PC->Client_ReturnCarryItemInstances(LeftoverInstances);
		}
	}
}

void AOBCharacterBase::FinalizeSpawnLoadout()
{
	TArray<TSubclassOf<AOBWeaponBase>> Loadout;
	if (const AOBPlayerStateBase* PS = GetPlayerState<AOBPlayerStateBase>())
	{
		if (!PS->GetSelectedWeaponInstances().IsEmpty())
		{
			FinalizeSpawnLoadoutInstancesInternal(PS->GetSelectedWeaponInstances());
			return;
		}
		Loadout = PS->GetSelectedWeapons();
	}

	// 아직 push가 안 왔다. 플래그를 세우지 않고 다음 호출을 기다린다.
	if (Loadout.IsEmpty()) return;

	FinalizeSpawnLoadoutInternal(Loadout);
}

void AOBCharacterBase::FinalizeSpawnLoadoutFallback()
{
	if (bSpawnLoadoutApplied) return;

	UE_LOG(LogTemp, Warning,
		TEXT("[Loadout] %s: 로드아웃 push가 %.1f초 안에 안 왔다 → PawnData 기본 무기로 진행."),
		*GetName(), LoadoutWaitSeconds);

	FinalizeSpawnLoadoutInternal(
		PawnData ? PawnData->DefaultWeapons : TArray<TSubclassOf<AOBWeaponBase>>());
}

void AOBCharacterBase::FinalizeSpawnLoadoutInternal(
	const TArray<TSubclassOf<AOBWeaponBase>>& Weapons)
{
	if (!HasAuthority() || bSpawnLoadoutApplied || !PlayerInventoryComponent) return;

	bSpawnLoadoutApplied = true;
	GetWorldTimerManager().ClearTimer(LoadoutWaitTimer);

	for (const TSubclassOf<AOBWeaponBase>& WeaponClass : Weapons)
	{
		if (WeaponClass)
		{
			PlayerInventoryComponent->AddWeapon(WeaponClass);
		}
	}
	CompleteSpawnInventorySetup();
}

void AOBCharacterBase::FinalizeSpawnLoadoutInstancesInternal(
	const TArray<FInventoryData>& Weapons)
{
	if (!HasAuthority() || bSpawnLoadoutApplied || !PlayerInventoryComponent) return;
	bSpawnLoadoutApplied = true;
	GetWorldTimerManager().ClearTimer(LoadoutWaitTimer);
	for (const FInventoryData& Weapon : Weapons)
	{
		PlayerInventoryComponent->AddWeaponInstance(Weapon);
	}
	CompleteSpawnInventorySetup();
}

void AOBCharacterBase::CompleteSpawnInventorySetup()
{
	if (!HasAuthority() || !PlayerInventoryComponent) return;

	if (PawnData)
	{
		for (const TPair<FGameplayTag, int32>& Item : PawnData->StartingItems)
		{
			PlayerInventoryComponent->TryAddItem(Item.Key, Item.Value);
		}
	}

	PlayerInventoryComponent->EquipDefaultSlot();

	// 무기·초기지급이 끝난 이 시점이 정산 기준선이다.
	// 무기 지급이 늦어졌는데 여기서 안 찍으면 무기가 "이번에 얻은 것"이 되어 창고로 복사된다.
	CaptureSpawnInventorySnapshot();

	// 반입분은 기준선 뒤에 넣어야 탈출 시 창고로 되돌아간다.
	ApplyCarryItems();
}

void AOBCharacterBase::CaptureSpawnInventorySnapshot()
{
	SpawnBagSnapshot = GetBagContentsAsStacks();
	SpawnInstanceIds.Reset();
	if (!PlayerInventoryComponent) return;
	TArray<FInventoryData> Items;
	PlayerInventoryComponent->GetLootableItemInstances(Items);
	for (const FInventoryData& Item : Items)
	{
		if (Item.InstanceId.IsValid()) SpawnInstanceIds.Add(Item.InstanceId);
	}
}

void AOBCharacterBase::NotifyHitForRagdoll(FName BoneName, const FVector& HitDirection)
{
	if (!HasAuthority()) return;

	LastHitDirection = HitDirection.GetSafeNormal();
	LastHitBoneName = BoneName;
}
