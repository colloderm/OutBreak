// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/EnemyCharacter.h"

#include "Engine/DamageEvents.h"

#include "SkeletalMeshComponentBudgeted.h"
#include "IAnimationBudgetAllocator.h"

#include "AI/Data/EnemyAsset.h"

#include "Components/ChildActorComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Controller.h"
#include "MotionWarpingComponent.h"
#include "AI/EnemyController.h"

#include "AI/Components/EnemyMovementComponent.h"
#include "AI/Components/EnemyStatusComponent.h"
#include "AI/Components/EnemyPhysicalComponent.h"
#include "Perception/AISense_Damage.h"
#include "Sound/SoundCue.h"


#include "AI/System/ModularSkeletalMeshActor.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Item/Loot/OBLootContainer.h"
#include "Net/UnrealNetwork.h"


DEFINE_LOG_CATEGORY(LogModularAnimationProxy);

FGenericTeamId AEnemyCharacter::GetGenericTeamId() const
{
	return TeamId;
}

AEnemyCharacter::AEnemyCharacter(
	const FObjectInitializer& ObjectInitializer)
	: Super(
		ObjectInitializer
		.SetDefaultSubobjectClass<
			USkeletalMeshComponentBudgeted>(
				ACharacter::MeshComponentName)
		.SetDefaultSubobjectClass<
			UEnemyMovementComponent>(
				ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	

	InitializeComponents();
	/*
	 * 컴포넌트가 등록되기 전에 자동 등록 설정을 활성화한다.
	 */
	ApplyAnimationBudgetSettings();
}

void AEnemyCharacter::InitializeComponents()
{
		UCapsuleComponent* Capsule =
		GetCapsuleComponent();
	
	check(Capsule);
	
	 /* ———————————————————————————————————Set Primitive Target————————————————————————————————————————— */
		UPrimitiveComponent* CollisionComponent = Capsule;
	/* ———————————————————————————————————————————————————————————————————————————————————————————————— */
	

	/* ——————————————————————————————————Default Engine Collision Setting—————————————————————————————— */
	// using Event Enable
	CollisionComponent->SetGenerateOverlapEvents(true);
	// Collision Setting
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetCollisionObjectType(ECC_Pawn);

	// Trace Channel
	CollisionComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	
	CollisionComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore); /* Camera Probe */
	CollisionComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore); /* Weapon */

	// Object Channel
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	CollisionComponent->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Block);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Destructible, ECR_Block);
	/* ———————————————————————————————————————————————————————————————————————————————————————————————— */
	Capsule->SetCanEverAffectNavigation(false);

	USkeletalMeshComponentBudgeted* BudgetedMesh =
		Cast<USkeletalMeshComponentBudgeted>(GetMesh());

	checkf(
		BudgetedMesh,
		TEXT(
			"AEnemyCharacter requires "
			"USkeletalMeshComponentBudgeted as its Mesh component."));

	
	
	/* ———————————————————————————————————Set Primitive Target————————————————————————————————————————— */
	CollisionComponent = BudgetedMesh;
	/* ———————————————————————————————————————————————————————————————————————————————————————————————— */
	
	CollisionComponent->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -90.f), FRotator(0.0f, 0.0f, -90.0f));

	/* ——————————————————————————————————Default Engine Collision Setting—————————————————————————————— */
	// using Event Enable
	CollisionComponent->SetGenerateOverlapEvents(true);
	// Collision Setting
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionComponent->SetCollisionObjectType(ECC_Pawn);

	// Trace Channel
	CollisionComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	
	CollisionComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore); /* Camera Probe */
	CollisionComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block); /* Weapon */

	// Object Channel
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Block);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Destructible, ECR_Block);
	/* ———————————————————————————————————————————————————————————————————————————————————————————————— */
	
	BudgetedMesh->SetCanEverAffectNavigation(false);
	
	MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));
	PhysicalComponent = CreateDefaultSubobject<UEnemyPhysicalComponent>(TEXT("PhysicalComponent"));
	
	ChildActorComponent = CreateDefaultSubobject<UChildActorComponent>(TEXT("ChildActorComponent"));
	ChildActorComponent->SetupAttachment(GetMesh());
	
	
}

void AEnemyCharacter::InitializeAsset()
{
	if(!ensureAlwaysMsgf(IsValid(EnemyAsset),
		TEXT("%s::%s: Enemy Asset is invalid."),
		*GetClass()->GetName(),
		TEXT(__FUNCTION__)))
	{
		return;
	}
}

void AEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	InitializeAsset();
	
	USkeletalMeshComponentBudgeted* BudgetedMesh =
		Cast<USkeletalMeshComponentBudgeted>(GetMesh());

	if (!IsValid(BudgetedMesh))
	{
		UE_LOG(
			LogModularAnimationProxy,
			Error,
			TEXT(
				"%s::%s: Mesh is not a valid "
				"USkeletalMeshComponentBudgeted."),
			*GetName(),
			TEXT(__FUNCTION__));

		return;
	}

	ApplyAnimationBudgetSettings();

	BudgetedMesh->OnReduceWork().BindUObject(
		this,
		&AEnemyCharacter::HandleReducedWorkChanged);
	
	ChildActorSkeletalMesh = GetChildActorSkeletalMesh();
	
	USoundCue* CryingSound = EnemyAsset->GetSoundAssets()->ZombieCryingSound;
	if (IsValid(CryingSound))
	{
		CryingSoundComponent = UGameplayStatics::SpawnSoundAttached(CryingSound, RootComponent);
	}
	
	/*
	 * BeginPlay 시점에는 컴포넌트 등록이 끝났으므로
	 * 초기 중요도를 강제로 한 번 적용한다.
	 */
	bHasAppliedBudgetState = false;
	ApplyAnimationBudgetSignificance();
}

void AEnemyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
}

void AEnemyCharacter::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	USkeletalMeshComponentBudgeted* BudgetedMesh =
		Cast<USkeletalMeshComponentBudgeted>(GetMesh());

	if (IsValid(BudgetedMesh))
	{
		BudgetedMesh->OnReduceWork().Unbind();
	}

	bHasAppliedBudgetState = false;
	bReducedAnimationWork = false;

	Super::EndPlay(EndPlayReason);
}


float AEnemyCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator,
                                  AActor* DamageCauser)
{
	if (bIsDead)
	{
		return 0.0f;
	}

	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	FVector HitLocation = GetActorLocation();
	if (ActualDamage > 0.0f && IsValid(PhysicalComponent))
	{
		if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
		{
			const FPointDamageEvent& PointDamageEvent =
				static_cast<const FPointDamageEvent&>(DamageEvent);
			const FHitResult& HitResult = PointDamageEvent.HitInfo;
			if (HitResult.bBlockingHit)
			{
				HitLocation = HitResult.ImpactPoint;
			}

			PhysicalComponent->ActionPhysical(HitResult, ActualDamage);
		}
		else
		{
			PhysicalComponent->ApplyDamage(ActualDamage);
		}
	}

	AActor* DamageInstigatorActor =
		IsValid(EventInstigator)
			? EventInstigator->GetPawn()
			: nullptr;
	if (!IsValid(DamageInstigatorActor))
	{
		DamageInstigatorActor = DamageCauser;
	}

	if (ActualDamage > 0.0f &&
		IsValid(DamageInstigatorActor) &&
		DamageInstigatorActor != this)
	{
		UAISense_Damage::ReportDamageEvent(
			this,
			this,
			DamageInstigatorActor,
			ActualDamage,
			DamageInstigatorActor->GetActorLocation(),
			HitLocation);
	}

	return ActualDamage;
}

ELocomotionWalkRunState AEnemyCharacter::GetLocomotionWalkRunState() const
{
	UEnemyMovementComponent* MovementComponent = Cast<UEnemyMovementComponent>(GetMovementComponent());
	if (IsValid(MovementComponent))
	{
		return MovementComponent->GetLocomotionState();
	}
	return ELocomotionWalkRunState::Dead;
}

EEnemyMissingArmState AEnemyCharacter::GetMissingArmState() const
{
	if (IsValid(PhysicalComponent))
	{
		return PhysicalComponent->GetMissingArmState();
	}

	return EEnemyMissingArmState::None;
}

USkeletalMeshComponent*
AEnemyCharacter::GetChildActorSkeletalMesh()
{
	if (!ensureAlwaysMsgf(
		IsValid(ChildActorComponent),
		TEXT("%s::%s: ChildActorComponent is invalid."),
		*GetClass()->GetName(),
		TEXT(__FUNCTION__)))
	{
		return nullptr;
	}

	AModularSkeletalMeshActor* MeshOwner =
		Cast<AModularSkeletalMeshActor>(
			ChildActorComponent->GetChildActor());

	if (!ensureAlwaysMsgf(
		IsValid(MeshOwner),
		TEXT("%s::%s: Child actor must be "
			 "AModularSkeletalMeshActor."),
		*GetClass()->GetName(),
		TEXT(__FUNCTION__)))
	{
		return nullptr;
	}

	if (!ensureAlwaysMsgf(
		IsValid(MeshOwner->LeaderHead),
		TEXT("%s::%s: LeaderHead is invalid."),
		*GetClass()->GetName(),
		TEXT(__FUNCTION__)))
	{
		return nullptr;
	}

	return MeshOwner->LeaderHead;
}

void AEnemyCharacter::StopCharacterMovement()
{
	GetMovementComponent()->StopMovementImmediately();
}

void AEnemyCharacter::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEnemyCharacter, bIsDead);
	DOREPLIFETIME(AEnemyCharacter, LastHitDirection);
	DOREPLIFETIME(AEnemyCharacter, LastHitBoneName);
}

void AEnemyCharacter::OnRep_IsDead()
{
	// 프로퍼티는 RepNotify 이전에 전부 적용된다. 즉 여기서는
	// LastHitDirection/BoneName이 이미 서버 값이다.
	if (bIsDead)
	{
		PlayDeathCosmetics();
	}
}

void AEnemyCharacter::PlayDeathCosmetics()
{
	if (UAudioComponent* CryingAudio = CryingSoundComponent.Get())
	{
		CryingSoundComponent.Reset();
		CryingAudio->Stop();
	}

	if (UEnemyMovementComponent* EnemyMovementComponent =
		Cast<UEnemyMovementComponent>(GetMovementComponent()))
	{
		// bIsDead가 이미 true라 여기서 Dead()로 되돌아오지 않는다.
		EnemyMovementComponent->SetLocomotationState(
			ELocomotionWalkRunState::Dead);
	}

	StopCharacterMovement();

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetSimulatePhysics(true);

		// 맞은 방향으로 쓰러진다. 방향이 없으면(환경 피해 등) 그냥 무너진다.
		if (!LastHitDirection.IsNearlyZero() && RagdollImpulseStrength > 0.f)
		{
			const FName Bone = (LastHitBoneName != NAME_None)
				? LastHitBoneName
				: MeshComp->GetBoneName(0);

			MeshComp->AddImpulse(
				LastHitDirection * RagdollImpulseStrength, Bone, /*bVelChange=*/false);
		}
	}
}

void AEnemyCharacter::Dead()
{
	if (bIsDead)
	{
		return;
	}

	// 사망 선언은 서버만. 클라는 OnRep_IsDead로 따라온다.
	if (!HasAuthority())
	{
		return;
	}

	bIsDead = true;

	// 서버 로컬 연출(리슨 서버 화면·데디의 물리 상태).
	PlayDeathCosmetics();

	AEnemyController* EnemyController =
		Cast<AEnemyController>(GetController());
	if (IsValid(EnemyController))
	{
		EnemyController->Dead(DeathCleanupDelay);
	}

	// 처치 보상. Destroy 분기보다 먼저 굴려야 시체가 사라지기 전에 드랍이 남는다.
	if (!DeathLootRow.IsNull() && LootContainerClass)
	{
		const float HalfHeight = GetCapsuleComponent()
			? GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
			: 0.f;
		const FVector DropLoc =
			GetActorLocation() - FVector(0.f, 0.f, HalfHeight);

		AOBLootContainer::SpawnFromTable(GetWorld(), LootContainerClass,
			FTransform(FRotator::ZeroRotator, DropLoc), DeathLootRow);
	}

	if (DeathCleanupDelay <= 0.0f)
	{
		Destroy();
		return;
	}

	SetLifeSpan(DeathCleanupDelay);
}

void AEnemyCharacter::SetAnimationSignificance(
	const float InSignificance)
{
	const float NewSignificance =
		FMath::Clamp(InSignificance, 0.0f, 1.0f);

	if (FMath::IsNearlyEqual(
		CurrentAnimationSignificance,
		NewSignificance))
	{
		return;
	}

	CurrentAnimationSignificance =
		NewSignificance;

	ApplyAnimationBudgetSignificance();
}

FString AEnemyCharacter::
GetAnimationBudgetDebugSummary() const
{
#if UE_BUILD_SHIPPING
	return FString();
#else
	USkeletalMeshComponentBudgeted* BudgetedMesh =
		Cast<USkeletalMeshComponentBudgeted>(GetMesh());

	return FString::Printf(
		TEXT(
			"Actor=%s\n"
			"MeshComponent=%s\n"
			"SkeletalMesh=%s\n"
			"AnimClass=%s\n"
			"Significance=%.3f\n"
			"ReducedWork=%s\n"
			"Registered=%s"),
		*GetName(),
		*GetNameSafe(BudgetedMesh),
		BudgetedMesh
			? *GetNameSafe(
				BudgetedMesh->GetSkeletalMeshAsset())
			: TEXT("None"),
		BudgetedMesh
			? *GetNameSafe(
				BudgetedMesh->GetAnimClass())
			: TEXT("None"),
		CurrentAnimationSignificance,
		bReducedAnimationWork
			? TEXT("true")
			: TEXT("false"),
		BudgetedMesh && BudgetedMesh->IsRegistered()
			? TEXT("true")
			: TEXT("false"));
#endif
}

void AEnemyCharacter::ApplyAnimationBudgetSettings()
{
	USkeletalMeshComponentBudgeted* BudgetedMesh =
		Cast<USkeletalMeshComponentBudgeted>(GetMesh());

	if (!IsValid(BudgetedMesh))
	{
		return;
	}

	BudgetedMesh->SetAutoRegisterWithBudgetAllocator(true);

	/*
	 * 중요도는 외부에서 직접 설정한다. -> true : 자동 설정
	 */
	BudgetedMesh->SetAutoCalculateSignificance(true);

	/*
	 * 최근 렌더링 여부를 예산 판단에 반영한다.
	 */
	BudgetedMesh->SetShouldUseActorRenderedFlag(true);
}

bool AEnemyCharacter::
EnsureAnimationBudgetRegistration() const
{
	if (!HasActorBegunPlay())
	{
		return false;
	}

	USkeletalMeshComponentBudgeted* BudgetedMesh =
		Cast<USkeletalMeshComponentBudgeted>(GetMesh());

	if (!IsValid(BudgetedMesh) ||
		!BudgetedMesh->IsRegistered())
	{
		return false;
	}

	UWorld* World = BudgetedMesh->GetWorld();

	if (!IsValid(World))
	{
		return false;
	}

	IAnimationBudgetAllocator* Allocator =
		IAnimationBudgetAllocator::Get(World);

	if (Allocator == nullptr)
	{
		return false;
	}

	/*
	 * 자동 등록이 설정되어 있지만,
	 * 중요도 적용 전에 등록 상태를 확실히 보장한다.
	 */
	Allocator->RegisterComponent(BudgetedMesh);

	return true;
}

void AEnemyCharacter::
ApplyAnimationBudgetSignificance()
{
	if (bHasAppliedBudgetState &&
		FMath::IsNearlyEqual(
			LastAppliedAnimationSignificance,
			CurrentAnimationSignificance) &&
		bLastAppliedTickEvenIfNotRendered ==
			bTickEvenIfNotRendered)
	{
		return;
	}

	if (!EnsureAnimationBudgetRegistration())
	{
		return;
	}

	USkeletalMeshComponentBudgeted* BudgetedMesh =
		CastChecked<USkeletalMeshComponentBudgeted>(
			GetMesh());

	BudgetedMesh->SetComponentSignificance(
		CurrentAnimationSignificance,
		false,                   // bNeverSkip
		bTickEvenIfNotRendered,  // bTickEvenIfNotRendered
		true,                    // bAllowReducedWork
		false);                  // bForceInterpolate

	bHasAppliedBudgetState = true;

	LastAppliedAnimationSignificance =
		CurrentAnimationSignificance;

	bLastAppliedTickEvenIfNotRendered =
		bTickEvenIfNotRendered;
}

void AEnemyCharacter::HandleReducedWorkChanged(
	USkeletalMeshComponentBudgeted* InComponent,
	const bool bInReducedWork)
{
	USkeletalMeshComponentBudgeted* BudgetedMesh =
		Cast<USkeletalMeshComponentBudgeted>(GetMesh());

	if (InComponent != BudgetedMesh)
	{
		return;
	}

	if (bReducedAnimationWork == bInReducedWork)
	{
		return;
	}

	bReducedAnimationWork = bInReducedWork;

	OnReducedAnimationWorkChanged.Broadcast(
		bReducedAnimationWork);
}

void AEnemyCharacter::NotifyHitForRagdoll(FName BoneName, const FVector& HitDirection)
{
	if (!HasAuthority()) return;

	LastHitDirection = HitDirection.GetSafeNormal();
	LastHitBoneName = BoneName;
}

void AEnemyCharacter::Multicast_PlayMontage_Implementation(UAnimMontage* Montage, const float PlayRate, const FName StartSection)
{
	// 서버는 StateTree가 이미 재생했다. 다시 틀면 몽타주가 처음부터 되감긴다.
	if (HasAuthority() || !Montage) return;

	PlayAnimMontage(Montage, PlayRate, StartSection);
}
