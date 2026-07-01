// Fill out your copyright notice in the Description page of Project Settings.


#include "FlowField/Subsystem/BudgetOverlordSubsystem.h"

#include "FlowField/Settings/FlowFieldSettings.h"

// Subsystem
#include "FlowField/Subsystem/HordeMovementSubsystem.h"
#include "FlowField/Subsystem/HordeProxySubsystem.h"


void UBudgetOverlordSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	MovementSubsystem = Collection.InitializeDependency<UHordeMovementSubsystem>();
	ProxySubsystem = Collection.InitializeDependency<UHordeProxySubsystem>();
	
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
	
	MovementSubsystem->ProcessSystem(DeltaTime);
	ProxySubsystem->ProcessSystem(DeltaTime);
}

TStatId UBudgetOverlordSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBudgetOverlordSubsystem, STATGROUP_Tickables);
}

void UBudgetOverlordSubsystem::RegisterAgent(
	FTransform inTransform,
	float inMoveSpeed
)
{
	MovementSubsystem->Register(inTransform, inMoveSpeed);
	ProxySubsystem->Register(inTransform);
}

void UBudgetOverlordSubsystem::InitializeViceroy(int32 Capacity)
{
	check(MovementSubsystem);
	check(Capacity > 0);
	
	AgentStorage.Initialize(Capacity);
	MovementSubsystem->InitializeStorage(Capacity);
	ProxySubsystem->InitializeStorage(Capacity);
}
