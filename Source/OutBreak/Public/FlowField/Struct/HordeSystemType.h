// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "HordeSystemType.generated.h"

/**
 * 
 */

using HordeAgentID = uint32;

struct FHordeSeparationGridEntry
{
	int32 CellX = 0;
	int32 CellY = 0;
	int32 AgentIndex = INDEX_NONE;
};

struct FHordeSeparationCellRange
{
	int32 CellX = 0;
	int32 CellY = 0;
	int32 StartIndex = 0;
	int32 Count = 0;
};


UENUM()
enum class EHordeNetworkOperation : uint8
{
	Register,
	Update,
	Unregister
};

struct ProxyRegisterResult
{
	AActor* Actor = nullptr;
	int32 ProxyStorageIndex = INDEX_NONE;
	int32 InstanceIndex = INDEX_NONE;
	bool bSucceeded = false;
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

FORCEINLINE uint32 GetTypeHash(const FHordeAgentHandle& Handle)
{
	return HashCombine(
		GetTypeHash(Handle.AgentID),
		GetTypeHash(Handle.Generation));
}

struct HordeDamageEvent
{
	FHordeAgentHandle Handle;
	TWeakObjectPtr<AActor> DamagedActor;
	double Damage = 0.0;
};

struct HordeRemoveResult
{
	int32 RemovedPackedIndex = INDEX_NONE;
	int32 PreviousLastIndex = INDEX_NONE;

	bool bMovedLastAgent = false;

	HordeAgentHandle RemovedHandle;
	HordeAgentHandle MovedHandle;

	TWeakObjectPtr<AActor> RemovedActor;
	TWeakObjectPtr<AActor> MovedActor;

	int32 RemovedInstanceIndex = INDEX_NONE;
	int32 MovedInstanceIndex = INDEX_NONE;
};

struct HordeMovementStorage
{
	TArray<FTransform>				Transforms;
	TArray<float>					MoveSpeeds;

	TArray<FVector>					Velocities;
	TArray<FVector>					CachedFlowDirections;
	TArray<FVector>					SeparationDirections;
	TArray<uint8>					FlowQueryFailureCounts;
	TArray<uint8>					MovementStates;
	TArray<uint8>					TraversalStates;
	TArray<uint8>					PriorityTiers;

	TArray<FVector>					PositionSnapshot;
	TArray<FVector>					MoveOffsetScratch;
	TArray<FVector>					FinalMoveOffsetScratch;
	TArray<FHordeSeparationGridEntry> SeparationGridEntries;
	TArray<FHordeSeparationCellRange> SeparationCellRanges;

	int32 Size() const
	{
		return Transforms.Num();
	}
	
	void Initialize(const int32 Capacity)
	{
		Transforms.Reserve(Capacity);
		Velocities.Reserve(Capacity);
		CachedFlowDirections.Reserve(Capacity);
		SeparationDirections.Reserve(Capacity);
		FlowQueryFailureCounts.Reserve(Capacity);
		MoveSpeeds.Reserve(Capacity);
		MovementStates.Reserve(Capacity);
		TraversalStates.Reserve(Capacity);
		PriorityTiers.Reserve(Capacity);
		PositionSnapshot.Reserve(Capacity);
		MoveOffsetScratch.Reserve(Capacity);
		FinalMoveOffsetScratch.Reserve(Capacity);
		SeparationGridEntries.Reserve(Capacity);
		SeparationCellRanges.Reserve(Capacity);
	}
	
	bool IsValid() const
	{
		const int32 AgentCount = Transforms.Num();

		return Velocities.Num() == AgentCount
			&& MoveSpeeds.Num() == AgentCount
			&& CachedFlowDirections.Num() == AgentCount
			&& SeparationDirections.Num() == AgentCount
			&& FlowQueryFailureCounts.Num() == AgentCount
			&& MovementStates.Num() == AgentCount
			&& TraversalStates.Num() == AgentCount
			&& PriorityTiers.Num() == AgentCount;
	}
	
	int32 Add(FTransform inTransform, float MoveSpeed)
	{
		Transforms.Add(inTransform);
		Velocities.Add(FVector::ZeroVector);
		CachedFlowDirections.Add(FVector::ZeroVector);
		SeparationDirections.Add(FVector::ZeroVector);
		FlowQueryFailureCounts.Add(0);
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
		SeparationDirections.RemoveAtSwap(PackedIndex, 1, EAllowShrinking::No);
		FlowQueryFailureCounts.RemoveAtSwap(PackedIndex, 1, EAllowShrinking::No);
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
		CurrentHealths.Reserve(Capacity);
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
		
		return CurrentHealths.Num() == AgentCount;
	}
	
	void RemoveAtSwap(const int32 PackedIndex)
	{
		check(IsValid());
		check(MaxHealths.IsValidIndex(PackedIndex));
		check(CurrentHealths.IsValidIndex(PackedIndex));
		
		MaxHealths.RemoveAtSwap(PackedIndex, 1, EAllowShrinking::No);
		CurrentHealths.RemoveAtSwap(PackedIndex, 1, EAllowShrinking::No);

		check(IsValid());
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
		
		return PoseIndices.Num() == AgentCount
			&& InstanceIds.Num() == AgentCount;
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
	check(InstanceIds.IsValidIndex(PackedIndex));
	check(PawnProxies.IsValidIndex(PackedIndex));
		
	PoseIndices.RemoveAtSwap(PackedIndex, 1, EAllowShrinking::No);
	InstanceIds.RemoveAtSwap(PackedIndex, 1, EAllowShrinking::No);
	PawnProxies.RemoveAtSwap(PackedIndex, 1, EAllowShrinking::No);

	check(IsValid());
	}
};

USTRUCT()
struct FHordeNetworkFormat
{
	GENERATED_BODY()
	
	UPROPERTY()
	EHordeNetworkOperation Operation =
		EHordeNetworkOperation::Update;
	
	/* Handle */
	UPROPERTY()
	FHordeAgentHandle				Handle;
	
	/* Movement Storage Info */
	UPROPERTY()
	FTransform						Transforms = FTransform::Identity;
	
	UPROPERTY()
	float							MoveSpeed = 0.0f;

	UPROPERTY()
	float							MaxHealth = 0.0f;

	UPROPERTY()
	float							CurrentHealth = 0.0f;
	
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
};

using HordeNetworkFormat = FHordeNetworkFormat;
