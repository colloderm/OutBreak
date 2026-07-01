// Copyright Epic Games, Inc. All Rights Reserved.

#include "FlowField/Subsystem/FlowFieldSubsystem.h"

#include "EngineUtils.h"
#include "FlowField/FlowFieldRecastNavMesh.h"
#include "GameFramework/Actor.h"
#include "FlowField/FlowFieldAgentPawn.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY(LogFlowField);

namespace
{
	TAutoConsoleVariable<int32> CVarFlowFieldNetDiagnostics(
		TEXT("OutBreak.FlowField.NetDiagnostics"),
		0,
		TEXT("Enables throttled diagnostics for Flow Field goal and movement queries."),
		ECVF_Default);

	float ComputeDensity2D(
		const FVector& PositionA,
		const FVector& PositionB,
		const float InfluenceRadius)
	{
		if (InfluenceRadius <= KINDA_SMALL_NUMBER)
		{
			return 0.0f;
		}

		FVector Delta = PositionA - PositionB;
		Delta.Z = 0.0f;
		const float Distance = Delta.Size();
		return FMath::Clamp(1.0f - Distance / InfluenceRadius, 0.0f, 1.0f);
	}
}

bool IsFlowFieldNetworkDiagnosticsEnabled()
{
	return CVarFlowFieldNetDiagnostics.GetValueOnGameThread() != 0;
}

void UFlowFieldSubsystem::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
}

TStatId UFlowFieldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFlowFieldSubsystem, STATGROUP_Tickables);
}

bool UFlowFieldSubsystem::SetGoal(const FVector& GoalWorldLocation)
{
	if (bHasFlowField
		&& GoalWorldLocation.Equals(CurrentGoal)
		&& CachedNavMesh.IsValid()
		&& CachedNavMesh->HasFlowField())
	{
		return true;
	}

	bHasFlowField = false;

	AFlowFieldRecastNavMesh* NavMesh = FindNavMesh();
	if (NavMesh == nullptr || !NavMesh->BuildFlowField(GoalWorldLocation))
	{
		if (IsFlowFieldNetworkDiagnosticsEnabled())
		{
			const UWorld* World = GetWorld();
			const double Now = World != nullptr ? World->GetTimeSeconds() : FPlatformTime::Seconds();
			if (LastSetGoalFailureLogTime < 0.0 || Now - LastSetGoalFailureLogTime >= 5.0)
			{
				LastSetGoalFailureLogTime = Now;
			}
		}
		return false;
	}

	CachedNavMesh = NavMesh;
	CurrentGoal = GoalWorldLocation;
	bHasFlowField = true;
	LastSetGoalFailureLogTime = -1.0;
	return true;
}

bool UFlowFieldSubsystem::QueryDirection(const FVector& WorldLocation, FVector& OutDirection) const
{
	OutDirection = FVector::ZeroVector;

	const AFlowFieldRecastNavMesh* NavMesh = FindNavMesh();
	return NavMesh != nullptr
		&& NavMesh->HasFlowField()
		&& NavMesh->QueryDirection(WorldLocation, OutDirection);
}

bool UFlowFieldSubsystem::QueryConstrainedMove(
	const FVector& WorldLocation,
	const float MaxTravelDistance,
	FVector& OutMoveOffset) const
{
	OutMoveOffset = FVector::ZeroVector;

	const AFlowFieldRecastNavMesh* NavMesh = FindNavMesh();
	return NavMesh != nullptr
		&& NavMesh->HasFlowField()
		&& NavMesh->QueryConstrainedMove(WorldLocation, MaxTravelDistance, OutMoveOffset);
}

bool UFlowFieldSubsystem::QueryNodeRef(const FVector& WorldLocation, NavNodeRef& OutNodeRef) const
{
	OutNodeRef = INVALID_NAVNODEREF;

	const AFlowFieldRecastNavMesh* NavMesh = FindNavMesh();
	return NavMesh != nullptr && NavMesh->QueryNodeRef(WorldLocation, OutNodeRef);
}

bool UFlowFieldSubsystem::QueryNavLink(const FVector& WorldLocation, FFlowFieldNavLinkInfo& OutNavLinksRef) const
{
	OutNavLinksRef = FFlowFieldNavLinkInfo();

	const AFlowFieldRecastNavMesh* NavMesh = FindNavMesh();
	return NavMesh != nullptr
		&& NavMesh->HasFlowField()
		&& NavMesh->QueryNavLink(WorldLocation, OutNavLinksRef);
}

bool UFlowFieldSubsystem::HasFlowField() const
{
	const AFlowFieldRecastNavMesh* NavMesh = FindNavMesh();
	return NavMesh != nullptr && NavMesh->HasFlowField();
}

AFlowFieldRecastNavMesh* UFlowFieldSubsystem::FindNavMesh() const
{
	if (AFlowFieldRecastNavMesh* NavMesh = CachedNavMesh.Get())
	{
		return NavMesh;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	for (TActorIterator<AFlowFieldRecastNavMesh> It(World); It; ++It)
	{
		return *It;
	}

	return nullptr;
}
