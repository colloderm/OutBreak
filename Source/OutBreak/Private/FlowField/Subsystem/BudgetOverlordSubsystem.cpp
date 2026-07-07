// Fill out your copyright notice in the Description page of Project Settings.


#include "FlowField/Subsystem/BudgetOverlordSubsystem.h"

#include "FlowField/Settings/FlowFieldSettings.h"

// Subsystem
#include "FlowField/Subsystem/HordeMovementSubsystem.h"
#include "FlowField/Subsystem/HordeProxySubsystem.h"
#include "FlowField/Subsystem/HordeStatusSubsystem.h"


void UBudgetOverlordSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	MovementSubsystem = Collection.InitializeDependency<UHordeMovementSubsystem>();
	ProxySubsystem = Collection.InitializeDependency<UHordeProxySubsystem>();
	StatusSubsystem = Collection.InitializeDependency<UHordeStatusSubsystem>();
	
	const UFlowFieldSettings* Settings = GetDefault<UFlowFieldSettings>();
	InitializeViceroy(Settings->GetMaxAgentCount());
}

void UBudgetOverlordSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	
	ProxySubsystem->CreateProxyHost();
}

void UBudgetOverlordSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	
	/* Proxy는 Movement Subsystem의 값을 가져가 병렬로 처리하기에 우선 순위에 배치해야함.*/
	MovementSubsystem->ProcessSystem(DeltaTime);
	StatusSubsystem->ProcessSystem(DeltaTime);
	
	/* 유사 Rendering 단계. */
	ProxySubsystem->ProcessSystem(DeltaTime);
	
}

TStatId UBudgetOverlordSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBudgetOverlordSubsystem, STATGROUP_Tickables);
}

int32 UBudgetOverlordSubsystem::GetIndexByActor(
	const AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		return INDEX_NONE;
	}

	const int32* FoundIndex = IndexByActor.Find(Actor);

	return FoundIndex
		? *FoundIndex
		: INDEX_NONE;
}

void UBudgetOverlordSubsystem::RegisterAgent(
	FTransform inTransform,
	float inMoveSpeed,
	float MaxHealth,
	float HealthPercent
)
{
	MovementSubsystem->Register(inTransform, inMoveSpeed);
	ProxyRegisterResult ProxyResult = ProxySubsystem->Register(inTransform);
	IndexByActor.FindOrAdd(ProxyResult.Actor, ProxyResult.Index);
	StatusSubsystem->Register(MaxHealth, HealthPercent);
	
	/*
	 * NetworkSubsystem->AgentRegistered()
	 * Agent가 등록됨을 모든 Client에 전파
	 */
}

void UBudgetOverlordSubsystem::UnregisterAgent(int32 Index)
{
	MovementSubsystem->Unregister(Index);
	ProxySubsystem->Unregister(Index);
	StatusSubsystem->Unregister(Index);
	
	/*
	 * NetworkSubsystem->AgentUnegistered()
	 * Agent가 등록 해제됨을 모든 Client에 전파
	 */
}

void UBudgetOverlordSubsystem::InitializeViceroy(int32 Capacity)
{
	check(MovementSubsystem);
	check(Capacity > 0);
	
	MovementSubsystem->InitializeStorage(Capacity);
	ProxySubsystem->InitializeStorage(Capacity);
	StatusSubsystem->InitializeStorage(Capacity);
}
