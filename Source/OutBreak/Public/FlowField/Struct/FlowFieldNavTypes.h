// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AI/Navigation/NavigationTypes.h"
#include "Navigation/NavLinkProxy.h"
#include "FlowFieldNavTypes.generated.h"

/* ToDo Liist*/

/**
 * Player 마다 FlowField Data가 도착된 Nav Node가 Target이 존재하는 위치 일 경우 바로 직선으로 이동하도록 Query문 반환해야함.
 * Player에 대한 Nav 정보는 각 Player Capsule 크기 정보를 보유해야함. 이유는 도착 반경을 지정해야하기 때문에.
 */
class AFlowFieldNavLinkProxy;

UENUM(BlueprintType)
enum class EFlowFieldNavBehaviorType : uint8
{
	NONE,
	MOVE,
	DROP,
	VAULT,
	CLIMB,
	
};

UENUM(BlueprintType)
enum class EFlowFieldTraversalBakeType : uint8
{
	None,
	SimpleClimb,
	HordeTower,
	Drop,
	Vault,
	Crawl,
};

/** Static traversal data computed while the Flow Field graph is rebuilt.
 *
 * This data deliberately contains only values that are stable until the next
 * NavMesh/Flow Field rebuild. Runtime tower state such as support Pawns,
 * current tower height, explosion suppression, or occupied slots must stay in
 * movement/runtime systems so static bake data cannot go stale every frame.
 */
USTRUCT()
struct OUTBREAK_API FFlowFieldTraversalBakeData
{
	GENERATED_BODY()

	EFlowFieldTraversalBakeType TraversalType = EFlowFieldTraversalBakeType::None;

	NavNodeRef LowerNodeRef = INVALID_NAVNODEREF;
	NavNodeRef UpperNodeRef = INVALID_NAVNODEREF;

	FVector LowerEntryPoint = FVector::ZeroVector;
	FVector UpperExitPoint = FVector::ZeroVector;

	/** Direction used by an Agent when rushing toward the wall before traversal. */
	FVector WallForward = FVector::ForwardVector;

	/** Surface normal pointing away from the wall. WallForward is normally -WallNormal. */
	FVector WallNormal = FVector::BackwardVector;

	/** Left/right axis along the wall, computed from world up and WallNormal. */
	FVector WallTangent = FVector::RightVector;

	float WallHeight = 0.0f;
	float UsableWallWidth = 0.0f;

	FVector LandingPoint = FVector::ZeroVector;
	FVector DropTargetPoint = FVector::ZeroVector;

	int32 TraversalId = INDEX_NONE;
	uint32 BakeGeneration = 0;

	bool bCanTraverseLowerToUpper = false;
	bool bCanTraverseUpperToLower = false;
	bool bRequiresHordeTower = false;

	bool IsValid() const
	{
		return TraversalType != EFlowFieldTraversalBakeType::None
			&& LowerNodeRef != INVALID_NAVNODEREF
			&& UpperNodeRef != INVALID_NAVNODEREF
			&& LowerNodeRef != UpperNodeRef
			&& !LowerEntryPoint.ContainsNaN()
			&& !UpperExitPoint.ContainsNaN()
			&& !WallForward.IsNearlyZero()
			&& !WallNormal.IsNearlyZero()
			&& WallHeight >= 0.0f;
	}
};

USTRUCT()
struct OUTBREAK_API FFlowFieldNavLinkInfo
{
	GENERATED_BODY()

	NavNodeRef SourceNodeRef = INVALID_NAVNODEREF;
	NavNodeRef TargetNodeRef = INVALID_NAVNODEREF;
	
	FNavigationPortalEdge BetweenEdge = {};

	FVector StartPoint = FVector::ZeroVector;
	FVector TargetPoint = FVector::ZeroVector;

	EFlowFieldNavBehaviorType BehaviorType = EFlowFieldNavBehaviorType::NONE;

	FFlowFieldTraversalBakeData TraversalBakeData;

	bool IsValid() const
	{
		return SourceNodeRef != INVALID_NAVNODEREF
			&& TargetNodeRef != INVALID_NAVNODEREF
			&& SourceNodeRef != TargetNodeRef
			&& BehaviorType != EFlowFieldNavBehaviorType::NONE
			&& !StartPoint.ContainsNaN()
			&& !TargetPoint.ContainsNaN();
	}
};


/** Flow Field 계산에 필요한 Recast Polygon의 최소 데이터입니다. */
USTRUCT()
struct OUTBREAK_API FFlowFieldPolyNode
{
	GENERATED_BODY()

	NavNodeRef PolyRef = INVALID_NAVNODEREF;
	FVector Center = FVector::ZeroVector;
	float TravelCostMultiplier = 1.0f;
	float EnteringCost = 0.0f;

	
	FFlowFieldNavLinkInfo Link;
	
	/** Forward movement neighbours, including NavLink endpoint connections. */
	TArray<NavNodeRef> OutgoingNeighbors;

	/** Reverse traversal neighbours used only while propagating goal costs. */
	TArray<NavNodeRef> IncomingNeighbors;

	float IntegrationCost = TNumericLimits<float>::Max();

	NavNodeRef BestNextPolyRef = INVALID_NAVNODEREF;
	FVector FlowDirection = FVector::ZeroVector;
};
