// Copyright Epic Games, Inc. All Rights Reserved.

#include "FlowField/ModularSkeletalMeshProxyActor.h"

#include "IAnimationBudgetAllocator.h"
#include "SkeletalMeshComponentBudgeted.h"

DEFINE_LOG_CATEGORY(LogModularAnimationProxy);

AModularSkeletalMeshProxyActor::AModularSkeletalMeshProxyActor()
{
	PrimaryActorTick.bCanEverTick = false;

	ABAHead = CreateDefaultSubobject<USkeletalMeshComponentBudgeted>(TEXT("ABA Head"));
	SetRootComponent(ABAHead);

	ApplyAnimationBudgetSettings();
}

void AModularSkeletalMeshProxyActor::BeginPlay()
{
	Super::BeginPlay();

	ApplyAnimationBudgetSettings();

	if (IsValid(ABAHead))
	{
		ABAHead->OnReduceWork().BindUObject(this, &AModularSkeletalMeshProxyActor::HandleReducedWorkChanged);
		ApplyAnimationBudgetSignificance();
	}
}

void AModularSkeletalMeshProxyActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(ABAHead))
	{
		ABAHead->OnReduceWork().Unbind();
	}

	Super::EndPlay(EndPlayReason);
}

void AModularSkeletalMeshProxyActor::SetAnimationSignificance(const float InSignificance)
{
	CurrentAnimationSignificance = FMath::Clamp(InSignificance, 0.0f, 1.0f);
	ApplyAnimationBudgetSignificance();
}


FString AModularSkeletalMeshProxyActor::GetAnimationBudgetDebugSummary() const
{
#if UE_BUILD_SHIPPING
	return FString();
#else
	return FString::Printf(
		TEXT("Actor=%s\nHead=%s\nHeadMesh=%s\nHeadAnimClass=%s\nSignificance=%.3f\nReducedWork=%s"),
		*GetName(),
		*GetNameSafe(ABAHead),
		ABAHead ? *GetNameSafe(ABAHead->GetSkeletalMeshAsset()) : TEXT("None"),
		ABAHead ? *GetNameSafe(ABAHead->GetAnimClass()) : TEXT("None"),
		CurrentAnimationSignificance,
		bReducedAnimationWork ? TEXT("true") : TEXT("false"));
#endif
}

void AModularSkeletalMeshProxyActor::ApplyAnimationBudgetSettings()
{
	if (!IsValid(ABAHead))
	{
		return;
	}

	ABAHead->SetAutoRegisterWithBudgetAllocator(true);
	ABAHead->SetAutoCalculateSignificance(false);
	ABAHead->SetShouldUseActorRenderedFlag(true);
}

bool AModularSkeletalMeshProxyActor::EnsureAnimationBudgetRegistration() const
{
	if (!HasActorBegunPlay() || !IsValid(ABAHead) || !ABAHead->IsRegistered())
	{
		return false;
	}

	UWorld* World = ABAHead->GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	IAnimationBudgetAllocator* Allocator = IAnimationBudgetAllocator::Get(World);
	if (Allocator == nullptr)
	{
		return false;
	}

	Allocator->RegisterComponent(ABAHead);
	return true;
}

void AModularSkeletalMeshProxyActor::ApplyAnimationBudgetSignificance()
{
	if (bHasAppliedBudgetState
		&& FMath::IsNearlyEqual(LastAppliedAnimationSignificance, CurrentAnimationSignificance)
		&& bLastAppliedTickEvenIfNotRendered == bTickEvenIfNotRendered)
	{
		return;
	}

	if (!EnsureAnimationBudgetRegistration())
	{
		return;
	}

	ABAHead->SetComponentSignificance(
		CurrentAnimationSignificance,
		false,
		bTickEvenIfNotRendered,
		true,
		false);

	bHasAppliedBudgetState = true;
	LastAppliedAnimationSignificance = CurrentAnimationSignificance;
	bLastAppliedTickEvenIfNotRendered = bTickEvenIfNotRendered;
}

void AModularSkeletalMeshProxyActor::HandleReducedWorkChanged(
	USkeletalMeshComponentBudgeted* InComponent,
	const bool bInReducedWork)
{
	if (InComponent != ABAHead)
	{
		return;
	}

	if (bReducedAnimationWork == bInReducedWork)
	{
		return;
	}

	bReducedAnimationWork = bInReducedWork;
	OnReducedAnimationWorkChanged.Broadcast(bReducedAnimationWork);
}
