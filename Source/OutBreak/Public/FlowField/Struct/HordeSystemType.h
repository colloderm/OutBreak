// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "HordeSystemType.generated.h"

/**
 * 
 */

using HordeAgentID = int32;


struct ProxyRegisterResult
{
	AActor* Actor = nullptr;
	int32 Index = INDEX_NONE;
};

struct HordeDamageEvent
{
	int32 StatusIndex = INDEX_NONE;
	TWeakObjectPtr<AActor> DamagedActor;
	double Damage;
};


USTRUCT(Blueprintable)
struct FHordeAgentHandle
{
	GENERATED_BODY()
	
	UPROPERTY()
	uint32 AgentID = MAX_uint32;
	
	UPROPERTY()
	uint32 Generation = 0;
	
	bool IsValid() const
	{
		return AgentID != MAX_uint32;
	}
	
	bool operator==(const FHordeAgentHandle& Other) const
	{
		return AgentID == Other.AgentID
			&& Generation == Other.Generation;
	}
};

using HordeAgentHandle = FHordeAgentHandle;

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

struct HordeStatusStorage
{
	TArray<float>					MaxHealths;
	TArray<float>					CurrentHealths;
	
	int32 Size() const
	{
		return MaxHealths.Num();
	}

	void Initialize(const int32 Capacity)
	{
		MaxHealths.Reserve(Capacity);
	}
	
	/* Percent는 MaxHealth에 대한 현재 체력 비중 입니다.*/
	int32 Add(float inMaxHealth, float Pecsent = 1.0)
	{
		const float CurrentHP = inMaxHealth * Pecsent;
		CurrentHealths.Add(CurrentHP);
		return MaxHealths.Add(inMaxHealth);
	}
	
	bool IsValid() const
	{
		const int32 AgentCount = MaxHealths.Num();
		
		return static_cast<bool>(AgentCount);
	}
	
	void RemoveAtSwap(const int32 PackedIndex)
	{
		check(IsValid());
		check(MaxHealths.IsValidIndex(PackedIndex));
		
		MaxHealths.RemoveAtSwap(PackedIndex, 1, EAllowShrinking::No);
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

USTRUCT()
struct FHordeNetworkFormat
{
	GENERATED_BODY()
	
	/* Handle */
	UPROPERTY()
	FHordeAgentHandle				Handle;
	
	/* Movement Storage Info */
	UPROPERTY()
	FTransform						Transforms = FTransform::Identity;
	
	UPROPERTY()
	float							MoveSpeed = 0.0f;
	
	UPROPERTY()
	FVector							Velocities = FVector::ZeroVector;
	
	UPROPERTY()
	FVector							CachedFlowDirections = FVector::ZeroVector;
	
	UPROPERTY()
	uint8							MovementStates = 0;
	
	UPROPERTY()
	uint8							TraversalStates = 0;
	
	UPROPERTY()
	uint8							PriorityTiers = 0;
	
	/* Horde Proxy Storage Info */
	UPROPERTY()
	FIntVector2						PoseIndex = FIntVector2::ZeroValue;
	
	UPROPERTY()
	int32							InstanceId = INDEX_NONE;
};

using HordeNetworkFormat = FHordeNetworkFormat;
