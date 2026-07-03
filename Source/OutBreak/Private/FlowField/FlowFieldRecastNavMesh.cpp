// Copyright Epic Games, Inc. All Rights Reserved.

#include "FlowField/FlowFieldRecastNavMesh.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FlowField/FlowFieldNavLinkProxy.h"
#include "Navigation/NavLinkProxy.h"

#if WITH_RECAST
#include "Detour/DetourNavMesh.h"
#endif

namespace
{
	constexpr float UnwalkableAreaCost = TNumericLimits<float>::Max() * 0.5f;

#if WITH_RECAST
	bool ResolveGeneratedNavLinkEndpointRefs(
		const dtNavMesh& DetourNavMesh,
		const dtMeshTile& Tile,
		const dtOffMeshConnection& Connection,
		NavNodeRef& OutStartRef,
		NavNodeRef& OutEndRef)
	{
		OutStartRef = INVALID_NAVNODEREF;
		OutEndRef = INVALID_NAVNODEREF;

		if (Tile.header == nullptr
			|| Tile.polys == nullptr
			|| Connection.poly >= Tile.header->polyCount)
		{
			return false;
		}

		const dtPoly& OffMeshPoly = Tile.polys[Connection.poly];
		if (OffMeshPoly.getType() != DT_POLYTYPE_OFFMESH_POINT)
		{
			return false;
		}

		unsigned int LinkIndex = OffMeshPoly.firstLink;
		while (LinkIndex != DT_NULL_LINK)
		{
			const dtLink& Link = DetourNavMesh.getLink(&Tile, LinkIndex);
			LinkIndex = Link.next;

			if (Link.ref == 0)
			{
				continue;
			}

			if (Link.edge == 0)
			{
				OutStartRef = static_cast<NavNodeRef>(Link.ref);
			}
			else if (Link.edge == 1)
			{
				OutEndRef = static_cast<NavNodeRef>(Link.ref);
			}
		}

		return OutStartRef != INVALID_NAVNODEREF
			&& OutEndRef != INVALID_NAVNODEREF
			&& OutStartRef != OutEndRef;
	}
#endif

	bool GetFlowPolyTraversalCosts(
		const AFlowFieldRecastNavMesh& NavMesh,
		const FNavigationQueryFilter& QueryFilter,
		const float* AreaCosts,
		const float* AreaEnteringCosts,
		const NavNodeRef PolyRef,
		float& OutTravelCostMultiplier,
		float& OutEnteringCost)
	{
		uint16 PolyFlags = 0;
		uint16 AreaFlags = 0;
		if (!NavMesh.GetPolyFlags(PolyRef, PolyFlags, AreaFlags)
			|| (PolyFlags & QueryFilter.GetIncludeFlags()) == 0
			|| (PolyFlags & QueryFilter.GetExcludeFlags()) != 0)
		{
			return false;
		}

		const uint32 AreaId = NavMesh.GetPolyAreaID(PolyRef);
		if (AreaId >= RECAST_MAX_AREAS
			|| !FMath::IsFinite(AreaCosts[AreaId])
			|| !FMath::IsFinite(AreaEnteringCosts[AreaId])
			|| AreaCosts[AreaId] >= UnwalkableAreaCost
			|| AreaEnteringCosts[AreaId] >= UnwalkableAreaCost)
		{
			return false;
		}

		OutTravelCostMultiplier = AreaCosts[AreaId];
		OutEnteringCost = AreaEnteringCosts[AreaId];
		return true;
	}
}

bool AFlowFieldRecastNavMesh::BuildFlowField(const FVector& GoalWorldLocation)
{
	bHasFlowField = false;
	++BakeGeneration;

	if (!CollectFlowNodes())
	{
		return false;
	}

	FNavLocation ProjectedGoal;
	const FVector ProjectionExtent = GetModifiedQueryExtent(GetDefaultQueryExtent());
	const FSharedConstNavQueryFilter QueryFilter = GetFlowQueryFilter();
	if (!QueryFilter.IsValid()
		|| !ProjectPoint(GoalWorldLocation, ProjectedGoal, ProjectionExtent, QueryFilter)
		|| ProjectedGoal.NodeRef == INVALID_NAVNODEREF
		|| !FlowNodes.Contains(ProjectedGoal.NodeRef))
	{
		return false;
	}

	CalculateIntegrationCosts(ProjectedGoal.NodeRef);
	SelectFlowDirections(ProjectedGoal.NodeRef);
	if (bSmoothFlowDirections)
	{
		SmoothFlowDirections(ProjectedGoal.NodeRef);
	}

	if (bDrawFlowField)
	{
		DrawFlowFieldDebug(ProjectedGoal.NodeRef);
	}

	CurrentGoal = GoalWorldLocation;
	bHasFlowField = true;
	return true;
}

FSharedConstNavQueryFilter AFlowFieldRecastNavMesh::GetFlowQueryFilter() const
{
	return FlowQueryFilter != nullptr ? GetQueryFilter(FlowQueryFilter) : GetDefaultQueryFilter();
}

bool AFlowFieldRecastNavMesh::QueryDirection(
	const FVector& WorldLocation,
	FVector& OutDirection) const
{
	OutDirection = FVector::ZeroVector;

	if (!bHasFlowField)
	{
		return false;
	}

	NavNodeRef CurrentNodeRef = INVALID_NAVNODEREF;
	if (!QueryNodeRef(WorldLocation, CurrentNodeRef))
	{
		return false;
	}

	const FFlowFieldPolyNode* CurrentNode = FlowNodes.Find(CurrentNodeRef);
	if (CurrentNode == nullptr || CurrentNode->IntegrationCost == TNumericLimits<float>::Max())
	{
		return false;
	}

	FVector DescentGradient = FVector::ZeroVector;
	for (const NavNodeRef NeighborRef : CurrentNode->OutgoingNeighbors)
	{
		const FFlowFieldPolyNode* NeighborNode = FlowNodes.Find(NeighborRef);
		if (NeighborNode == nullptr || NeighborNode->IntegrationCost == TNumericLimits<float>::Max())
		{
			continue;
		}

		FVector ToNeighbor = NeighborNode->Center - CurrentNode->Center;
		ToNeighbor.Z = 0.0f;
		const float NeighborDistance = ToNeighbor.Size();
		if (NeighborDistance > KINDA_SMALL_NUMBER)
		{
			const float CostSlope = (CurrentNode->IntegrationCost - NeighborNode->IntegrationCost) / NeighborDistance;
			DescentGradient += ToNeighbor.GetSafeNormal() * CostSlope;
		}
	}

	OutDirection = DescentGradient.GetSafeNormal2D();
	if (OutDirection.IsNearlyZero())
	{
		OutDirection = CurrentNode->FlowDirection;
	}

	return !OutDirection.IsNearlyZero();
}

bool AFlowFieldRecastNavMesh::QueryNodeRef(const FVector& WorldLocation, NavNodeRef& OutNodeRef) const
{
	OutNodeRef = INVALID_NAVNODEREF;

	const FSharedConstNavQueryFilter QueryFilter = GetFlowQueryFilter();
	FNavLocation ProjectedLocation;
	if (!QueryFilter.IsValid()
		|| !ProjectPoint(WorldLocation, ProjectedLocation, GetDefaultQueryExtent(), QueryFilter))
	{
		return false;
	}

	OutNodeRef = ProjectedLocation.NodeRef;
	return OutNodeRef != INVALID_NAVNODEREF;
}

bool AFlowFieldRecastNavMesh::QueryConstrainedMove(
	const FVector& WorldLocation,
	const float MaxTravelDistance,
	FVector& OutMoveOffset) const
{
	OutMoveOffset = FVector::ZeroVector;

	FVector RawFlowDirection;
	if (MaxTravelDistance <= KINDA_SMALL_NUMBER || !QueryDirection(WorldLocation, RawFlowDirection))
	{
		return false;
	}

	const FSharedConstNavQueryFilter QueryFilter = GetFlowQueryFilter();
	FNavLocation StartLocation;
	if (!QueryFilter.IsValid()
		|| !ProjectPoint(WorldLocation, StartLocation, GetDefaultQueryExtent(), QueryFilter, this))
	{
		return false;
	}

	FNavLocation SurfaceLocation;
	const FVector DesiredTarget = StartLocation.Location + RawFlowDirection * MaxTravelDistance;
	if (!FindMoveAlongSurface(StartLocation, DesiredTarget, SurfaceLocation, QueryFilter, this))
	{
		return false;
	}

	FVector SurfaceDelta = SurfaceLocation.Location - WorldLocation;
	SurfaceDelta.Z = 0.0f;
	if (SurfaceDelta.Size2D() >= MaxTravelDistance * 0.5f)
	{
		OutMoveOffset = SurfaceDelta;
		return true;
	}

	TArray<FNavigationPortalEdge> WallSegments;
	if (!GetPolyWallSegments(StartLocation.NodeRef, QueryFilter, this, WallSegments))
	{
		OutMoveOffset = SurfaceDelta;
		return !OutMoveOffset.IsNearlyZero();
	}

	const float ProbeDistance = FMath::Max(MaxTravelDistance * 8.0f, 100.0f);
	float BestCost = TNumericLimits<float>::Max();
	float BestAlignment = -1.0f;
	FVector BestTangent = FVector::ZeroVector;

	for (const FNavigationPortalEdge& WallSegment : WallSegments)
	{
		FVector WallTangent = WallSegment.Right - WallSegment.Left;
		WallTangent.Z = 0.0f;
		WallTangent = WallTangent.GetSafeNormal();
		if (WallTangent.IsNearlyZero())
		{
			continue;
		}

		for (const float Sign : { -1.0f, 1.0f })
		{
			const FVector CandidateTangent = WallTangent * Sign;
			FNavLocation ProbeLocation;
			if (!FindMoveAlongSurface(
				StartLocation,
				StartLocation.Location + CandidateTangent * ProbeDistance,
				ProbeLocation,
				QueryFilter,
				this))
			{
				continue;
			}

			const FFlowFieldPolyNode* ProbeNode = FlowNodes.Find(ProbeLocation.NodeRef);
			if (ProbeNode == nullptr || ProbeNode->IntegrationCost == TNumericLimits<float>::Max())
			{
				continue;
			}

			const float Alignment = FVector::DotProduct(CandidateTangent, RawFlowDirection);
			if (ProbeNode->IntegrationCost < BestCost
				|| (FMath::IsNearlyEqual(ProbeNode->IntegrationCost, BestCost) && Alignment > BestAlignment))
			{
				BestCost = ProbeNode->IntegrationCost;
				BestAlignment = Alignment;
				BestTangent = CandidateTangent;
			}
		}
	}

	if (BestTangent.IsNearlyZero()
		|| !FindMoveAlongSurface(
			StartLocation,
			StartLocation.Location + BestTangent * MaxTravelDistance,
			SurfaceLocation,
			QueryFilter,
			this))
	{
		OutMoveOffset = SurfaceDelta;
		return !OutMoveOffset.IsNearlyZero();
	}

	SurfaceDelta = SurfaceLocation.Location - WorldLocation;
	SurfaceDelta.Z = 0.0f;
	OutMoveOffset = SurfaceDelta;
	return !OutMoveOffset.IsNearlyZero();
}

bool AFlowFieldRecastNavMesh::QueryNavLink(
	const FVector& WorldLocation,
	FFlowFieldNavLinkInfo& OutNavLink) const
{
	OutNavLink = FFlowFieldNavLinkInfo();

	if (!bHasFlowField)
	{
		return false;
	}

	NavNodeRef CurrentNodeRef = INVALID_NAVNODEREF;
	if (!QueryNodeRef(WorldLocation, CurrentNodeRef))
	{
		return false;
	}

	const FFlowFieldPolyNode* Node = FlowNodes.Find(CurrentNodeRef);
	if (Node != nullptr && Node->Link.IsValid())
	{
		OutNavLink = Node->Link;
		return true;
	}

	return false;
}

bool AFlowFieldRecastNavMesh::CollectFlowNodes()
{
	FlowNodes.Reset();

	const FSharedConstNavQueryFilter QueryFilter = GetFlowQueryFilter();
	if (!QueryFilter.IsValid())
	{
		return false;
	}

	float AreaCosts[RECAST_MAX_AREAS];
	float AreaEnteringCosts[RECAST_MAX_AREAS];
	QueryFilter->GetAllAreaCosts(AreaCosts, AreaEnteringCosts, RECAST_MAX_AREAS);

	TArray<FNavTileRef> TileRefs;
	GetAllNavMeshTiles(TileRefs);

	for (const FNavTileRef TileRef : TileRefs)
	{
		if (!TileRef.IsValid())
		{
			continue;
		}

		TArray<FNavPoly> TilePolys;
		if (!GetPolysInTile(TileRef, TilePolys))
		{
			continue;
		}

		for (const FNavPoly& Poly : TilePolys)
		{
			float TravelCostMultiplier = 1.0f;
			float EnteringCost = 0.0f;
			if (Poly.Ref == INVALID_NAVNODEREF
				|| !GetFlowPolyTraversalCosts(*this, *QueryFilter, AreaCosts, AreaEnteringCosts, Poly.Ref, TravelCostMultiplier, EnteringCost))
			{
				continue;
			}

			FVector PolyCenter;
			if (!GetPolyCenter(Poly.Ref, PolyCenter))
			{
				continue;
			}

			FFlowFieldPolyNode& Node = FlowNodes.FindOrAdd(Poly.Ref);
			Node.PolyRef = Poly.Ref;
			Node.Center = PolyCenter;
			Node.TravelCostMultiplier = TravelCostMultiplier;
			Node.EnteringCost = EnteringCost;
		}
	}

	if (FlowNodes.IsEmpty())
	{
		return false;
	}

	for (TPair<NavNodeRef, FFlowFieldPolyNode>& Pair : FlowNodes)
	{
		TArray<NavNodeRef> PolyNeighbors;
		if (!GetPolyNeighbors(Pair.Key, PolyNeighbors))
		{
			continue;
		}

		for (const NavNodeRef NeighborRef : PolyNeighbors)
		{
			if (NeighborRef != INVALID_NAVNODEREF && FlowNodes.Contains(NeighborRef))
			{
				AddDirectedNeighbor(Pair.Key, NeighborRef);
			}
		}
	}

	CollectNavLinkNeighbors();
	CollectGeneratedNavLinkNeighbors();

	return true;
}

void AFlowFieldRecastNavMesh::CollectNavLinkNeighbors()
{
	UWorld* World = GetWorld();
	const FSharedConstNavQueryFilter QueryFilter = GetFlowQueryFilter();
	if (World == nullptr || !QueryFilter.IsValid())
	{
		return;
	}

	const FVector ProjectionExtent = GetModifiedQueryExtent(GetDefaultQueryExtent());
	for (TActorIterator<ANavLinkProxy> It(World); It; ++It)
	{
		const ANavLinkProxy& LinkProxy = **It;
		const FTransform ActorTransform = LinkProxy.GetActorTransform();
		for (const FNavigationLink& LocalLink : LinkProxy.PointLinks)
		{
			const FNavigationLink WorldLink = LocalLink.Transform(ActorTransform);
			FNavLocation LeftLocation;
			FNavLocation RightLocation;
			if (!ProjectPoint(WorldLink.Left, LeftLocation, ProjectionExtent, QueryFilter)
				|| !ProjectPoint(WorldLink.Right, RightLocation, ProjectionExtent, QueryFilter)
				|| !FlowNodes.Contains(LeftLocation.NodeRef)
				|| !FlowNodes.Contains(RightLocation.NodeRef)
				|| LeftLocation.NodeRef == RightLocation.NodeRef)
			{
				continue;
			}

			if (WorldLink.Direction != ENavLinkDirection::RightToLeft)
			{
				AddDirectedNeighbor(LeftLocation.NodeRef, RightLocation.NodeRef);
				AddLinkProxy(LeftLocation.NodeRef, RightLocation.NodeRef, WorldLink.Left, WorldLink.Right, &LinkProxy);
			}
			if (WorldLink.Direction != ENavLinkDirection::LeftToRight)
			{
				AddDirectedNeighbor(RightLocation.NodeRef, LeftLocation.NodeRef);
				AddLinkProxy(RightLocation.NodeRef, LeftLocation.NodeRef, WorldLink.Right, WorldLink.Left, &LinkProxy);
			}
		}
	}
}

void AFlowFieldRecastNavMesh::CollectGeneratedNavLinkNeighbors()
{
#if WITH_RECAST
	const dtNavMesh* DetourNavMesh = GetRecastMesh();
	if (DetourNavMesh == nullptr)
	{
		return;
	}

	for (int32 TileIndex = 0; TileIndex < DetourNavMesh->getMaxTiles(); ++TileIndex)
	{
		const dtMeshTile* Tile = DetourNavMesh->getTile(TileIndex);
		if (Tile == nullptr
			|| Tile->header == nullptr
			|| Tile->offMeshCons == nullptr)
		{
			continue;
		}

		for (int32 LinkIndex = 0; LinkIndex < Tile->header->offMeshConCount; ++LinkIndex)
		{
			const dtOffMeshConnection& Connection = Tile->offMeshCons[LinkIndex];
			if (!Connection.getIsGenerated())
			{
				continue;
			}

			NavNodeRef StartRef = INVALID_NAVNODEREF;
			NavNodeRef EndRef = INVALID_NAVNODEREF;
			if (!ResolveGeneratedNavLinkEndpointRefs(*DetourNavMesh, *Tile, Connection, StartRef, EndRef)
				|| !FlowNodes.Contains(StartRef)
				|| !FlowNodes.Contains(EndRef))
			{
				continue;
			}

			AddDirectedNeighbor(StartRef, EndRef);
			const FFlowFieldPolyNode* StartNode = FlowNodes.Find(StartRef);
			const FFlowFieldPolyNode* EndNode = FlowNodes.Find(EndRef);
			const FVector StartPoint = StartNode != nullptr ? StartNode->Center : FVector::ZeroVector;
			const FVector EndPoint = EndNode != nullptr ? EndNode->Center : FVector::ZeroVector;
			AddLinkProxy(StartRef, EndRef, StartPoint, EndPoint, nullptr);

			if (Connection.getBiDirectional())
			{
				AddDirectedNeighbor(EndRef, StartRef);
				AddLinkProxy(EndRef, StartRef, EndPoint, StartPoint, nullptr);
			}
		}
	}
#endif
}

void AFlowFieldRecastNavMesh::AddDirectedNeighbor(const NavNodeRef FromRef, const NavNodeRef ToRef)
{
	if (FromRef == INVALID_NAVNODEREF || ToRef == INVALID_NAVNODEREF || FromRef == ToRef)
	{
		return;
	}

	FFlowFieldPolyNode* FromNode = FlowNodes.Find(FromRef);
	FFlowFieldPolyNode* ToNode = FlowNodes.Find(ToRef);
	if (FromNode != nullptr && ToNode != nullptr)
	{
		FromNode->OutgoingNeighbors.AddUnique(ToRef);
		ToNode->IncomingNeighbors.AddUnique(FromRef);
	}
}

void AFlowFieldRecastNavMesh::AddLinkProxy(
	const NavNodeRef FromRef,
	const NavNodeRef ToRef,
	const FVector& FromPoint,
	const FVector& ToPoint,
	const ANavLinkProxy* LinkProxy)
{
	if (FromRef == INVALID_NAVNODEREF
		|| ToRef == INVALID_NAVNODEREF
		|| FromRef == ToRef)
	{
		return;
	}

	FFlowFieldPolyNode* FromNode = FlowNodes.Find(FromRef);
	const FFlowFieldPolyNode* ToNode = FlowNodes.Find(ToRef);
	if (FromNode == nullptr || ToNode == nullptr)
	{
		return;
	}
	
	FromNode->Link.SourceNodeRef = FromRef;
	FromNode->Link.TargetNodeRef = ToRef;
	FromNode->Link.StartPoint = FromPoint.ContainsNaN() ? FromNode->Center : FromPoint;
	FromNode->Link.TargetPoint = ToPoint.ContainsNaN() ? ToNode->Center : ToPoint;
	FromNode->Link.BehaviorType = EFlowFieldNavBehaviorType::NONE;
	FromNode->Link.TraversalBakeData = FFlowFieldTraversalBakeData();

	const float HeightDelta = FromNode->Link.TargetPoint.Z - FromNode->Link.StartPoint.Z;
	const float AbsoluteHeightDelta = FMath::Abs(HeightDelta);
	const AFlowFieldNavLinkProxy* FlowFieldLinkProxy = Cast<AFlowFieldNavLinkProxy>(LinkProxy);
	const EFlowFieldTraversalBakeType ManualTraversalType = FlowFieldLinkProxy != nullptr
		? FlowFieldLinkProxy->TraversalBakeOverride
		: EFlowFieldTraversalBakeType::None;

	if (ManualTraversalType == EFlowFieldTraversalBakeType::Vault
		|| (ManualTraversalType == EFlowFieldTraversalBakeType::None
			&& AbsoluteHeightDelta >= VaultMinHeight
			&& AbsoluteHeightDelta <= VaultMaxHeight))
	{
		FromNode->Link.BehaviorType = EFlowFieldNavBehaviorType::VAULT;
	}
	else if (ManualTraversalType == EFlowFieldTraversalBakeType::SimpleClimb
		|| ManualTraversalType == EFlowFieldTraversalBakeType::HordeTower
		|| (ManualTraversalType == EFlowFieldTraversalBakeType::None
			&& AbsoluteHeightDelta >= ClimbMinHeight
			&& AbsoluteHeightDelta <= ClimbMaxHeight))
	{
		FromNode->Link.BehaviorType = HeightDelta > 0.0f
			? EFlowFieldNavBehaviorType::CLIMB
			: EFlowFieldNavBehaviorType::DROP;
	}
	else if (ManualTraversalType == EFlowFieldTraversalBakeType::Drop)
	{
		FromNode->Link.BehaviorType = EFlowFieldNavBehaviorType::DROP;
	}

	if (FromNode->Link.BehaviorType != EFlowFieldNavBehaviorType::NONE)
	{
		FromNode->Link.TraversalBakeData = BuildTraversalBakeData(
			FromRef,
			ToRef,
			FromNode->Link.StartPoint,
			FromNode->Link.TargetPoint,
			FromNode->Link.BehaviorType,
			LinkProxy);
	}
}

FFlowFieldTraversalBakeData AFlowFieldRecastNavMesh::BuildTraversalBakeData(
	const NavNodeRef FromRef,
	const NavNodeRef ToRef,
	const FVector& FromPoint,
	const FVector& ToPoint,
	const EFlowFieldNavBehaviorType BehaviorType,
	const ANavLinkProxy* LinkProxy) const
{
	FFlowFieldTraversalBakeData BakeData;
	if (BehaviorType == EFlowFieldNavBehaviorType::NONE
		|| FromRef == INVALID_NAVNODEREF
		|| ToRef == INVALID_NAVNODEREF
		|| FromRef == ToRef)
	{
		return BakeData;
	}

	const bool bFromIsLower = FromPoint.Z <= ToPoint.Z;
	const NavNodeRef LowerRef = bFromIsLower ? FromRef : ToRef;
	const NavNodeRef UpperRef = bFromIsLower ? ToRef : FromRef;
	const FVector LowerPoint = bFromIsLower ? FromPoint : ToPoint;
	const FVector UpperPoint = bFromIsLower ? ToPoint : FromPoint;
	const float WallHeight = FMath::Abs(ToPoint.Z - FromPoint.Z);

	const AFlowFieldNavLinkProxy* FlowFieldLinkProxy = Cast<AFlowFieldNavLinkProxy>(LinkProxy);
	const EFlowFieldTraversalBakeType ManualTraversalType = FlowFieldLinkProxy != nullptr
		? FlowFieldLinkProxy->TraversalBakeOverride
		: EFlowFieldTraversalBakeType::None;

	BakeData.TraversalType = EFlowFieldTraversalBakeType::None;
	if (ManualTraversalType != EFlowFieldTraversalBakeType::None)
	{
		BakeData.TraversalType = ManualTraversalType;
	}
	else if (BehaviorType == EFlowFieldNavBehaviorType::DROP)
	{
		BakeData.TraversalType = EFlowFieldTraversalBakeType::Drop;
	}
	else if (BehaviorType == EFlowFieldNavBehaviorType::VAULT)
	{
		BakeData.TraversalType = EFlowFieldTraversalBakeType::Vault;
	}
	else if (BehaviorType == EFlowFieldNavBehaviorType::CLIMB)
	{
		const bool bHeightRequiresTower = WallHeight >= HordeTowerMinHeight && WallHeight <= HordeTowerMaxHeight;
		const bool bManualRequiresTower = FlowFieldLinkProxy != nullptr && FlowFieldLinkProxy->bRequiresHordeTower;
		BakeData.TraversalType = (bHeightRequiresTower || bManualRequiresTower)
			? EFlowFieldTraversalBakeType::HordeTower
			: EFlowFieldTraversalBakeType::SimpleClimb;
	}

	BakeData.LowerNodeRef = LowerRef;
	BakeData.UpperNodeRef = UpperRef;
	BakeData.LowerEntryPoint = LowerPoint;
	BakeData.UpperExitPoint = UpperPoint;
	BakeData.WallHeight = WallHeight;
	BakeData.LandingPoint = UpperPoint;
	BakeData.DropTargetPoint = LowerPoint;
	BakeData.BakeGeneration = BakeGeneration;
	BakeData.TraversalId = static_cast<int32>(HashCombineFast(::GetTypeHash(LowerRef), ::GetTypeHash(UpperRef)) & 0x7fffffff);
	BakeData.bCanTraverseLowerToUpper = BehaviorType == EFlowFieldNavBehaviorType::CLIMB && FromRef == LowerRef && ToRef == UpperRef;
	BakeData.bCanTraverseUpperToLower = BehaviorType == EFlowFieldNavBehaviorType::DROP && FromRef == UpperRef && ToRef == LowerRef;
	BakeData.bRequiresHordeTower =
		BakeData.TraversalType == EFlowFieldTraversalBakeType::HordeTower
		|| (FlowFieldLinkProxy != nullptr && FlowFieldLinkProxy->bRequiresHordeTower);

	// WallForward is static traversal intent, not the per-frame flow vector.
	// Prefer explicit designer data because endpoint direction alone cannot
	// reliably describe concave or artist-authored tower walls.
	FVector WallForward = FlowFieldLinkProxy != nullptr ? FlowFieldLinkProxy->WallForwardOverride : FVector::ZeroVector;
	WallForward.Z = 0.0f;
	if (WallForward.IsNearlyZero())
	{
		WallForward = UpperPoint - LowerPoint;
		WallForward.Z = 0.0f;
	}
	if (WallForward.IsNearlyZero())
	{
		WallForward = ToPoint - FromPoint;
		WallForward.Z = 0.0f;
	}
	BakeData.WallForward = WallForward.GetSafeNormal2D();
	if (BakeData.WallForward.IsNearlyZero())
	{
		BakeData.WallForward = FVector::ForwardVector;
	}

	BakeData.WallNormal = (-BakeData.WallForward).GetSafeNormal2D();
	BakeData.WallTangent = FVector::CrossProduct(FVector::UpVector, BakeData.WallNormal).GetSafeNormal();
	if (BakeData.WallTangent.IsNearlyZero())
	{
		BakeData.WallTangent = FVector::RightVector;
	}

	return BakeData;
}

void AFlowFieldRecastNavMesh::CalculateIntegrationCosts(const NavNodeRef GoalPolyRef)
{
	FFlowFieldPolyNode* GoalNode = FlowNodes.Find(GoalPolyRef);
	if (GoalNode == nullptr)
	{
		return;
	}

	GoalNode->IntegrationCost = 0.0f;

	TArray<NavNodeRef> OpenSet;
	TSet<NavNodeRef> SettledPolys;
	OpenSet.Add(GoalPolyRef);

	while (!OpenSet.IsEmpty())
	{
		int32 LowestCostIndex = 0;
		for (int32 Index = 1; Index < OpenSet.Num(); ++Index)
		{
			const FFlowFieldPolyNode* CandidateNode = FlowNodes.Find(OpenSet[Index]);
			const FFlowFieldPolyNode* LowestNode = FlowNodes.Find(OpenSet[LowestCostIndex]);
			if (CandidateNode != nullptr
				&& LowestNode != nullptr
				&& CandidateNode->IntegrationCost < LowestNode->IntegrationCost)
			{
				LowestCostIndex = Index;
			}
		}

		const NavNodeRef CurrentRef = OpenSet[LowestCostIndex];
		OpenSet.RemoveAtSwap(LowestCostIndex, 1, EAllowShrinking::No);

		if (SettledPolys.Contains(CurrentRef))
		{
			continue;
		}

		FFlowFieldPolyNode* CurrentNode = FlowNodes.Find(CurrentRef);
		if (CurrentNode == nullptr)
		{
			continue;
		}

		SettledPolys.Add(CurrentRef);

		for (const NavNodeRef NeighborRef : CurrentNode->IncomingNeighbors)
		{
			if (SettledPolys.Contains(NeighborRef))
			{
				continue;
			}

			FFlowFieldPolyNode* NeighborNode = FlowNodes.Find(NeighborRef);
			if (NeighborNode == nullptr)
			{
				continue;
			}

			const float TravelCost = static_cast<float>(FVector::Dist(CurrentNode->Center, NeighborNode->Center))
				* CurrentNode->TravelCostMultiplier + CurrentNode->EnteringCost;
			const float CandidateCost = CurrentNode->IntegrationCost + TravelCost;
			if (CandidateCost < NeighborNode->IntegrationCost)
			{
				NeighborNode->IntegrationCost = CandidateCost;
				OpenSet.Add(NeighborRef);
			}
		}
	}
}

void AFlowFieldRecastNavMesh::SelectFlowDirections(const NavNodeRef GoalPolyRef)
{
	for (TPair<NavNodeRef, FFlowFieldPolyNode>& Pair : FlowNodes)
	{
		FFlowFieldPolyNode& CurrentNode = Pair.Value;
		CurrentNode.BestNextPolyRef = INVALID_NAVNODEREF;
		CurrentNode.FlowDirection = FVector::ZeroVector;

		if (CurrentNode.PolyRef == GoalPolyRef)
		{
			continue;
		}

		float LowestNeighborCost = TNumericLimits<float>::Max();
		for (const NavNodeRef NeighborRef : CurrentNode.OutgoingNeighbors)
		{
			const FFlowFieldPolyNode* NeighborNode = FlowNodes.Find(NeighborRef);
			if (NeighborNode != nullptr && NeighborNode->IntegrationCost < LowestNeighborCost)
			{
				LowestNeighborCost = NeighborNode->IntegrationCost;
				CurrentNode.BestNextPolyRef = NeighborRef;
			}
		}

		if (const FFlowFieldPolyNode* BestNextNode = FlowNodes.Find(CurrentNode.BestNextPolyRef))
		{
			FVector Direction = BestNextNode->Center - CurrentNode.Center;
			Direction.Z = 0.0f;
			CurrentNode.FlowDirection = Direction.GetSafeNormal();
		}
	}
}

void AFlowFieldRecastNavMesh::SmoothFlowDirections(const NavNodeRef GoalPolyRef)
{
	if (FlowDirectionSmoothingStrength <= 0.0f)
	{
		return;
	}

	TMap<NavNodeRef, FVector> OriginalDirections;
	OriginalDirections.Reserve(FlowNodes.Num());
	for (const TPair<NavNodeRef, FFlowFieldPolyNode>& Pair : FlowNodes)
	{
		OriginalDirections.Add(Pair.Key, Pair.Value.FlowDirection);
	}

	for (const TPair<NavNodeRef, FFlowFieldPolyNode>& Pair : FlowNodes)
	{
		const FFlowFieldPolyNode& CurrentNode = Pair.Value;
		const FVector* OriginalDirection = OriginalDirections.Find(Pair.Key);
		if (CurrentNode.PolyRef == GoalPolyRef
			|| CurrentNode.IntegrationCost == TNumericLimits<float>::Max()
			|| OriginalDirection == nullptr
			|| OriginalDirection->IsNearlyZero())
		{
			continue;
		}

		const FVector BaseDirection = OriginalDirection->GetSafeNormal2D();
		FVector DescentGradient = FVector::ZeroVector;

		for (const NavNodeRef NeighborRef : CurrentNode.OutgoingNeighbors)
		{
			const FFlowFieldPolyNode* NeighborNode = FlowNodes.Find(NeighborRef);
			if (NeighborNode == nullptr
				|| NeighborNode->IntegrationCost == TNumericLimits<float>::Max())
			{
				continue;
			}

			FVector ToNeighbor = NeighborNode->Center - CurrentNode.Center;
			ToNeighbor.Z = 0.0f;
			const float NeighborDistance = ToNeighbor.Size();
			if (NeighborDistance <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			const float CostSlope = (CurrentNode.IntegrationCost - NeighborNode->IntegrationCost) / NeighborDistance;
			DescentGradient += ToNeighbor.GetSafeNormal() * CostSlope;
		}

		const FVector GradientDirection = DescentGradient.GetSafeNormal2D();
		const FVector CandidateDirection =
			FMath::Lerp(BaseDirection, GradientDirection, FlowDirectionSmoothingStrength).GetSafeNormal2D();
		if (!GradientDirection.IsNearlyZero()
			&& FVector::DotProduct(CandidateDirection, BaseDirection) >= MinimumProgressDot)
		{
			FlowNodes.FindChecked(Pair.Key).FlowDirection = CandidateDirection;
		}
	}
}

void AFlowFieldRecastNavMesh::DrawFlowFieldDebug(const NavNodeRef GoalPolyRef) const
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	for (const TPair<NavNodeRef, FFlowFieldPolyNode>& Pair : FlowNodes)
	{
		const FFlowFieldPolyNode& Node = Pair.Value;
		const FVector Start = Node.Center + FVector(0.0f, 0.0f, DebugHeight);

		if (Node.PolyRef == GoalPolyRef)
		{
			DrawDebugSphere(World, Start, 25.0f, 12, FColor::Red, false, DebugLifeTime, 0, 2.0f);
			continue;
		}

		if (Node.IntegrationCost == TNumericLimits<float>::Max())
		{
			DrawDebugPoint(World, Start, 8.0f, FColor::Silver, false, DebugLifeTime, 0);
			continue;
		}

		if (!Node.FlowDirection.IsNearlyZero())
		{
			const FVector End = Start + Node.FlowDirection * DebugArrowLength;
			DrawDebugDirectionalArrow(World, Start, End, 20.0f, FColor::Green, false, DebugLifeTime, 0, 15.0f);
		}
	}
}
