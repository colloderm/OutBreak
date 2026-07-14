// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/EnemyCharacter.h"

#include "AI/Components/EnemyMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "IAnimationBudgetAllocator.h"
#include "SkeletalMeshComponentBudgeted.h"

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
	PrimaryActorTick.bCanEverTick = false;

	UCapsuleComponent* Capsule =
		GetCapsuleComponent();

	check(Capsule);

	Capsule->SetCanEverAffectNavigation(false);

	USkeletalMeshComponentBudgeted* BudgetedMesh =
		Cast<USkeletalMeshComponentBudgeted>(GetMesh());

	checkf(
		BudgetedMesh,
		TEXT(
			"AEnemyCharacter requires "
			"USkeletalMeshComponentBudgeted as its Mesh component."));

	BudgetedMesh->SetCanEverAffectNavigation(false);

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

	/*
	 * BeginPlay 시점에는 컴포넌트 등록이 끝났으므로
	 * 초기 중요도를 강제로 한 번 적용한다.
	 */
	bHasAppliedBudgetState = false;
	ApplyAnimationBudgetSignificance();
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
	 * 중요도는 외부에서 직접 설정한다.
	 */
	BudgetedMesh->SetAutoCalculateSignificance(false);

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