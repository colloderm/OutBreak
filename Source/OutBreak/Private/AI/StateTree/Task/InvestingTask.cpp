// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/StateTree/Task/InvestingTask.h"

#include "StateTreeExecutionContext.h"

FSTTInvestingTask::FSTTInvestingTask()
{
	bShouldCallTick = true;
}

EStateTreeRunStatus FSTTInvestingTask::EnterState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData((*this));
	
	if (!IsValid(InstanceData.ControlledPawn) ||
		InstanceData.LastPerceptionLocation == FVector::ZeroVector)
	{
		return EStateTreeRunStatus::Failed;
	}
	
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FSTTInvestingTask::Tick(FStateTreeExecutionContext& Context, float DeltaTime) const
{
	const FInstanceDataType& InstanceData =
		Context.GetInstanceData(*this);
	
	if (!IsValid(InstanceData.ControlledPawn) ||
		InstanceData.LastPerceptionLocation == FVector::ZeroVector)
	{
		return EStateTreeRunStatus::Failed;
	}
	
	
	
	const FVector LastPerceptionLocation = InstanceData.LastPerceptionLocation;
	
	if (InstanceData.InvestingLocation.IsSet())
	{
		
	}
	
	
	
	return EStateTreeRunStatus::Running;
}
