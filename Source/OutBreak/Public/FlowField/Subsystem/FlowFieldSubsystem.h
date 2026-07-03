// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AI/Navigation/NavigationTypes.h"
#include "FlowField/Struct/FlowFieldNavTypes.h"
#include "FlowFieldSubsystem.generated.h"

class AFlowFieldRecastNavMesh;
class AFlowFieldNavLinkProxy;
class UFlowFieldDensityComponent;
class AFlowFieldAgentPawn;

DECLARE_LOG_CATEGORY_EXTERN(LogFlowField, Log, All);

OUTBREAK_API bool IsFlowFieldNetworkDiagnosticsEnabled();

/**
 * World-local Flow Field cache. SetGoal builds one shared field and agents query
 * their current location for the next horizontal movement direction.
 */
UCLASS()
class OUTBREAK_API UFlowFieldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

public:
	/** Builds the shared field toward GoalWorldLocation. Call after navigation generation is complete. */
	UFUNCTION(BlueprintCallable, Category = "FlowField")
	bool SetGoal(const FVector& GoalWorldLocation);

	/** Returns the horizontal direction from WorldLocation toward the active flow-field goal. */
	UFUNCTION(BlueprintPure, Category = "FlowField")
	bool QueryDirection(const FVector& WorldLocation, FVector& OutDirection) const;

	bool QueryNodeRef(const FVector& WorldLocation, NavNodeRef& OutNodeRef) const;

	/** Returns a Flow Field movement offset constrained to the current NavMesh surface for one movement step. */
	bool QueryConstrainedMove(const FVector& WorldLocation, float MaxTravelDistance, FVector& OutMoveOffset) const;
	
	bool QueryNavLink(const FVector& WorldLocation, FFlowFieldNavLinkInfo& OutNavLinksRef) const;

	UFUNCTION(BlueprintPure, Category = "FlowField")
	bool HasFlowField() const;

private:
	AFlowFieldRecastNavMesh* FindNavMesh() const;

	TWeakObjectPtr<AFlowFieldRecastNavMesh> CachedNavMesh;
	TArray<TWeakObjectPtr<UFlowFieldDensityComponent>> RegisteredDensityComponents;
	TMap<TWeakObjectPtr<UFlowFieldDensityComponent>, FVector> PreviousDensityComponentPositions;
	double LastSetGoalFailureLogTime = -1.0;
	bool bHasFlowField = false;
	FVector CurrentGoal = FVector::ZeroVector;
};
