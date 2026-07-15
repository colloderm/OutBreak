// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Nav/EnemyGenNavLinksProxy.h"

#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "AI/Components/EnemyMovementComponent.h"


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
		UE_LOG(LogTemp, Error, TEXT("%s::%s: NavSysem is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
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
	
	GetActiveLinkEndpoints(PathComp, DestPoint, Start, End);
	
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

		ensureMsgf(
			PathEnd.Equals(DestPoint, 10.f),
			TEXT(
				"Generated link end mismatch. "
				"PathEnd=%s DestPoint=%s"),
			*PathEnd.ToCompactString(),
			*DestPoint.ToCompactString());
	}

	return true;
}
