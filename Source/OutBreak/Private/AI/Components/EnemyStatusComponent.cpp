// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Components/EnemyStatusComponent.h"
#include "AI/EnemyCharacter.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

// Sets default values for this component's properties
UEnemyStatusComponent::UEnemyStatusComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}


// Called when the game starts
void UEnemyStatusComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UEnemyStatusComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	ClearActionStateTimer();
	Super::EndPlay(EndPlayReason);
}

void UEnemyStatusComponent::ApplyActionState(
	const EEnemyActionState NewState)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}

	if (NewState == EEnemyActionState::Dead)
	{
		SetDead();
		return;
	}

	if (NewState == EEnemyActionState::Active)
	{
		ClearActionState(ActionState);
		return;
	}

	if (ActionState == EEnemyActionState::Dead ||
		GetStatePriority(NewState) < GetStatePriority(ActionState))
	{
		return;
	}

	ClearActionStateTimer();
	SetActionStateInternal(NewState);
}

void UEnemyStatusComponent::ApplyTimedActionState(
	const EEnemyActionState NewState,
	const float Duration)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}

	const float ClampedDuration = FMath::Max(0.0f, Duration);
	ApplyActionState(NewState);
	if (ActionState != NewState ||
		NewState == EEnemyActionState::Active ||
		NewState == EEnemyActionState::Dead)
	{
		return;
	}

	TimedActionState = NewState;
	if (ClampedDuration <= 0.0f)
	{
		HandleTimedActionStateExpired();
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		ClearActionStateTimer();
		SetActionStateInternal(EEnemyActionState::Active);
		return;
	}

	TimedActionStateEndTime =
		World->GetTimeSeconds() + ClampedDuration;
	World->GetTimerManager().SetTimer(
		ActionStateTimerHandle,
		this,
		&UEnemyStatusComponent::HandleTimedActionStateExpired,
		ClampedDuration,
		false);
}

void UEnemyStatusComponent::ClearActionState(
	const EEnemyActionState StateToClear)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() ||
		ActionState != StateToClear ||
		ActionState == EEnemyActionState::Dead)
	{
		return;
	}

	ClearActionStateTimer();
	SetActionStateInternal(EEnemyActionState::Active);
}

void UEnemyStatusComponent::ClearTimedActionState()
{
	ClearActionState(ActionState);
}

void UEnemyStatusComponent::SetDead()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}

	ClearActionStateTimer();
	SetActionStateInternal(EEnemyActionState::Dead);
}

void UEnemyStatusComponent::ResetForPool()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}

	ClearActionStateTimer();
	SetActionStateInternal(EEnemyActionState::Active);
}

void UEnemyStatusComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UEnemyStatusComponent, ActionState);
}

void UEnemyStatusComponent::OnRep_ActionState()
{
}

void UEnemyStatusComponent::HandleTimedActionStateExpired()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!IsValid(Owner) || !Owner->HasAuthority() || !IsValid(World))
	{
		return;
	}

	if (ActionState != TimedActionState ||
		ActionState == EEnemyActionState::Dead)
	{
		return;
	}

	const double RemainingTime =
		TimedActionStateEndTime - World->GetTimeSeconds();
	if (RemainingTime > KINDA_SMALL_NUMBER)
	{
		World->GetTimerManager().SetTimer(
			ActionStateTimerHandle,
			this,
			&UEnemyStatusComponent::HandleTimedActionStateExpired,
			static_cast<float>(RemainingTime),
			false);
		return;
	}

	ClearActionStateTimer();
	SetActionStateInternal(EEnemyActionState::Active);
}

void UEnemyStatusComponent::SetActionStateInternal(
	const EEnemyActionState NewState)
{
	if (ActionState == NewState)
	{
		return;
	}

	ActionState = NewState;
	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
}

void UEnemyStatusComponent::ClearActionStateTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ActionStateTimerHandle);
	}
	ActionStateTimerHandle.Invalidate();
	TimedActionState = EEnemyActionState::Active;
	TimedActionStateEndTime = 0.0;
}

int32 UEnemyStatusComponent::GetStatePriority(
	const EEnemyActionState State)
{
	switch (State)
	{
	case EEnemyActionState::Dead:
		return 3;
	case EEnemyActionState::Knockdown:
		return 2;
	case EEnemyActionState::Stunned:
		return 1;
	case EEnemyActionState::Active:
	default:
		return 0;
	}
}
