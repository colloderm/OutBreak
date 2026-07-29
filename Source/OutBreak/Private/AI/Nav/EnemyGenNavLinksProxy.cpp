// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Nav/EnemyGenNavLinksProxy.h"

#include "AIController.h"
#include "NavMesh/NavMeshPath.h"
#include "NavMesh/RecastNavMesh.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "AI/Components/EnemyMovementComponent.h"

namespace
{
	constexpr double LinkEndpointMatchTolerance = 10.0;

	bool TryGetCorridorLinkEndpoints(
		const FNavMeshPath& NavMeshPath,
		const ARecastNavMesh& RecastNavData,
		const FNavLinkId LinkProxyId,
		const FVector& DestPoint,
		FVector& OutStart,
		FVector& OutEnd)
	{
		double ClosestEndpointDistanceSquared = TNumericLimits<double>::Max();
		FVector ClosestStart = FVector::ZeroVector;

		for (const NavNodeRef PolyRef : NavMeshPath.PathCorridor)
		{
			if (RecastNavData.GetNavLinkUserId(PolyRef) != LinkProxyId)
			{
				continue;
			}

			FVector PointA = FVector::ZeroVector;
			FVector PointB = FVector::ZeroVector;
			if (!RecastNavData.GetLinkEndPoints(PolyRef, PointA, PointB))
			{
				continue;
			}

			const double PointADistanceSquared =
				FVector::DistSquared(PointA, DestPoint);
			const double PointBDistanceSquared =
				FVector::DistSquared(PointB, DestPoint);

			if (PointADistanceSquared < ClosestEndpointDistanceSquared)
			{
				ClosestEndpointDistanceSquared = PointADistanceSquared;
				ClosestStart = PointB;
			}

			if (PointBDistanceSquared < ClosestEndpointDistanceSquared)
			{
				ClosestEndpointDistanceSquared = PointBDistanceSquared;
				ClosestStart = PointA;
			}
		}

		if (ClosestEndpointDistanceSquared >
			FMath::Square(LinkEndpointMatchTolerance))
		{
			return false;
		}

		OutStart = ClosestStart;
		OutEnd = DestPoint;
		return true;
	}
}


bool UEnemyGenNavLinksProxy::OnLinkMoveStarted(class UObject* PathComp, const FVector& DestPoint)
{
	UPathFollowingComponent* PathFollowing =
	   Cast<UPathFollowingComponent>(PathComp);

	if (!PathFollowing)
	{
		return false;
	}

	AAIController* AIController =
		Cast<AAIController>(PathFollowing->GetOwner());

	APawn* Pawn =
		AIController
			? AIController->GetPawn()
			: nullptr;

	if (!Pawn)
	{
		return false;
	}

	UEnemyMovementComponent* Movement =
		Cast<UEnemyMovementComponent>(
			Pawn->GetMovementComponent());

	if (!Movement)
	{
		return false;
	}
	
	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld());
	
	if (!IsValid(NavSystem))
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s: NavSystem is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return false;
	}
	
	INavLinkCustomInterface* CustomLink = NavSystem->GetCustomLink(LinkProxyId);
	
	if (CustomLink == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s: Custom Link is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return false;
	}
	
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	
	if (!GetActiveLinkEndpoints(PathComp, DestPoint, Start, End))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"%s::%s: Failed to resolve generated link endpoints. "
				"Destination=%s LinkProxyId=%llu"),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__),
			*DestPoint.ToCompactString(),
			LinkProxyId.GetId());

		// Crowd path following has already put the agent into its custom-link
		// waiting state before this callback. Release it instead of leaving it
		// blocked on a traversal that cannot be initialized safely.
		PathFollowing->FinishUsingCustomLink(this);
		return false;
	}
	
	Movement->StartNavLinkTraversal(
		DestPoint,
		PathFollowing,
		this, Start, End, LinkTraversalType);

	return true;
}

bool UEnemyGenNavLinksProxy::GetActiveLinkEndpoints(UObject* PathComp, const FVector& DestPoint, FVector& OutStart,
	FVector& OutEnd) const
{
	const UPathFollowingComponent* PathFollowing =
		Cast<UPathFollowingComponent>(PathComp);

	if (!IsValid(PathFollowing))
	{
		return false;
	}

	const FNavPathSharedPtr Path =
		PathFollowing->GetPath();

	if (!Path.IsValid())
	{
		return false;
	}

	const FNavMeshPath* NavMeshPath =
		Path->CastPath<FNavMeshPath>();

	if (NavMeshPath && !NavMeshPath->IsStringPulled())
	{
		const ARecastNavMesh* RecastNavData =
			Cast<ARecastNavMesh>(
				NavMeshPath->GetNavigationDataUsed());

		if (!IsValid(RecastNavData))
		{
			return false;
		}

		return TryGetCorridorLinkEndpoints(
			*NavMeshPath,
			*RecastNavData,
			LinkProxyId,
			DestPoint,
			OutStart,
			OutEnd);
	}

	const TArray<FNavPathPoint>& Points =
		Path->GetPathPoints();

	const int32 CurrentIndex =
		static_cast<int32>(
			PathFollowing->GetCurrentPathIndex());

	const int32 NextIndex =
		static_cast<int32>(
			PathFollowing->GetNextPathIndex());

	if (!Points.IsValidIndex(CurrentIndex))
	{
		return false;
	}

	OutStart = Points[CurrentIndex].Location;
	OutEnd = DestPoint;

	if (Points.IsValidIndex(NextIndex))
	{
		const FVector PathEnd =
			Points[NextIndex].Location;

		UE_CLOG(
			!PathEnd.Equals(DestPoint, LinkEndpointMatchTolerance),
			LogTemp,
			Warning,
			TEXT(
				"Generated link end differs from the path segment end. "
				"PathEnd=%s DestPoint=%s"),
			*PathEnd.ToCompactString(),
			*DestPoint.ToCompactString());
	}

	return true;
}
