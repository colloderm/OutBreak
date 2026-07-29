// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/StateTree/Task/RandomLocationGen.h"

#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"
#include "StateTreeExecutionContext.h"

FRandomLocationGen::FRandomLocationGen()
{
	// 위치는 EnterState에서 한 번만 생성하므로 Tick이 필요하지 않습니다.
	bShouldCallTick = false;
	
}

EStateTreeRunStatus FRandomLocationGen::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData =
		Context.GetInstanceData(*this);

	InstanceData.RandomLocation = FVector::ZeroVector;

	APawn* ControlledPawn = InstanceData.ControlledPawn.Get();

	if (!IsValid(ControlledPawn))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Generate Random Reachable Location: ControlledPawn is invalid."));

		return EStateTreeRunStatus::Failed;
	}

	if (InstanceData.SearchRadius <= 0.0f)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Generate Random Reachable Location: SearchRadius must be greater than zero. Pawn=%s"),
			*GetNameSafe(ControlledPawn));

		return EStateTreeRunStatus::Failed;
	}

	UNavigationSystemV1* NavigationSystem =
		FNavigationSystem::GetCurrent<UNavigationSystemV1>(
			ControlledPawn->GetWorld());

	if (!IsValid(NavigationSystem))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Generate Random Reachable Location: NavigationSystem is invalid. Pawn=%s"),
			*GetNameSafe(ControlledPawn));

		return EStateTreeRunStatus::Failed;
	}

	const FVector Origin = ControlledPawn->GetNavAgentLocation();
	ANavigationData* NavigationData =
		NavigationSystem->GetNavDataForProps(
			ControlledPawn->GetNavAgentPropertiesRef(),
			Origin);

	if (!IsValid(NavigationData))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Generate Random Reachable Location: No matching NavigationData was found. Pawn=%s"),
			*GetNameSafe(ControlledPawn));

		return EStateTreeRunStatus::Failed;
	}

	FNavLocation RandomNavLocation;
	const bool bFoundLocation =
		NavigationSystem->GetRandomReachablePointInRadius(
			Origin,
			InstanceData.SearchRadius,
			RandomNavLocation,
			NavigationData);

	if (!bFoundLocation)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"Generate Random Reachable Location: No reachable point found. Pawn=%s, Radius=%.2f"),
			*GetNameSafe(ControlledPawn),
			InstanceData.SearchRadius);

		return EStateTreeRunStatus::Failed;
	}

	InstanceData.RandomLocation = RandomNavLocation.Location;

	return EStateTreeRunStatus::Succeeded;
}
