// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 
 */

using HordeAgentID = int32;

struct HordeDamageEvent
{
	TWeakPtr<APawn> DamagedPawn;
	double Damage;
};


struct HordeAgentHandle
{
	uint32 AgentID = MAX_uint32;
	uint32 Generation = 0;
	
	bool IsValid() const
	{
		return AgentID != MAX_uint32;
	}
	
	bool operator==(const HordeAgentHandle& Other) const
	{
		return AgentID == Other.AgentID
			&& Generation == Other.Generation;
	}
};

struct HordeRemoveResult
{
	int32 RemovedIndex = INDEX_NONE;
	int32 LastIndex = INDEX_NONE;
	
	HordeAgentHandle RemovedAgent;
	HordeAgentHandle MovedAgent;
	
	bool bMovedLastAgent = false;
};

struct HordeMovementStorage
{
	TArray<FTransform>				Transforms;
	TArray<float>					MoveSpeeds;

	TArray<FVector>					Velocities;
	TArray<FVector>					CachedFlowDirections;
	TArray<uint8>					MovementStates;
	TArray<uint8>					TraversalStates;
	TArray<uint8>					PriorityTiers;

	int32 Size() const
	{
		return Transforms.Num();
	}
	
	void Initialize(const int32 Capacity)
	{
		Transforms.Reserve(Capacity);
		Velocities.Reserve(Capacity);
		CachedFlowDirections.Reserve(Capacity);
		MoveSpeeds.Reserve(Capacity);
		MovementStates.Reserve(Capacity);
		TraversalStates.Reserve(Capacity);
		PriorityTiers.Reserve(Capacity);
	}
	
	bool IsValid() const
	{
		const int32 AgentCount = Transforms.Num();

		return Velocities.Num() == AgentCount
			&& MoveSpeeds.Num() == AgentCount
			&& CachedFlowDirections.Num() == AgentCount
			&& MovementStates.Num() == AgentCount
			&& TraversalStates.Num() == AgentCount
			&& PriorityTiers.Num() == AgentCount;
	}
	
	int32 Add(FTransform inTransform, float MoveSpeed)
	{
		Transforms.Add(inTransform);
		Velocities.Add(FVector::ZeroVector);
		CachedFlowDirections.Add(FVector::ZeroVector);
		MoveSpeeds.Add(MoveSpeed);
		MovementStates.Add(0);
		TraversalStates.Add(0);
		return PriorityTiers.Add(0);
	}
	
	void RemoveAtSwap(const int32 PackedIndex)
	{
		check(IsValid());
		check(Transforms.IsValidIndex(PackedIndex));
		
		Transforms.RemoveAtSwap(PackedIndex, 1, EAllowShrinking::No);
		Velocities.RemoveAtSwap(PackedIndex, 1, EAllowShrinking::No);
		CachedFlowDirections.RemoveAtSwap(PackedIndex, 1, EAllowShrinking::No);
		MoveSpeeds.RemoveAtSwap(PackedIndex, 1, EAllowShrinking::No);
		MovementStates.RemoveAtSwap(PackedIndex, 1, EAllowShrinking::No);
		TraversalStates.RemoveAtSwap(PackedIndex, 1, EAllowShrinking::No);
		PriorityTiers.RemoveAtSwap(PackedIndex, 1, EAllowShrinking::No);
	}
	
};

struct HordeAgentType
{
	TArray<float>					FlowQueryAges;
	TArray<float>					NetworkUpdateAges;
				
	TArray<uint32>					AgentIDs;
	TArray<uint16>					HealthValues;
	TArray<uint16>					FlowRevisions;
	
	int32 Size() const
	{
		return FlowQueryAges.Num();
	}

	void Initialize(const int32 Capacity)
	{
		FlowQueryAges.Reserve(Capacity);
		NetworkUpdateAges.Reserve(Capacity);

		AgentIDs.Reserve(Capacity);
		HealthValues.Reserve(Capacity);
		FlowRevisions.Reserve(Capacity);
	}
	
	bool IsValid() const
	{
		const int32 AgentCount = FlowQueryAges.Num();
		
		return NetworkUpdateAges.Num() == AgentCount
			&& AgentIDs.Num() == AgentCount
			&& HealthValues.Num() == AgentCount
			&& FlowRevisions.Num() == AgentCount;
	}
	
	void RemoveAtSwap(const int32 PackedIndex)
	{
		check(IsValid());
		check(AgentIDs.IsValidIndex(PackedIndex));
		
		FlowQueryAges.RemoveAtSwap(PackedIndex, 1, EAllowShrinking::No);
		NetworkUpdateAges.RemoveAtSwap(PackedIndex, 1, EAllowShrinking::No);
		AgentIDs.RemoveAtSwap(PackedIndex, 1, EAllowShrinking::No);
		HealthValues.RemoveAtSwap(PackedIndex, 1, EAllowShrinking::No);
		FlowRevisions.RemoveAtSwap(PackedIndex, 1, EAllowShrinking::No);
	}
};


struct HordeProxyStorage
{
	/* 
	 * Horde Proxy System using VAT(Vertex Animation Texture)
	 * FInterVector2 : X = Animation Start Frame, Y = Animation End Frame 
	 * 
	 */
	TArray<FIntVector2>				PoseIndices;
	TArray<int32>					InstanceIds;
	TArray<TObjectPtr<AActor>>		PawnProxies;
	
	int32 Size() const
	{
		return PawnProxies.Num();
	}

	void Initialize(const int32 Capacity)
	{
		InstanceIds.Reserve(Capacity);
		PoseIndices.Reserve(Capacity);
		PawnProxies.Reserve(Capacity);
		
		// check(World)
		//
		// FActorSpawnParameters SpawnParameters;
		// SpawnParameters.Name = TEXT("HordeProxy");
		// SpawnParameters.SpawnCollisionHandlingOverride =
		// 	ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		//
		// for (int32 i = 0 ; i < Capacity ; i++)
		// {
		// 	APawn* SpawnActor = World->SpawnActor<APawn>
		// 	(
		// 		APawn::StaticClass(),
		// 		FTransform::Identity,
		// 		SpawnParameters
		// 	);
		// 	PawnProxies.Add(SpawnActor);
		// }
	}
	
	bool IsValid() const
	{
		const int32 AgentCount = PawnProxies.Num();
		
		return PoseIndices.Num() == AgentCount;
	}
	
	int32 Add(AActor* Pawn, int32 InstanceId)
	{
		PoseIndices.Add(FIntVector2::ZeroValue);
		InstanceIds.Add(InstanceId);
		return PawnProxies.Add(Pawn);
	}
	
	void RemoveAtSwap(const int32 PackedIndex)
	{
		check(IsValid());
		check(PoseIndices.IsValidIndex(PackedIndex));
		
		PoseIndices.RemoveAtSwap(PackedIndex, 1, EAllowShrinking::No);
		InstanceIds.RemoveAtSwap(PackedIndex, 1, EAllowShrinking::No);
		PawnProxies.RemoveAtSwap(PackedIndex, 1, EAllowShrinking::No);
	}
};

struct HordeNetworkFormat
{
	/* Handle */
	HordeAgentHandle				Handle;
	
	/* Movement Storage Info */
	FTransform						Transforms;
	float							MoveSpeeds;
	FVector							Velocities;
	FVector							CachedFlowDirections;
	uint8							MovementStates;
	uint8							TraversalStates;
	uint8							PriorityTiers;
	
	/* Horde Proxy Storage Info */
	FIntVector2						PoseIndex;
	int32							InstanceId;
};