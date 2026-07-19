// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/EnemyCharacter.h"

#include "AI/Components/EnemyMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "IAnimationBudgetAllocator.h"
#include "SkeletalMeshComponentBudgeted.h"
#include "MotionWarpingComponent.h"
#include "AI/System/ModularSkeletalMeshActor.h"
#include "Engine/DamageEvents.h"
#include "Components/ChildActorComponent.h"
#include "Engine/StaticMeshActor.h"
#include "AI/Components/EnemyStatusComponent.h"


DEFINE_LOG_CATEGORY(LogModularAnimationProxy);

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
	
	CollisionComponent->SetRelativeLocationAndRotation(
		FVector(0.0f, 0.0f, 0.0f),
		FRotator(0.0f, 0.0f, 0.0f));
	
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
	EnemyStatusComponent = CreateDefaultSubobject<UEnemyStatusComponent>(TEXT("EnemyStatusComponent"));
	
	ChildActorComponent = CreateDefaultSubobject<UChildActorComponent>(TEXT("ChildActorComponent"));
	ChildActorComponent->SetupAttachment(GetMesh());
	
	/*
	 * 컴포넌트가 등록되기 전에 자동 등록 설정을 활성화한다.
	 */
	ApplyAnimationBudgetSettings();
}

void AEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

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
	
	if (IsValid(ReactCurveFloat))
	{
		FOnTimelineFloat UpdateDelegate;
		UpdateDelegate.BindUFunction(
			this,
			FName(TEXT("HandleReactTimeline")));
		

		FOnTimelineEvent FinishedDelegate;
		FinishedDelegate.BindUFunction(
			this,
			FName(TEXT("HandleReactTimelineFinished")));
		
		ReactTimeline.AddInterpFloat(ReactCurveFloat, UpdateDelegate);
		ReactTimeline.SetTimelineFinishedFunc(FinishedDelegate);
		
		ReactTimeline.SetLooping(false);
		ReactTimeline.SetPlayRate(1.f);
	}
	
	if (!ensureAlwaysMsgf(
		PM_Head && PM_Torso && PM_Arm_R && PM_Arm_L && PM_Leg_R && PM_Leg_L,
		TEXT("%s::%s: VaultMontage is invalid."),
		*GetClass()->GetName(),
		TEXT(__FUNCTION__)))
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s: Physical Material is not set."), *GetClass()->GetName(), TEXT(__FUNCTION__));
	}
	
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
	
	ReactTimeline.TickTimeline(DeltaSeconds);
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

void AEnemyCharacter::PhysicalMaterialProcess(TWeakObjectPtr<UPhysicalMaterial> PhyMtrl)
{
	if (PhyMtrl == PM_Head)
	{
		GetMesh()->HideBoneByName(FName(TEXT("Head")), PBO_None);
	}
	else if (PhyMtrl == PM_Arm_R)
	{
		MeshPartDestruction(SM_Arm_R, FName(TEXT("upperarm_r")));
	}
	else if (PhyMtrl == PM_Arm_L)
	{
		MeshPartDestruction(SM_Arm_L, FName(TEXT("upperarm_l")));
	}
	else if (PhyMtrl == PM_Leg_R)
	{
		MeshPartDestruction(SM_Leg_R, FName(TEXT("thigh_r")));
	}
	else if (PhyMtrl == PM_Leg_L)
	{
		MeshPartDestruction(SM_Leg_L, FName(TEXT("thigh_l")));
	}
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
	
			FName BoneName = HitResult.BoneName;
			const FVector HitDirection = HitResult.Normal;
			TWeakObjectPtr<UPhysicalMaterial> PhyMtrl = HitResult.PhysMaterial;
			
			if (BoneName == FName(TEXT("pelvis")))
			{
				UE_LOG(LogTemp, Display, TEXT("%s::%s: Hitted Bone is pelvis"), *GetClass()->GetName(), TEXT(__FUNCTION__));
				return ActualDamage;
			}
			if (!bIsHit)
			{
				UE_LOG(LogTemp, Display, TEXT("%s::%s: It's already a hit."), *GetClass()->GetName(), TEXT(__FUNCTION__));
			}
			
			
			PhysicalMaterialProcess(PhyMtrl);
			
			
			
			
	
			CacheBoneName = BoneName;
	
			GetCharacterMovement()->StopMovementImmediately();
	
			GetMesh()->SetAllBodiesBelowSimulatePhysics(BoneName, true, true);
			GetMesh()->AddImpulse(HitDirection.GetSafeNormal() * ReactScale, BoneName, true);
	
			ReactTimeline.PlayFromStart();
	
	
			bIsHit = true;
		}
	}
	
	return ActualDamage;
}

void AEnemyCharacter::MeshPartDestruction(UStaticMesh* MeshAsset, FName BoneName)
{
	if (!IsValid(ChildActorSkeletalMesh))
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s: Child Actor Skeletal Mesh is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	
	if (!IsValid(MeshAsset))
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s: Mesh Asset is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	
	if (BoneName == NAME_None)
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s: BoneName is Name_None."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	
	UWorld* World = GetWorld();
	
	if (!IsValid(World))
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s: World is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	FTransform SpawnTransform = ChildActorSkeletalMesh->GetSocketTransform(BoneName);
	AStaticMeshActor* MeshPart = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), SpawnTransform, SpawnParams);
	UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(MeshPart->GetRootComponent());
	MeshComp->SetMobility(EComponentMobility::Movable);
	MeshComp->SetStaticMesh(MeshAsset);

	MeshComp->SetCollisionProfileName(TEXT("PhysicsActor"));
	MeshComp->SetGenerateOverlapEvents(false);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);

	MeshComp->SetCanEverAffectNavigation(false);
	MeshComp->SetMassOverrideInKg(NAME_None, 300.f);
	MeshComp->SetSimulatePhysics(true);
	MeshComp->WakeAllRigidBodies();
	
	ChildActorSkeletalMesh->HideBoneByName(BoneName, PBO_Term);
	
	USkeletalMeshComponent* CharacterMesh = GetMesh();

	const int32 CharacterBoneIndex =
		CharacterMesh
			? CharacterMesh->GetBoneIndex(BoneName)
			: INDEX_NONE;

	const int32 ChildBoneIndex =
		ChildActorSkeletalMesh
			? ChildActorSkeletalMesh->GetBoneIndex(BoneName)
			: INDEX_NONE;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT(
			"Hide Bone: Name=%s "
			"CharacterIndex=%d ChildIndex=%d "
			"ChildOwner=%s ChildMesh=%s"),
		*BoneName.ToString(),
		CharacterBoneIndex,
		ChildBoneIndex,
		*GetNameSafe(
			ChildActorSkeletalMesh
				? ChildActorSkeletalMesh->GetOwner()
				: nullptr),
		*GetNameSafe(
			ChildActorSkeletalMesh
				? ChildActorSkeletalMesh->GetSkeletalMeshAsset()
				: nullptr));
}

USkeletalMeshComponent* AEnemyCharacter::GetChildActorSkeletalMesh()
{
	if (!IsValid(ChildActorComponent))
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s: Child Actor Component is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return nullptr;
	}
	
	AActor* ChildActor = ChildActorComponent->GetChildActor();
	if (!IsValid(ChildActor))
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s: Child Actor is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return nullptr;
	}
	
	AModularSkeletalMeshActor* MeshOwner = Cast<AModularSkeletalMeshActor>(ChildActor);
	
	check(MeshOwner)
	// {
	// 	UE_LOG(LogTemp, Error, TEXT("%s::%s: MeshOwner is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
	// 	return nullptr;
	// }
	
	return MeshOwner->LeaderHead;
}

void AEnemyCharacter::HandleReactTimeline(float value)
{
	if (CacheBoneName == NAME_None)
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s: Cache Bone Name is \"NAME_None\""), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	GetMesh()->SetAllBodiesBelowPhysicsBlendWeight(CacheBoneName, value);
}

void AEnemyCharacter::HandleReactTimelineFinished()
{
	GetMesh()->SetAllBodiesPhysicsBlendWeight(0.0f, false);
	GetMesh()->SetAllBodiesSimulatePhysics(false);
	bIsHit = false;
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