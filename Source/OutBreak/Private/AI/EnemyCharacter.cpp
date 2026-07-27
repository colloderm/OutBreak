// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/EnemyCharacter.h"

#include "Engine/DamageEvents.h"

#include "SkeletalMeshComponentBudgeted.h"
#include "IAnimationBudgetAllocator.h"

#include "AI/Data/EnemyAsset.h"

#include "Components/ChildActorComponent.h"
#include "Components/CapsuleComponent.h"
#include "MotionWarpingComponent.h"
#include "AI/EnemyController.h"

#include "AI/Components/EnemyMovementComponent.h"
#include "AI/Components/EnemyStatusComponent.h"
#include "AI/Components/EnemyPhysicalComponent.h"


#include "AI/System/ModularSkeletalMeshActor.h"



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
	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& PointEvent =
			static_cast<const FPointDamageEvent&>(DamageEvent);

	
		if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
		{
			const FPointDamageEvent& PointDamageEvent =
				static_cast<const FPointDamageEvent&>(DamageEvent);
			
			FHitResult HitResult= PointDamageEvent.HitInfo;
			
			PhysicalComponent->ActionPhysical(HitResult, DamageAmount);
		}
	}
	
	return ActualDamage;
}

ELocomotionWalkRunState AEnemyCharacter::GetLocomotionWalkRunState() const
{
	return PhysicalComponent->EvaluateLocomotionState();
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

void AEnemyCharacter::Dead()
{
	AEnemyController* EnemyContoller = Cast<AEnemyController>(GetController());
	if (IsValid(EnemyContoller))
	{
		EnemyContoller->Dead();
	}
	GetMesh()->SetSimulatePhysics(true);
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