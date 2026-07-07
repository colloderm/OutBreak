// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FlowField/Struct/HordeSystemType.h"
#include "BudgetOverlordSubsystem.generated.h"

class UHordeMovementSubsystem;
class UHordeProxySubsystem;
class UHordeStatusSubsystem;
class UHordeNetworkSubsystem;

DECLARE_LOG_CATEGORY_EXTERN(LogHordeLifecycle, Log, All);

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
	bool TryGetHandleByActor(
		const AActor* Actor,
		FHordeAgentHandle& OutHandle) const;

	bool TryResolvePackedIndex(
		const FHordeAgentHandle& Handle,
		int32& OutPackedIndex) const;

	FHordeAgentHandle GetHandleByPackedIndex(
		int32 PackedIndex) const;
	
	
	FORCEINLINE UHordeMovementSubsystem* GetMovementSubsystem() { return MovementSubsystem; }
	FORCEINLINE UHordeProxySubsystem* GetProxySubsystem() { return ProxySubsystem; }
	FORCEINLINE UHordeStatusSubsystem* GetStatusSubsystem() { return StatusSubsystem; };
	FORCEINLINE UHordeNetworkSubsystem* GetNetworkSubsystem() { return NetworkSubsystem; }
	
	
	UFUNCTION(BlueprintCallable)
	FHordeAgentHandle RegisterAgent(
		const FTransform& inTransform,
		float inMoveSpeed = 300.f,
		float MaxHealth = 100.f,
		float HealthPercent = 1.f
	);
	
	bool UnregisterAgent(
		const FHordeAgentHandle& Handle);

	bool UnregisterAgent(
		const AActor* Actor);

protected:
	
	void DispatchPayload(const FHordeNetworkFormat& Payload);
	
	friend class UHordeNetworkSubsystem;

private:
	int32 CacheTestIndex = 0;
	void BuildPacket();
	
	FHordeAgentHandle AllocateAgentHandle();
	void RollbackAgentHandleAllocation(
		const FHordeAgentHandle& Handle);
	void ReleaseAgentHandle(
		const FHordeAgentHandle& Handle);
	bool UnregisterAgentByPackedIndex(
		int32 PackedIndex,
		bool bQueueNetworkPayload = true);
	bool RegisterAgentWithHandle(
		const FHordeNetworkFormat& Payload);
	void ApplyRegisterPayload(
		const FHordeNetworkFormat& Payload);
	void ApplyUpdatePayload(
		const FHordeNetworkFormat& Payload);
	void ApplyUnregisterPayload(
		const FHordeNetworkFormat& Payload);
	void ValidateAgentRegistry() const;
	int32 GetIndexByActor(const AActor* Actor) const;
	
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
	TObjectPtr<UHordeMovementSubsystem> MovementSubsystem;
	
	UPROPERTY(Transient)
	TObjectPtr<UHordeProxySubsystem> ProxySubsystem;
	
	UPROPERTY(Transient)
	TObjectPtr<UHordeStatusSubsystem> StatusSubsystem;
	
	UPROPERTY(Transient)
	TObjectPtr<UHordeNetworkSubsystem> NetworkSubsystem;
	
	void InitializeViceroy(int32 Capacity);
};
