// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/StateTree/Task/InvestigatingTask.h"

#include "StateTreeExecutionContext.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "StateTreeExecutionContext.h"

FSTTRandomLocationTask::FSTTRandomLocationTask()
{
	bShouldCallTick = true;
}

EStateTreeRunStatus FSTTRandomLocationTask::EnterState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData((*this));
	
	if (!IsValid(InstanceData.ControlledPawn) ||
		!InstanceData.bHasLastPerceptionLocation)
	{
		return EStateTreeRunStatus::Failed;
	}
	
	AAIController* AIController =
		Cast<AAIController>(
			InstanceData.ControlledPawn->GetController());
	
	if (!IsValid(AIController))
	{
		return EStateTreeRunStatus::Failed;
	}
	
	InstanceData.CurrentInvestigationLocation =
		FVector::ZeroVector;
	
	
	InstanceData.bMoveRequested = false;
	InstanceData.CompletedPointCount = 0;
	InstanceData.RetryTimeRemaining = 0.0f;

	return RequestNextInvestigationMove(InstanceData);
}

EStateTreeRunStatus FSTTRandomLocationTask::Tick(FStateTreeExecutionContext& Context, float DeltaTime) const
{
	FInstanceDataType& InstanceData =
		Context.GetInstanceData(*this);
	
	if (!IsValid(InstanceData.ControlledPawn) ||
		!InstanceData.bHasLastPerceptionLocation)
	{
		return EStateTreeRunStatus::Failed;
	}
	
	AAIController* AIController =
		Cast<AAIController>(
			InstanceData.ControlledPawn->GetController());

	
	const FVector LastPerceptionLocation = InstanceData.LastPerceptionLocation;
	
	if (!IsValid(AIController))
	{
		return EStateTreeRunStatus::Failed;
	}
	
	if (InstanceData.CompletedPointCount >=
		InstanceData.MaxInvestigationPoints)
	{
		return EStateTreeRunStatus::Succeeded;
	}
	
	/*
	 * 경로 생성 실패 시 매 프레임 다시 요청하지 않는다.
	 */
	if (InstanceData.RetryTimeRemaining > 0.0f)
	{
		InstanceData.RetryTimeRemaining -= DeltaTime;
		return EStateTreeRunStatus::Running;
	}
	
	const EPathFollowingStatus::Type MoveStatus =
		AIController->GetMoveStatus();

	switch (MoveStatus)
	{
	case EPathFollowingStatus::Moving:
	case EPathFollowingStatus::Waiting:
	case EPathFollowingStatus::Paused:
		return EStateTreeRunStatus::Running;

	case EPathFollowingStatus::Idle:
	default:
		break;
	}
	
	/*
	 * 이전에 요청한 이동이 Idle 상태가 되었다면
	 * 성공 또는 실패 여부와 관계없이 다음 조사 지점으로 넘어간다.
	 *
	 * 조사 행동에서는 특정 지점 도달 실패가 전체 State 실패로
	 * 이어지기보다 다른 지점을 선택하는 편이 자연스럽다.
	 */
	if (InstanceData.bMoveRequested)
	{
		InstanceData.bMoveRequested = false;
		++InstanceData.CompletedPointCount;
	}

	if (InstanceData.CompletedPointCount >=
		InstanceData.MaxInvestigationPoints)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	return RequestNextInvestigationMove(InstanceData);
}

void FSTTRandomLocationTask::ExitState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData =
		Context.GetInstanceData(*this);
	
	if (!IsValid(InstanceData.ControlledPawn))
	{
		return;
	}
	

	if (AAIController* AIController =
		Cast<AAIController>(
			InstanceData.ControlledPawn->GetController()))
	{
		AIController->StopMovement();
	}
}

EStateTreeRunStatus FSTTRandomLocationTask::RequestNextInvestigationMove(FInstanceDataType& InstanceData) const
{
	APawn* ControlledPawn = InstanceData.ControlledPawn;

	if (!IsValid(ControlledPawn))
	{
		return EStateTreeRunStatus::Failed;
	}
	
	AAIController* AIController =
		Cast<AAIController>(ControlledPawn->GetController());
	
	if (!IsValid(AIController))
	{
		return EStateTreeRunStatus::Failed;
	}
	
	UNavigationSystemV1* NavigationSystem =
		FNavigationSystem::GetCurrent<UNavigationSystemV1>(
			ControlledPawn->GetWorld());
	
	if (!IsValid(NavigationSystem))
	{
		return EStateTreeRunStatus::Failed;
	}
	
	/*
	 * 같은 점이나 이동 불가능한 점이 연속으로 선택되는 경우를
	 * 피하기 위해 한 번에 몇 차례까지만 재시도한다.
	 */
	constexpr int32 MaxSelectionAttempts = 4;

	for (int32 Attempt = 0;
			Attempt < MaxSelectionAttempts;
			++Attempt)
	{
		FNavLocation RandomNavLocation;

		const bool bFoundLocation =
			NavigationSystem->GetRandomReachablePointInRadius(
				InstanceData.LastPerceptionLocation,
				InstanceData.InvestigationRadius,
				RandomNavLocation);

		if (!bFoundLocation)
		{
			continue;
		}

		const EPathFollowingRequestResult::Type MoveResult =
			AIController->MoveToLocation(
				RandomNavLocation.Location,
				InstanceData.AcceptanceRadius,
				true,  // bStopOnOverlap
				true,  // bUsePathfinding
				false, // 이미 Navigation 위치이므로 재투영 불필요
				false, // bCanStrafe
				InstanceData.NavigationFilter,
				false  // bAllowPartialPath
			);

		switch (MoveResult)
		{
		case EPathFollowingRequestResult::RequestSuccessful:
			InstanceData.CurrentInvestigationLocation =
				RandomNavLocation.Location;

			InstanceData.bMoveRequested = true;

			return EStateTreeRunStatus::Running;

		case EPathFollowingRequestResult::AlreadyAtGoal:
			++InstanceData.CompletedPointCount;

			if (InstanceData.CompletedPointCount >=
				InstanceData.MaxInvestigationPoints)
			{
				return EStateTreeRunStatus::Succeeded;
			}

			continue;

		case EPathFollowingRequestResult::Failed:
		default:
			continue;
		}
	}
	
	/*
	 * 일시적인 Navigation 실패는 State 전체 실패로 처리하지 않고
	 * 짧은 간격 후 다시 시도한다.
	 */
	InstanceData.bMoveRequested = false;
	InstanceData.RetryTimeRemaining =
		InstanceData.RetryInterval;

	return EStateTreeRunStatus::Running;
}
