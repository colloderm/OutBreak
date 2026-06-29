// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "NavMesh/RecastNavMesh.h"
#include "Struct/FlowFieldTypes.h"
#include "FlowFieldRecastNavMesh.generated.h"

class ANavLinkProxy;

/** 현재 빌드된 Recast NavMesh Polygon 연결을 사용해 Flow Field를 디버그 출력하는 NavMesh입니다. */
UCLASS(config = Engine, defaultconfig)
class OUTBREAK_API AFlowFieldRecastNavMesh : public ARecastNavMesh
{
	GENERATED_BODY()

public:
	/** GoalWorldLocation을 포함한 Polygon을 목적지로 하여 Flow Field를 다시 계산합니다. */
	UFUNCTION(BlueprintCallable, Category = "FlowField")
	bool BuildFlowField(const FVector& GoalWorldLocation);

	bool QueryDirection(const FVector& WorldLocation, FVector& OutDirection) const;
	bool QueryNodeRef(const FVector& WorldLocation, NavNodeRef& OutNodeRef) const;
	bool QueryConstrainedMove(const FVector& WorldLocation, float MaxTravelDistance, FVector& OutMoveOffset) const;
	bool QueryNavLink(const FVector& WorldLocation, FFlowFieldNavLinkInfo& OutNavLink) const;
	bool HasFlowField() const { return bHasFlowField; }

	/** Navigation filter whose area costs and exclusions are used by this Flow Field. */
	UPROPERTY(EditAnywhere, Category = "FlowField|Navigation")
	TSubclassOf<UNavigationQueryFilter> FlowQueryFilter;

	/** Returns the filter that defines traversable polygons and navigation-area costs. */
	FSharedConstNavQueryFilter GetFlowQueryFilter() const;

	UPROPERTY(EditAnywhere, Category = "FlowField|Debug")
	bool bDrawFlowField = true;

	UPROPERTY(EditAnywhere, Category = "FlowField|Debug")
	float DebugArrowLength = 100.0f;

	UPROPERTY(EditAnywhere, Category = "FlowField|Debug")
	float DebugHeight = 20.0f;

	UPROPERTY(EditAnywhere, Category = "FlowField|Debug")
	float DebugLifeTime = 10.0f;

	UPROPERTY(EditAnywhere, Category = "FlowField|Traversal")
	float VaultMinHeight = 80.0f;
	
	UPROPERTY(EditAnywhere, Category = "FlowField|Traversal")
	float VaultMaxHeight = 240.0f;
	
	UPROPERTY(EditAnywhere, Category = "FlowField|Traversal")
	float ClimbMinHeight = 250.0f;
	
	UPROPERTY(EditAnywhere, Category = "FlowField|Traversal")
	float ClimbMaxHeight = 1000.0f;

	UPROPERTY(EditAnywhere, Category = "FlowField|Traversal|Horde Tower")
	float HordeTowerMinHeight = 600.0f;

	UPROPERTY(EditAnywhere, Category = "FlowField|Traversal|Horde Tower")
	float HordeTowerMaxHeight = 1600.0f;
	
	/** Smooths polygon flow directions after they are selected from integration costs. */
	UPROPERTY(EditAnywhere, Category = "FlowField|Smoothing")
	bool bSmoothFlowDirections = true;

	/** 0 keeps the original direction; 1 uses the full integration-cost gradient. */
	UPROPERTY(EditAnywhere, Category = "FlowField|Smoothing", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FlowDirectionSmoothingStrength = 1.0f;

	/** The final vector must retain this much agreement with the original downhill direction. */
	UPROPERTY(EditAnywhere, Category = "FlowField|Smoothing", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float MinimumProgressDot = 0.0f;

private:
	/** NavMesh Polygon 수집 후 실제 Recast 연결 관계를 저장합니다. */
	bool CollectFlowNodes();
	void CollectNavLinkNeighbors();
	void CollectGeneratedNavLinkNeighbors();
	void AddDirectedNeighbor(NavNodeRef FromRef, NavNodeRef ToRef);
	void AddLinkProxy(NavNodeRef FromRef, NavNodeRef ToRef, const FVector& FromPoint, const FVector& ToPoint, const ANavLinkProxy* LinkProxy);
	FFlowFieldTraversalBakeData BuildTraversalBakeData(
		NavNodeRef FromRef,
		NavNodeRef ToRef,
		const FVector& FromPoint,
		const FVector& ToPoint,
		EFlowFieldNavBehaviorType BehaviorType,
		const ANavLinkProxy* LinkProxy) const;

	/** 목적지에서 역방향 Dijkstra 비용 전파를 수행합니다. */
	void CalculateIntegrationCosts(NavNodeRef GoalPolyRef);

	/** 각 Polygon의 최저 비용 이웃과 그 방향 벡터를 결정합니다. */
	void SelectFlowDirections(NavNodeRef GoalPolyRef);

	/** Applies one snapshot-based integration-gradient smoothing pass to the selected directions. */
	void SmoothFlowDirections(NavNodeRef GoalPolyRef);


	/** 계산 결과를 Polygon 중심에 Debug Arrow로 표시합니다. */
	void DrawFlowFieldDebug(NavNodeRef GoalPolyRef) const;

	TMap<NavNodeRef, FFlowFieldPolyNode> FlowNodes;
	bool bHasFlowField = false;
	FVector CurrentGoal = FVector::ZeroVector;
	uint32 BakeGeneration = 0;
};
