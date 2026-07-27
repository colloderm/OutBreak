#include "AI/StateTree/Task/TimedMoveToTask.h"

#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FSTTTimedMoveToTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime = 0.0f;
	InstanceData.bMoveRequested = false;
	InstanceData.bUsingTargetActor = IsValid(InstanceData.TargetActor);

	AAIController* AIController = ResolveController(InstanceData);
	if (!IsValid(AIController))
	{
		return EStateTreeRunStatus::Failed;
	}

	if (InstanceData.MoveDuration <= 0.0f)
	{
		AIController->StopMovement();
		return EStateTreeRunStatus::Succeeded;
	}

	FAIMoveRequest MoveRequest;
	MoveRequest
		.SetNavigationFilter(
			InstanceData.NavigationFilter
				? InstanceData.NavigationFilter
				: AIController->GetDefaultNavigationFilterClass())
		.SetAcceptanceRadius(
			FMath::Max(0.0f, InstanceData.AcceptanceRadius))
		.SetUsePathfinding(InstanceData.bUsePathfinding)
		.SetProjectGoalLocation(InstanceData.bProjectGoalLocation)
		.SetAllowPartialPath(InstanceData.bAllowPartialPath)
		.SetCanStrafe(InstanceData.bAllowStrafe)
		.SetReachTestIncludesAgentRadius(
			InstanceData.bReachTestIncludesAgentRadius)
		.SetReachTestIncludesGoalRadius(
			InstanceData.bReachTestIncludesGoalRadius);

	if (InstanceData.bUsingTargetActor)
	{
		MoveRequest.SetGoalActor(InstanceData.TargetActor);
	}
	else
	{
		MoveRequest.SetGoalLocation(InstanceData.Destination);
	}

	const FPathFollowingRequestResult MoveResult =
		AIController->MoveTo(MoveRequest);

	switch (MoveResult.Code)
	{
	case EPathFollowingRequestResult::RequestSuccessful:
		InstanceData.bMoveRequested = true;
		return EStateTreeRunStatus::Running;

	case EPathFollowingRequestResult::AlreadyAtGoal:
		AIController->StopMovement();
		return EStateTreeRunStatus::Succeeded;

	case EPathFollowingRequestResult::Failed:
	default:
		AIController->StopMovement();
		return EStateTreeRunStatus::Failed;
	}
}

EStateTreeRunStatus FSTTTimedMoveToTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIController* AIController = ResolveController(InstanceData);
	if (!IsValid(AIController) || !InstanceData.bMoveRequested)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (InstanceData.bUsingTargetActor &&
		!IsValid(InstanceData.TargetActor))
	{
		StopMovement(InstanceData);
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.ElapsedTime += FMath::Max(0.0f, DeltaTime);
	if (InstanceData.ElapsedTime >=
		FMath::Max(0.0f, InstanceData.MoveDuration))
	{
		StopMovement(InstanceData);
		return EStateTreeRunStatus::Succeeded;
	}

	if (AIController->GetMoveStatus() == EPathFollowingStatus::Idle)
	{
		const UPathFollowingComponent* PathFollowingComponent =
			AIController->GetPathFollowingComponent();
		const bool bReachedGoal =
			IsValid(PathFollowingComponent) &&
			PathFollowingComponent->DidMoveReachGoal();

		StopMovement(InstanceData);
		return bReachedGoal
			? EStateTreeRunStatus::Succeeded
			: EStateTreeRunStatus::Failed;
	}

	return EStateTreeRunStatus::Running;
}

void FSTTTimedMoveToTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	StopMovement(InstanceData);
	FStateTreeAITaskBase::ExitState(Context, Transition);
}

AAIController* FSTTTimedMoveToTask::ResolveController(
	const FInstanceDataType& InstanceData)
{
	const APawn* ControlledPawn = InstanceData.ControlledPawn.Get();
	return IsValid(ControlledPawn)
		? Cast<AAIController>(ControlledPawn->GetController())
		: nullptr;
}

void FSTTTimedMoveToTask::StopMovement(
	FInstanceDataType& InstanceData)
{
	if (AAIController* AIController = ResolveController(InstanceData))
	{
		AIController->StopMovement();
	}

	InstanceData.bMoveRequested = false;
}
