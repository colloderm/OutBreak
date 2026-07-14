// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Nav/EnemyGenNavLinksProxy.h"

#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
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

	Movement->StartNavLinkTraversal(
		DestPoint,
		PathFollowing,
		this);

	return true;
}
