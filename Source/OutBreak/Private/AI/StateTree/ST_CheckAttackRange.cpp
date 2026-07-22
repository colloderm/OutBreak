// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/StateTree/ST_CheckAttackRange.h"


#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "StateTreeExecutionContext.h"

FSTTCheckAttackRange::FSTTCheckAttackRange()
{
	/*
	 * Tick()을 호출받으려면 활성화해야 합니다.
	 */
	bShouldCallTick = true;
}

EStateTreeRunStatus FSTTCheckAttackRange::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData =
		Context.GetInstanceData(*this);

	if (!IsValid(InstanceData.ControlledPawn) ||
		!IsValid(InstanceData.TargetActor))
	{
		return EStateTreeRunStatus::Failed;
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FSTTCheckAttackRange::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	const FInstanceDataType& InstanceData =
		Context.GetInstanceData(*this);

	if (!IsValid(InstanceData.ControlledPawn) ||
		!IsValid(InstanceData.TargetActor))
	{
		return EStateTreeRunStatus::Failed;
	}

	const FVector PawnLocation =
		InstanceData.ControlledPawn->GetActorLocation();

	const FVector TargetLocation =
		InstanceData.TargetActor->GetActorLocation();

	const float DistanceSquared =
		FVector::DistSquared2D(
			PawnLocation,
			TargetLocation);

	const float AttackRangeSquared =
		FMath::Square(InstanceData.AttackRange);

	if (DistanceSquared <= AttackRangeSquared)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}