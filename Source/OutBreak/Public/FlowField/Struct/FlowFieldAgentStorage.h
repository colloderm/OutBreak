// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 
 */
struct FFlowFieldAgentStorage
{
	TArray<FVector>					Positions;
	TArray<FVector>					Velocities;
	TArray<FVector>					CachedFlowDirections;
				
	TArray<float>					FlowQueryAges;
	TArray<float>					NetworkUpdateAges;
				
	TArray<uint32>					AgentIDs;
	TArray<uint16>					HealthValues;
	TArray<uint16>					FlowRevisions;
				
	TArray<uint8>					MovementStates;
	TArray<uint8>					TraversalStates;
	TArray<uint8>					PriorityTiers;
	
	TArray<TWeakObjectPtr<APawn>>	PawnProxies;
	
	
	
	void Reserve(const int32 Capacity)
	{
		Positions.Reserve(Capacity);
		Velocities.Reserve(Capacity);
		CachedFlowDirections.Reserve(Capacity);

		FlowQueryAges.Reserve(Capacity);
		NetworkUpdateAges.Reserve(Capacity);

		AgentIDs.Reserve(Capacity);
		HealthValues.Reserve(Capacity);
		FlowRevisions.Reserve(Capacity);

		MovementStates.Reserve(Capacity);
		TraversalStates.Reserve(Capacity);
		PriorityTiers.Reserve(Capacity);

		PawnProxies.Reserve(Capacity);
	}
	
	void Initialize(const int32 Capacity)
	{
		Reserve(Capacity);

		Positions.Init(FVector::ZeroVector, Capacity);
		Velocities.Init(FVector::ZeroVector, Capacity);
		CachedFlowDirections.Init(FVector::ZeroVector, Capacity);

		FlowQueryAges.Init(0.0f, Capacity);
		NetworkUpdateAges.Init(0.0f, Capacity);

		AgentIDs.Init(0, Capacity);
		HealthValues.Init(0, Capacity);
		FlowRevisions.Init(0, Capacity);

		MovementStates.Init(0, Capacity);
		TraversalStates.Init(0, Capacity);
		PriorityTiers.Init(0, Capacity);

		PawnProxies.Init(nullptr, Capacity);
	}
};
