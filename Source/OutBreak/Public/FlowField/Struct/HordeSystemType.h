// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AnimToTextureInstancePlaybackHelpers.h"
#include "HordeSystemType.generated.h"

/**
 * 
 */

using HordeAgentID = uint32;

enum class EHordeAnimationDataIndex : uint8
{
	Forward_Run,
	Forward_Run_Mirror,
	Hit_RightShoulder,
	Hit_LeftShoulder,
};

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
	FAnimToTextureAutoPlayData AnimToTextureAutoPlayData = FAnimToTextureAutoPlayData(); 
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

	/* 겹침이 많아 질수록 Spreation 연산이 폭증 함.*/
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
	TArray<int32>						InstanceIds;
	TArray<FAnimToTextureAutoPlayData>	VAT_Data;
	TArray<TObjectPtr<AActor>>			PawnProxies;
	
	int32 Size() const
	{
		return PawnProxies.Num();
	}

	void Initialize(const int32 Capacity)
	{
		InstanceIds.Reserve(Capacity);
		VAT_Data.Reserve(Capacity);
		PawnProxies.Reserve(Capacity);
	}
	
	bool IsValid() const
	{
		const int32 AgentCount = PawnProxies.Num();
		
		return VAT_Data.Num() == AgentCount
			&& InstanceIds.Num() == AgentCount;
	}
	
	int32 Add(AActor* Pawn, int32 InstanceId, FAnimToTextureAutoPlayData AnimToTextureAutoPlayData)
	{
		VAT_Data.Add(AnimToTextureAutoPlayData);
		InstanceIds.Add(InstanceId);
		return PawnProxies.Add(Pawn);
	}
	
	void RemoveAtSwap(const int32 PackedIndex)
	{
		check(IsValid());
		check(VAT_Data.IsValidIndex(PackedIndex));
		check(InstanceIds.IsValidIndex(PackedIndex));
		check(PawnProxies.IsValidIndex(PackedIndex));
			
		VAT_Data.RemoveAtSwap(PackedIndex, 1, EAllowShrinking::No);
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
	
	UPROPERTY()
	FAnimToTextureAutoPlayData		AnimToTextureAutoPlayData;
	
	/* Horde Proxy Storage Info */
	UPROPERTY()
	FIntVector2						PoseIndex = FIntVector2::ZeroValue;
};

using HordeNetworkFormat = FHordeNetworkFormat;
