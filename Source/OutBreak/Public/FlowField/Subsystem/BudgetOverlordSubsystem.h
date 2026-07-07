// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FlowField/Struct/HordeSystemType.h"
#include "BudgetOverlordSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class OUTBREAK_API UBudgetOverlordSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	int32 GetIndexByActor(const AActor* Actor) const;
	
	
	UFUNCTION(BlueprintCallable)
	FHordeAgentHandle RegisterAgent(
		const FTransform& inTransform,
		float inMoveSpeed = 300.f,
		float MaxHealth = 100.f,
		float HealthPercent = 1.f
	);
	
	void UnregisterAgent(int32 Index);
	
protected:
	
	void DispatchPayload(const FHordeNetworkFormat& Payload);
	
	friend class UHordeNetworkSubsystem;

private:
	int32 CacheTestIndex = 0;
	void BuildPacket();
	
	FHordeAgentHandle AllocateAgentHandle();
	
	// PackedIndex -> Stable Handle
	UPROPERTY(Transient)
	TArray<FHordeAgentHandle> PackedIndexToHandle;

	// AgentID -> 현재 PackedIndex
	UPROPERTY(Transient)
	TArray<int32> AgentIDToPackedIndex;

	// AgentID별 현재 세대
	UPROPERTY(Transient)
	TArray<uint32> AgentGenerations;

	// 제거되어 재사용 가능한 AgentID
	UPROPERTY(Transient)
	TArray<uint32> FreeAgentIDs;
	
	/* Status System에서 Damage 적용 병렬 처리를 위한 Index By Actor Cache */
	TMap<TWeakObjectPtr<AActor>, int32> IndexByActor;
	
	UPROPERTY(Transient)
	TObjectPtr<class UHordeMovementSubsystem> MovementSubsystem;
	
	UPROPERTY(Transient)
	TObjectPtr<class UHordeProxySubsystem> ProxySubsystem;
	
	UPROPERTY(Transient)
	TObjectPtr<class UHordeStatusSubsystem> StatusSubsystem;
	
	UPROPERTY(Transient)
	TObjectPtr<class UHordeNetworkSubsystem> NetworkSubsystem;
	
	void InitializeViceroy(int32 Capacity);
};
