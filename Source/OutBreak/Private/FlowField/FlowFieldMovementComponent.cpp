// Copyright Epic Games, Inc. All Rights Reserved.

#include "FlowField/FlowFieldMovementComponent.h"

#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "FlowField/FlowFieldDensityComponent.h"
#include "FlowField/FlowFieldSubsystem.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY(LogFlowFieldTraversal);

namespace
{
	uint32 GNextStableMovementBudgetId = 1;
	constexpr uint8 FlowMoveDiagnosticNone = 0;
	constexpr uint8 FlowMoveDiagnosticMissingSubsystem = 1;
	constexpr uint8 FlowMoveDiagnosticMissingFlowField = 2;
	constexpr uint8 FlowMoveDiagnosticQueryConstrainedMoveFailed = 3;

	const TCHAR* FlowMoveDiagnosticToString(const uint8 FailureCode)
	{
		switch (FailureCode)
		{
		case FlowMoveDiagnosticMissingSubsystem:
			return TEXT("MissingSubsystem");
		case FlowMoveDiagnosticMissingFlowField:
			return TEXT("MissingFlowField");
		case FlowMoveDiagnosticQueryConstrainedMoveFailed:
			return TEXT("QueryConstrainedMoveFailed");
		default:
			return TEXT("None");
		}
	}

	const TCHAR* NetModeToString(const ENetMode NetMode)
	{
		switch (NetMode)
		{
		case NM_Standalone:
			return TEXT("Standalone");
		case NM_DedicatedServer:
			return TEXT("DedicatedServer");
		case NM_ListenServer:
			return TEXT("ListenServer");
		case NM_Client:
			return TEXT("Client");
		default:
			return TEXT("Unknown");
		}
	}

	const TCHAR* RoleToString(const ENetRole Role)
	{
		switch (Role)
		{
		case ROLE_None:
			return TEXT("None");
		case ROLE_SimulatedProxy:
			return TEXT("SimulatedProxy");
		case ROLE_AutonomousProxy:
			return TEXT("AutonomousProxy");
		case ROLE_Authority:
			return TEXT("Authority");
		default:
			return TEXT("Unknown");
		}
	}

	const TCHAR* FlowFieldTraversalStateToString(const EFlowFieldPawnTraversalState State)
	{
		switch (State)
		{
		case EFlowFieldPawnTraversalState::Normal:
			return TEXT("Normal");
		case EFlowFieldPawnTraversalState::ClimbApproach:
			return TEXT("ClimbApproach");
		case EFlowFieldPawnTraversalState::Climbing:
			return TEXT("Climbing");
		case EFlowFieldPawnTraversalState::ClimbExit:
			return TEXT("ClimbExit");
		case EFlowFieldPawnTraversalState::DropApproach:
			return TEXT("DropApproach");
		case EFlowFieldPawnTraversalState::Dropping:
			return TEXT("Dropping");
		case EFlowFieldPawnTraversalState::DropExit:
			return TEXT("DropExit");
		default:
			return TEXT("Unknown");
		}
	}
}

UFlowFieldMovementComponent::UFlowFieldMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UFlowFieldMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	if (StableMovementBudgetId == 0)
	{
		StableMovementBudgetId = GNextStableMovementBudgetId++;
	}

	if (UpdatedComponent == nullptr)
	{
		if (AActor* Owner = GetOwner())
		{
			SetUpdatedComponent(Owner->GetRootComponent());
		}
	}

	if (DensityComponent == nullptr)
	{
		DensityComponent = GetDensityComponent();
	}

	if (DensityComponent != nullptr)
	{
		AddTickPrerequisiteComponent(DensityComponent);
	}
}

void UFlowFieldMovementComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (ShouldSkipUpdate(DeltaTime) || UpdatedComponent == nullptr)
	{
		return;
	}

	const FVector FrameStartLocation = GetMovementLocation();
	UFlowFieldSubsystem* FlowField = GetFlowFieldSubsystem();
	AgeQueryCache(DeltaTime);

	if (TickActiveTraversal(FlowField, DeltaTime))
	{
		UpdateVelocityFromMovement(FrameStartLocation, DeltaTime);
		return;
	}

	QueryAndBeginNavLinkTraversal();

	if (TickActiveTraversal(FlowField, DeltaTime))
	{
		UpdateVelocityFromMovement(FrameStartLocation, DeltaTime);
		return;
	}

	if (SweepState == EFlowFieldPawnSweepState::SweepUp)
	{
		TickUpSweep(DeltaTime);
		UpdateVelocityFromMovement(FrameStartLocation, DeltaTime);
		return;
	}

	ApplyGravity(DeltaTime);
	TickNormalFlowMovement(FlowField, DeltaTime);
	UpdateVelocityFromMovement(FrameStartLocation, DeltaTime);
}

void UFlowFieldMovementComponent::BeginUpAndForwardSweep(const float UpDistance)
{
	RemainingUpSweepDistance = FMath::Max(0.0f, UpDistance >= 0.0f ? UpDistance : DefaultUpSweepDistance);
	SweepState = RemainingUpSweepDistance > KINDA_SMALL_NUMBER
		? EFlowFieldPawnSweepState::SweepUp
		: EFlowFieldPawnSweepState::SweepForward;
}

void UFlowFieldMovementComponent::EndUpAndForwardSweep()
{
	RemainingUpSweepDistance = 0.0f;
	SweepState = EFlowFieldPawnSweepState::FollowFlow;
}

bool UFlowFieldMovementComponent::QueryDirection(const FVector& WorldLocation, FVector& OutDirection) const
{
	OutDirection = FVector::ZeroVector;

	const UFlowFieldSubsystem* FlowField = GetFlowFieldSubsystem();
	return FlowField != nullptr && FlowField->QueryDirection(WorldLocation, OutDirection);
}

bool UFlowFieldMovementComponent::QueryNodeRef(const FVector& WorldLocation, NavNodeRef& OutNodeRef) const
{
	OutNodeRef = INVALID_NAVNODEREF;

	const UFlowFieldSubsystem* FlowField = GetFlowFieldSubsystem();
	return FlowField != nullptr && FlowField->QueryNodeRef(WorldLocation, OutNodeRef);
}

bool UFlowFieldMovementComponent::QueryConstrainedMove(
	const FVector& WorldLocation,
	const float MaxTravelDistance,
	FVector& OutMoveOffset) const
{
	OutMoveOffset = FVector::ZeroVector;

	const UFlowFieldSubsystem* FlowField = GetFlowFieldSubsystem();
	return FlowField != nullptr && FlowField->QueryConstrainedMove(WorldLocation, MaxTravelDistance, OutMoveOffset);
}

bool UFlowFieldMovementComponent::QueryNavLink(const FVector& WorldLocation, FFlowFieldNavLinkInfo& OutNavLink) const
{
	OutNavLink = FFlowFieldNavLinkInfo();

	const UFlowFieldSubsystem* FlowField = GetFlowFieldSubsystem();
	return FlowField != nullptr && FlowField->QueryNavLink(WorldLocation, OutNavLink);
}

UFlowFieldSubsystem* UFlowFieldMovementComponent::GetFlowFieldSubsystem() const
{
	UWorld* World = GetWorld();
	return World != nullptr ? World->GetSubsystem<UFlowFieldSubsystem>() : nullptr;
}

AActor* UFlowFieldMovementComponent::GetMovementOwner() const
{
	return UpdatedComponent != nullptr ? UpdatedComponent->GetOwner() : GetOwner();
}

FVector UFlowFieldMovementComponent::GetMovementLocation() const
{
	if (UpdatedComponent != nullptr)
	{
		return UpdatedComponent->GetComponentLocation();
	}

	const AActor* Owner = GetOwner();
	return Owner != nullptr ? Owner->GetActorLocation() : FVector::ZeroVector;
}

FRotator UFlowFieldMovementComponent::GetMovementRotation() const
{
	if (UpdatedComponent != nullptr)
	{
		return UpdatedComponent->GetComponentRotation();
	}

	const AActor* Owner = GetOwner();
	return Owner != nullptr ? Owner->GetActorRotation() : FRotator::ZeroRotator;
}

void UFlowFieldMovementComponent::SetMovementRotation(const FRotator& NewRotation)
{
	if (AActor* Owner = GetMovementOwner())
	{
		Owner->SetActorRotation(NewRotation);
	}
}

UCapsuleComponent* UFlowFieldMovementComponent::GetCapsuleComponent() const
{
	return Cast<UCapsuleComponent>(UpdatedComponent);
}

UFlowFieldDensityComponent* UFlowFieldMovementComponent::GetDensityComponent()
{
	if (DensityComponent != nullptr)
	{
		return DensityComponent;
	}

	AActor* Owner = GetOwner();
	return Owner != nullptr ? Owner->FindComponentByClass<UFlowFieldDensityComponent>() : nullptr;
}

const UFlowFieldDensityComponent* UFlowFieldMovementComponent::GetDensityComponent() const
{
	if (DensityComponent != nullptr)
	{
		return DensityComponent;
	}

	const AActor* Owner = GetOwner();
	return Owner != nullptr ? Owner->FindComponentByClass<UFlowFieldDensityComponent>() : nullptr;
}

void UFlowFieldMovementComponent::SetTraversalState(const EFlowFieldPawnTraversalState NewState)
{
	if (TraversalState == NewState)
	{
		return;
	}

	const EFlowFieldPawnTraversalState OldState = TraversalState;
	TraversalState = NewState;

	UE_LOG(
		LogFlowFieldTraversal,
		VeryVerbose,
		TEXT("%s: traversal state changed from %s to %s."),
		*GetNameSafe(GetMovementOwner()),
		FlowFieldTraversalStateToString(OldState),
		FlowFieldTraversalStateToString(NewState));
}

bool UFlowFieldMovementComponent::TickActiveTraversal(UFlowFieldSubsystem* FlowField, const float DeltaSeconds)
{
	switch (TraversalState)
	{
	case EFlowFieldPawnTraversalState::Normal:
		return false;
	case EFlowFieldPawnTraversalState::ClimbApproach:
		SetTraversalState(EFlowFieldPawnTraversalState::Climbing);
		DrawClimbTraversalDebug();
		return true;
	case EFlowFieldPawnTraversalState::Climbing:
		TickClimbTraversal(FlowField, DeltaSeconds);
		DrawClimbTraversalDebug();
		return true;
	case EFlowFieldPawnTraversalState::ClimbExit:
		FinishClimbTraversal(FlowField);
		DrawClimbTraversalDebug();
		return true;
	case EFlowFieldPawnTraversalState::DropApproach:
		SetTraversalState(EFlowFieldPawnTraversalState::Dropping);
		DrawClimbTraversalDebug();
		return true;
	case EFlowFieldPawnTraversalState::Dropping:
		TickDropTraversal(FlowField, DeltaSeconds);
		DrawClimbTraversalDebug();
		return true;
	case EFlowFieldPawnTraversalState::DropExit:
		FinishDropTraversal(FlowField);
		DrawClimbTraversalDebug();
		return true;
	default:
		SetTraversalState(EFlowFieldPawnTraversalState::Normal);
		return false;
	}
}

void UFlowFieldMovementComponent::QueryAndBeginNavLinkTraversal()
{
	if (TraversalState != EFlowFieldPawnTraversalState::Normal
		|| SweepState != EFlowFieldPawnSweepState::FollowFlow)
	{
		return;
	}

	const FVector CurrentLocation = GetMovementLocation();
	NavNodeRef CurrentNodeRef = INVALID_NAVNODEREF;
	if (!QueryNodeRef(CurrentLocation, CurrentNodeRef))
	{
		return;
	}

	if (LastCompletedLinkTargetNodeRef != INVALID_NAVNODEREF)
	{
		if (CurrentNodeRef == LastCompletedLinkTargetNodeRef)
		{
			return;
		}

		LastCompletedLinkStartNodeRef = INVALID_NAVNODEREF;
		LastCompletedLinkTargetNodeRef = INVALID_NAVNODEREF;
	}
	else if (LastCompletedLinkStartNodeRef != INVALID_NAVNODEREF)
	{
		LastCompletedLinkStartNodeRef = INVALID_NAVNODEREF;
	}

	FFlowFieldNavLinkInfo NavLinkInfo;
	if (!QueryNavLink(CurrentLocation, NavLinkInfo))
	{
		return;
	}

	if (CanBeginClimbTraversal(NavLinkInfo, CurrentNodeRef))
	{
		BeginClimbTraversal(NavLinkInfo);
		return;
	}

	if (CanBeginDropTraversal(NavLinkInfo, CurrentNodeRef))
	{
		BeginDropTraversal(NavLinkInfo);
	}
}

bool UFlowFieldMovementComponent::CanBeginClimbTraversal(
	const FFlowFieldNavLinkInfo& NavLinkInfo,
	const NavNodeRef CurrentNodeRef) const
{
	if (TraversalState != EFlowFieldPawnTraversalState::Normal
		|| ClimbSpeed <= KINDA_SMALL_NUMBER
		|| !NavLinkInfo.IsValid()
		|| NavLinkInfo.BehaviorType != EFlowFieldNavBehaviorType::CLIMB)
	{
		return false;
	}

	if (CurrentNodeRef != INVALID_NAVNODEREF && NavLinkInfo.SourceNodeRef != CurrentNodeRef)
	{
		return false;
	}

	if (LastCompletedLinkTargetNodeRef != INVALID_NAVNODEREF
		&& CurrentNodeRef == LastCompletedLinkTargetNodeRef)
	{
		return false;
	}

	const FVector CurrentLocation = GetMovementLocation();
	if (NavLinkInfo.TargetPoint.Z <= CurrentLocation.Z + ClimbMinHeight)
	{
		return false;
	}

	return FVector::DistSquared(CurrentLocation, NavLinkInfo.TargetPoint) > FMath::Square(ClimbAcceptanceRadius);
}

void UFlowFieldMovementComponent::BeginClimbTraversal(const FFlowFieldNavLinkInfo& NavLinkInfo)
{
	if (TraversalState != EFlowFieldPawnTraversalState::Normal)
	{
		UE_LOG(
			LogFlowFieldTraversal,
			VeryVerbose,
			TEXT("%s: skipped CLIMB begin because traversal state is already %s."),
			*GetNameSafe(GetMovementOwner()),
			FlowFieldTraversalStateToString(TraversalState));
		return;
	}

	ActiveNavLink = NavLinkInfo;
	ActiveClimbStartNodeRef = NavLinkInfo.SourceNodeRef;
	ActiveClimbTargetNodeRef = NavLinkInfo.TargetNodeRef;
	ActiveClimbStart = GetMovementLocation();

	const UCapsuleComponent* CapsuleComponent = GetCapsuleComponent();
	const float CapsuleHeightOffset = CapsuleComponent != nullptr ? CapsuleComponent->GetScaledCapsuleHalfHeight() * 2.0f : 0.0f;
	ActiveClimbTarget = NavLinkInfo.TargetPoint + CapsuleHeightOffset;

	if (UCapsuleComponent* MutableCapsuleComponent = GetCapsuleComponent())
	{
		MutableCapsuleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	LastClimbMoveDirection = FVector::ZeroVector;
	ClimbBlockedElapsedTime = 0.0f;
	VerticalVelocity = 0.0f;
	if (UFlowFieldDensityComponent* ResolvedDensityComponent = GetDensityComponent())
	{
		ResolvedDensityComponent->ResetSeparationVelocity();
	}
	SetTraversalState(EFlowFieldPawnTraversalState::Climbing);

	FVector FacingDirection = NavLinkInfo.TargetPoint - NavLinkInfo.StartPoint;
	FacingDirection.Z = 0.0f;
	if (FacingDirection.IsNearlyZero())
	{
		FacingDirection = NavLinkInfo.TargetPoint - GetMovementLocation();
		FacingDirection.Z = 0.0f;
	}
	if (!FacingDirection.IsNearlyZero())
	{
		SetMovementRotation(FRotator(0.0f, FacingDirection.Rotation().Yaw, 0.0f));
	}

	UE_LOG(
		LogFlowFieldTraversal,
		VeryVerbose,
		TEXT("%s: traversal state -> %s. CLIMB source=%llu target=%llu targetPoint=%s"),
		*GetNameSafe(GetMovementOwner()),
		FlowFieldTraversalStateToString(TraversalState),
		static_cast<unsigned long long>(ActiveClimbStartNodeRef),
		static_cast<unsigned long long>(ActiveClimbTargetNodeRef),
		*ActiveClimbTarget.ToString());
}

void UFlowFieldMovementComponent::TickClimbTraversal(UFlowFieldSubsystem* FlowField, const float DeltaSeconds)
{
	if (DeltaSeconds <= 0.0f)
	{
		return;
	}

	if (ClimbSpeed <= KINDA_SMALL_NUMBER)
	{
		AbortClimbTraversal(TEXT("ClimbSpeed is zero"));
		return;
	}

	const FVector CurrentLocation = GetMovementLocation();
	const FVector ToTarget = ActiveClimbTarget - CurrentLocation;
	const float DistanceToTarget = ToTarget.Size();
	if (DistanceToTarget <= ClimbAcceptanceRadius)
	{
		SetTraversalState(EFlowFieldPawnTraversalState::ClimbExit);
		FinishClimbTraversal(FlowField);
		return;
	}

	if (ToTarget.Z < -ClimbAcceptanceRadius)
	{
		AbortClimbTraversal(TEXT("target moved below pawn during CLIMB"));
		return;
	}

	const float MaxStepDistance = ClimbSpeed * DeltaSeconds;
	if (MaxStepDistance <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector ClimbDirection = ToTarget / DistanceToTarget;
	const FVector MoveDelta = ClimbDirection * FMath::Min(MaxStepDistance, DistanceToTarget);

	FVector ActualDelta;
	FHitResult Hit;
	MoveClimbWithSweep(MoveDelta, ActualDelta, Hit);

	LastClimbMoveDirection = ActualDelta.IsNearlyZero() ? ClimbDirection : ActualDelta.GetSafeNormal();

	FVector FacingDirection = ActiveClimbTarget - GetMovementLocation();
	FacingDirection.Z = 0.0f;
	if (!FacingDirection.IsNearlyZero())
	{
		UpdateRotationFromMoveDirection(FacingDirection, DeltaSeconds);
	}

	const float RemainingDistance = FVector::Dist(GetMovementLocation(), ActiveClimbTarget);
	if (RemainingDistance <= ClimbAcceptanceRadius)
	{
		SetTraversalState(EFlowFieldPawnTraversalState::ClimbExit);
		FinishClimbTraversal(FlowField);
		return;
	}

	const float ProgressTowardTarget = DistanceToTarget - RemainingDistance;
	const bool bMadeUsefulProgress = ProgressTowardTarget > FMath::Max(1.0f, MaxStepDistance * 0.1f);
	ClimbBlockedElapsedTime = bMadeUsefulProgress ? 0.0f : ClimbBlockedElapsedTime + DeltaSeconds;

	if (ClimbBlockedTimeout > 0.0f && ClimbBlockedElapsedTime >= ClimbBlockedTimeout)
	{
		AbortClimbTraversal(Hit.bBlockingHit ? TEXT("CLIMB sweep remained blocked") : TEXT("CLIMB made no progress"));
	}
}

void UFlowFieldMovementComponent::FinishClimbTraversal(UFlowFieldSubsystem* FlowField)
{
	NavNodeRef ProjectedTargetNodeRef = INVALID_NAVNODEREF;
	if (FlowField != nullptr)
	{
		FlowField->QueryNodeRef(ActiveClimbTarget, ProjectedTargetNodeRef);
	}

	if (ProjectedTargetNodeRef != INVALID_NAVNODEREF
		&& ActiveClimbTargetNodeRef != INVALID_NAVNODEREF
		&& ProjectedTargetNodeRef != ActiveClimbTargetNodeRef)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("%s: CLIMB target projected to node %llu, expected %llu."),
			*GetNameSafe(GetMovementOwner()),
			static_cast<unsigned long long>(ProjectedTargetNodeRef),
			static_cast<unsigned long long>(ActiveClimbTargetNodeRef));
	}

	LastCompletedLinkStartNodeRef = ActiveClimbStartNodeRef;
	LastCompletedLinkTargetNodeRef = ActiveClimbTargetNodeRef;
	VerticalVelocity = 0.0f;

	UE_LOG(
		LogFlowFieldTraversal,
		VeryVerbose,
		TEXT("%s: traversal state -> Normal. Completed CLIMB source=%llu target=%llu"),
		*GetNameSafe(GetMovementOwner()),
		static_cast<unsigned long long>(LastCompletedLinkStartNodeRef),
		static_cast<unsigned long long>(LastCompletedLinkTargetNodeRef));

	ClearActiveClimbTraversal();
}

void UFlowFieldMovementComponent::AbortClimbTraversal(const TCHAR* Reason)
{
	LastCompletedLinkStartNodeRef = ActiveClimbStartNodeRef;
	LastCompletedLinkTargetNodeRef = ActiveClimbTargetNodeRef;
	VerticalVelocity = 0.0f;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("%s: traversal state -> Normal. Aborted CLIMB source=%llu target=%llu reason=%s"),
		*GetNameSafe(GetMovementOwner()),
		static_cast<unsigned long long>(ActiveClimbStartNodeRef),
		static_cast<unsigned long long>(ActiveClimbTargetNodeRef),
		Reason != nullptr ? Reason : TEXT("unknown"));

	ClearActiveClimbTraversal();
}

bool UFlowFieldMovementComponent::CanBeginDropTraversal(
	const FFlowFieldNavLinkInfo& NavLinkInfo,
	const NavNodeRef CurrentNodeRef) const
{
	if (TraversalState != EFlowFieldPawnTraversalState::Normal
		|| !NavLinkInfo.IsValid()
		|| NavLinkInfo.BehaviorType != EFlowFieldNavBehaviorType::DROP)
	{
		return false;
	}

	if (CurrentNodeRef != INVALID_NAVNODEREF && NavLinkInfo.SourceNodeRef != CurrentNodeRef)
	{
		return false;
	}

	if (LastCompletedLinkTargetNodeRef != INVALID_NAVNODEREF
		&& CurrentNodeRef == LastCompletedLinkTargetNodeRef)
	{
		return false;
	}

	const FVector CurrentLocation = GetMovementLocation();
	const bool bNeedsHorizontalMove = (CurrentLocation - NavLinkInfo.TargetPoint).SizeSquared2D() > FMath::Square(DropHorizontalAcceptanceRadius);
	const bool bNeedsVerticalDrop = NavLinkInfo.TargetPoint.Z < CurrentLocation.Z - KINDA_SMALL_NUMBER;

	if (bNeedsHorizontalMove && DropHorizontalSpeed <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	return bNeedsHorizontalMove || bNeedsVerticalDrop;
}

void UFlowFieldMovementComponent::BeginDropTraversal(const FFlowFieldNavLinkInfo& NavLinkInfo)
{
	if (TraversalState != EFlowFieldPawnTraversalState::Normal)
	{
		UE_LOG(
			LogFlowFieldTraversal,
			VeryVerbose,
			TEXT("%s: skipped DROP begin because traversal state is already %s."),
			*GetNameSafe(GetMovementOwner()),
			FlowFieldTraversalStateToString(TraversalState));
		return;
	}

	ActiveNavLink = NavLinkInfo;
	ActiveClimbStartNodeRef = NavLinkInfo.SourceNodeRef;
	ActiveClimbTargetNodeRef = NavLinkInfo.TargetNodeRef;
	ActiveClimbStart = GetMovementLocation();
	ActiveClimbTarget = NavLinkInfo.TargetPoint;
	LastClimbMoveDirection = FVector::ZeroVector;
	ClimbBlockedElapsedTime = 0.0f;
	VerticalVelocity = FMath::Min(VerticalVelocity, 0.0f);
	if (UFlowFieldDensityComponent* ResolvedDensityComponent = GetDensityComponent())
	{
		ResolvedDensityComponent->ResetSeparationVelocity();
	}
	bDropHasLeftGround = false;
	SetTraversalState(EFlowFieldPawnTraversalState::Dropping);
	if (UCapsuleComponent* CapsuleComponent = GetCapsuleComponent())
	{
		CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	FVector FacingDirection = NavLinkInfo.TargetPoint - NavLinkInfo.StartPoint;
	FacingDirection.Z = 0.0f;
	if (FacingDirection.IsNearlyZero())
	{
		FacingDirection = NavLinkInfo.TargetPoint - GetMovementLocation();
		FacingDirection.Z = 0.0f;
	}
	if (!FacingDirection.IsNearlyZero())
	{
		SetMovementRotation(FRotator(0.0f, FacingDirection.Rotation().Yaw, 0.0f));
	}

	UE_LOG(
		LogFlowFieldTraversal,
		VeryVerbose,
		TEXT("%s: traversal state -> %s. DROP source=%llu target=%llu targetPoint=%s"),
		*GetNameSafe(GetMovementOwner()),
		FlowFieldTraversalStateToString(TraversalState),
		static_cast<unsigned long long>(ActiveClimbStartNodeRef),
		static_cast<unsigned long long>(ActiveClimbTargetNodeRef),
		*ActiveClimbTarget.ToString());
}

void UFlowFieldMovementComponent::TickDropTraversal(UFlowFieldSubsystem* FlowField, const float DeltaSeconds)
{
	if (DeltaSeconds <= 0.0f)
	{
		return;
	}

	if (!ActiveNavLink.IsValid())
	{
		AbortDropTraversal(TEXT("invalid DROP link"));
		return;
	}

	ApplyGravity(DeltaSeconds);

	const float AcceptanceRadius = FMath::Max(0.0f, DropHorizontalAcceptanceRadius);
	const FVector FrameStartLocation = GetMovementLocation();
	FVector ToTarget2D = ActiveClimbTarget - FrameStartLocation;
	ToTarget2D.Z = 0.0f;

	FVector HorizontalMoveOffset = FVector::ZeroVector;
	const float DistanceToTarget2D = ToTarget2D.Size();
	if (DistanceToTarget2D > AcceptanceRadius && DropHorizontalSpeed > KINDA_SMALL_NUMBER)
	{
		HorizontalMoveOffset = ToTarget2D / DistanceToTarget2D * FMath::Min(DropHorizontalSpeed * DeltaSeconds, DistanceToTarget2D);
	}

	auto IsWalkableGroundHit = [this](const FHitResult& Hit)
	{
		return Hit.bBlockingHit && Hit.ImpactNormal.Z >= WalkableFloorZ;
	};

	bool bTouchedGround = false;
	if (!HorizontalMoveOffset.IsNearlyZero())
	{
		FHitResult HorizontalHit;
		const FVector BeforeHorizontalMove = GetMovementLocation();
		MoveByOffset(HorizontalMoveOffset, true, &HorizontalHit);
		if (HorizontalHit.bBlockingHit)
		{
			UpdateVerticalVelocityFromHit(HorizontalHit);
			bTouchedGround = bTouchedGround || IsWalkableGroundHit(HorizontalHit);
		}

		FVector ActualHorizontalMove = GetMovementLocation() - BeforeHorizontalMove;
		ActualHorizontalMove.Z = 0.0f;
		LastClimbMoveDirection = ActualHorizontalMove.IsNearlyZero()
			? ToTarget2D.GetSafeNormal()
			: ActualHorizontalMove.GetSafeNormal();
	}
	else
	{
		LastClimbMoveDirection = FVector::ZeroVector;
	}

	const FVector VerticalMoveOffset = FVector::UpVector * VerticalVelocity * DeltaSeconds;
	if (!VerticalMoveOffset.IsNearlyZero())
	{
		FHitResult VerticalHit;
		MoveByOffset(VerticalMoveOffset, true, &VerticalHit);
		if (VerticalHit.bBlockingHit)
		{
			UpdateVerticalVelocityFromHit(VerticalHit);
			bTouchedGround = bTouchedGround || IsWalkableGroundHit(VerticalHit);
		}
		else if (VerticalVelocity < 0.0f)
		{
			bDropHasLeftGround = true;
		}
	}

	FVector FacingDirection = ActiveClimbTarget - GetMovementLocation();
	FacingDirection.Z = 0.0f;
	UpdateRotationFromMoveDirection(FacingDirection, DeltaSeconds);

	const bool bTargetIsBelowStart = ActiveClimbTarget.Z < ActiveClimbStart.Z - 5.0f;
	const bool bPawnDroppedBelowStart = GetMovementLocation().Z < ActiveClimbStart.Z - 5.0f;
	const bool bNearTarget2D = (GetMovementLocation() - ActiveClimbTarget).SizeSquared2D() <= FMath::Square(AcceptanceRadius);
	const bool bCanFinishOnGround = bDropHasLeftGround || bPawnDroppedBelowStart || (!bTargetIsBelowStart && bNearTarget2D);
	if (bTouchedGround && bCanFinishOnGround)
	{
		SetTraversalState(EFlowFieldPawnTraversalState::DropExit);
		FinishDropTraversal(FlowField);
	}
}

void UFlowFieldMovementComponent::FinishDropTraversal(UFlowFieldSubsystem* FlowField)
{
	NavNodeRef ProjectedTargetNodeRef = INVALID_NAVNODEREF;
	if (FlowField != nullptr)
	{
		FlowField->QueryNodeRef(GetMovementLocation(), ProjectedTargetNodeRef);
	}

	if (ProjectedTargetNodeRef != INVALID_NAVNODEREF
		&& ActiveClimbTargetNodeRef != INVALID_NAVNODEREF
		&& ProjectedTargetNodeRef != ActiveClimbTargetNodeRef)
	{
		UE_LOG(
			LogTemp,
			Verbose,
			TEXT("%s: DROP landed on node %llu, expected %llu."),
			*GetNameSafe(GetMovementOwner()),
			static_cast<unsigned long long>(ProjectedTargetNodeRef),
			static_cast<unsigned long long>(ActiveClimbTargetNodeRef));
	}

	LastCompletedLinkStartNodeRef = ActiveClimbStartNodeRef;
	LastCompletedLinkTargetNodeRef = ActiveClimbTargetNodeRef;
	VerticalVelocity = 0.0f;

	UE_LOG(
		LogFlowFieldTraversal,
		VeryVerbose,
		TEXT("%s: traversal state -> Normal. Completed DROP source=%llu target=%llu"),
		*GetNameSafe(GetMovementOwner()),
		static_cast<unsigned long long>(LastCompletedLinkStartNodeRef),
		static_cast<unsigned long long>(LastCompletedLinkTargetNodeRef));

	ClearActiveClimbTraversal();
}

void UFlowFieldMovementComponent::AbortDropTraversal(const TCHAR* Reason)
{
	LastCompletedLinkStartNodeRef = ActiveClimbStartNodeRef;
	LastCompletedLinkTargetNodeRef = ActiveClimbTargetNodeRef;
	VerticalVelocity = 0.0f;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("%s: traversal state -> Normal. Aborted DROP source=%llu target=%llu reason=%s"),
		*GetNameSafe(GetMovementOwner()),
		static_cast<unsigned long long>(ActiveClimbStartNodeRef),
		static_cast<unsigned long long>(ActiveClimbTargetNodeRef),
		Reason != nullptr ? Reason : TEXT("unknown"));

	ClearActiveClimbTraversal();
}

void UFlowFieldMovementComponent::ClearActiveClimbTraversal()
{
	ActiveNavLink = FFlowFieldNavLinkInfo();
	ActiveClimbStartNodeRef = INVALID_NAVNODEREF;
	ActiveClimbTargetNodeRef = INVALID_NAVNODEREF;
	ActiveClimbStart = FVector::ZeroVector;
	ActiveClimbTarget = FVector::ZeroVector;
	LastClimbMoveDirection = FVector::ZeroVector;
	ClimbBlockedElapsedTime = 0.0f;
	bDropHasLeftGround = false;
	SetTraversalState(EFlowFieldPawnTraversalState::Normal);
	if (UCapsuleComponent* CapsuleComponent = GetCapsuleComponent())
	{
		CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
}

bool UFlowFieldMovementComponent::MoveClimbWithSweep(
	const FVector& DesiredOffset,
	FVector& OutActualDelta,
	FHitResult& OutHit)
{
	OutActualDelta = FVector::ZeroVector;
	OutHit = FHitResult();

	if (DesiredOffset.IsNearlyZero())
	{
		return false;
	}

	const FVector StartLocation = GetMovementLocation();
	FHitResult PrimaryHit;
	MoveByOffset(DesiredOffset, true, &PrimaryHit);
	OutHit = PrimaryHit;

	if (PrimaryHit.bBlockingHit)
	{
		const FVector RemainingOffset = DesiredOffset * FMath::Clamp(1.0f - PrimaryHit.Time, 0.0f, 1.0f);
		const FVector SlideOffset = FVector::VectorPlaneProject(RemainingOffset, PrimaryHit.Normal);
		if (!SlideOffset.IsNearlyZero())
		{
			FHitResult SlideHit;
			MoveByOffset(SlideOffset, true, &SlideHit);
			if (SlideHit.bBlockingHit)
			{
				OutHit = SlideHit;
			}
		}
	}

	OutActualDelta = GetMovementLocation() - StartLocation;
	return !OutActualDelta.IsNearlyZero();
}

void UFlowFieldMovementComponent::DrawClimbTraversalDebug() const
{
	if (!bDrawClimbTraversalDebug || GetWorld() == nullptr || TraversalState == EFlowFieldPawnTraversalState::Normal)
	{
		return;
	}

	const FVector PawnLocation = GetMovementLocation();
	DrawDebugSphere(GetWorld(), ActiveNavLink.StartPoint, 20.0f, 12, FColor::Blue, false, 0.0f, 0, 1.5f);
	DrawDebugSphere(GetWorld(), ActiveClimbTarget, ClimbAcceptanceRadius, 16, FColor::Green, false, 0.0f, 0, 1.5f);
	DrawDebugLine(GetWorld(), ActiveNavLink.StartPoint, ActiveClimbTarget, FColor::Green, false, 0.0f, 0, 2.0f);

	if (!LastClimbMoveDirection.IsNearlyZero())
	{
		DrawDebugDirectionalArrow(GetWorld(), PawnLocation, PawnLocation + LastClimbMoveDirection * 120.0f, 18.0f, FColor::Yellow, false, 0.0f, 0, 2.0f);
	}

	const FString DebugText = FString::Printf(
		TEXT("%s\nSource=%llu Target=%llu"),
		FlowFieldTraversalStateToString(TraversalState),
		static_cast<unsigned long long>(ActiveClimbStartNodeRef),
		static_cast<unsigned long long>(ActiveClimbTargetNodeRef));
	DrawDebugString(GetWorld(), PawnLocation + FVector(0.0f, 0.0f, 120.0f), DebugText, nullptr, FColor::White, 0.0f, true);
}

void UFlowFieldMovementComponent::AgeQueryCache(const float DeltaSeconds)
{
	if (DeltaSeconds <= 0.0f)
	{
		return;
	}

	QueryCache.FlowQueryAgeSeconds += DeltaSeconds;
	QueryCache.ConstrainedMoveAgeSeconds += DeltaSeconds;
}

bool UFlowFieldMovementComponent::ShouldRefreshFlowQuery() const
{
	if (!QueryCache.bHasValidFlowDirection)
	{
		return true;
	}

	if (MaxCachedFlowQueryAgeSeconds > 0.0f
		&& QueryCache.FlowQueryAgeSeconds >= MaxCachedFlowQueryAgeSeconds)
	{
		return true;
	}

	const int32 QueryInterval = FMath::Max(1, FlowQueryIntervalFrames);
	if (!bEnableFlowQueryThrottling || QueryInterval <= 1 || StableMovementBudgetId == 0)
	{
		return true;
	}

	return (GFrameCounter + StableMovementBudgetId) % static_cast<uint64>(QueryInterval) == 0;
}

bool UFlowFieldMovementComponent::TryBuildFreshFlowMove(
	UFlowFieldSubsystem* FlowField,
	const float DeltaSeconds,
	FVector& OutMoveOffset)
{
	OutMoveOffset = FVector::ZeroVector;
	if (FlowField == nullptr)
	{
		LogFlowMoveDiagnostic(FlowMoveDiagnosticMissingSubsystem, FlowField, GetMovementLocation(), 0.0f);
		return false;
	}

	if (DeltaSeconds <= 0.0f || !ShouldRefreshFlowQuery())
	{
		return false;
	}

	const float MaxTravelDistance = MoveSpeed * DeltaSeconds;
	if (MaxTravelDistance <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector CurrentLocation = GetMovementLocation();
	if (!FlowField->HasFlowField())
	{
		LogFlowMoveDiagnostic(FlowMoveDiagnosticMissingFlowField, FlowField, CurrentLocation, MaxTravelDistance);
		return false;
	}

	if (FlowField->QueryConstrainedMove(CurrentLocation, MaxTravelDistance, OutMoveOffset)
		&& !OutMoveOffset.IsNearlyZero())
	{
		ClearFlowMoveDiagnostic();
		return true;
	}

	// Query failure does not clear the cache. A short projection miss or budget
	// gap should not freeze the agent if the last direction is still fresh.
	LogFlowMoveDiagnostic(FlowMoveDiagnosticQueryConstrainedMoveFailed, FlowField, CurrentLocation, MaxTravelDistance);
	return false;
}

bool UFlowFieldMovementComponent::TryBuildCachedFlowMove(
	const float DeltaSeconds,
	FVector& OutMoveOffset) const
{
	OutMoveOffset = FVector::ZeroVector;
	if (DeltaSeconds <= 0.0f)
	{
		return false;
	}

	const bool bCacheExpired = MaxCachedFlowQueryAgeSeconds > 0.0f
		&& QueryCache.FlowQueryAgeSeconds > MaxCachedFlowQueryAgeSeconds;
	if (bCacheExpired)
	{
		return false;
	}

	if (QueryCache.bHasConstrainedMoveOffset && !QueryCache.LastConstrainedMoveOffset.IsNearlyZero())
	{
		OutMoveOffset = QueryCache.LastConstrainedMoveOffset.GetSafeNormal2D() * MoveSpeed * DeltaSeconds;
		return true;
	}

	if (QueryCache.bHasValidFlowDirection && !QueryCache.LastValidFlowDirection.IsNearlyZero())
	{
		OutMoveOffset = QueryCache.LastValidFlowDirection.GetSafeNormal2D() * MoveSpeed * DeltaSeconds;
		return true;
	}

	if (!QueryCache.LastFinalMoveDirection.IsNearlyZero())
	{
		OutMoveOffset = QueryCache.LastFinalMoveDirection.GetSafeNormal2D() * MoveSpeed * DeltaSeconds;
		return true;
	}

	return false;
}

void UFlowFieldMovementComponent::StoreFlowMoveResult(const FVector& MoveOffset)
{
	if (MoveOffset.IsNearlyZero())
	{
		return;
	}

	const FVector MoveDirection = MoveOffset.GetSafeNormal2D();
	if (MoveDirection.IsNearlyZero())
	{
		return;
	}

	QueryCache.LastValidFlowDirection = MoveDirection;
	QueryCache.LastConstrainedMoveOffset = MoveOffset;
	QueryCache.LastFinalMoveDirection = MoveDirection;
	QueryCache.FlowQueryAgeSeconds = 0.0f;
	QueryCache.ConstrainedMoveAgeSeconds = 0.0f;
	QueryCache.bHasValidFlowDirection = true;
	QueryCache.bHasConstrainedMoveOffset = true;
}

void UFlowFieldMovementComponent::LogFlowMoveDiagnostic(
	const uint8 FailureCode,
	UFlowFieldSubsystem* FlowField,
	const FVector& WorldLocation,
	const float MaxTravelDistance)
{
	if (!IsFlowFieldNetworkDiagnosticsEnabled())
	{
		return;
	}

	UWorld* World = GetWorld();
	const float Now = World != nullptr ? World->GetTimeSeconds() : 0.0f;
	const bool bStateChanged = LastMoveDiagnosticCode != FailureCode;
	if (!bStateChanged && LastMoveDiagnosticLogTime >= 0.0f && Now - LastMoveDiagnosticLogTime < 2.0f)
	{
		return;
	}

	LastMoveDiagnosticCode = FailureCode;
	LastMoveDiagnosticLogTime = Now;

	NavNodeRef NodeRef = INVALID_NAVNODEREF;
	FVector Direction = FVector::ZeroVector;
	FVector ConstrainedMoveOffset = FVector::ZeroVector;
	const bool bHasNodeRef = FlowField != nullptr && FlowField->QueryNodeRef(WorldLocation, NodeRef);
	const bool bHasDirection = FlowField != nullptr && FlowField->QueryDirection(WorldLocation, Direction);
	const bool bHasConstrainedMove = FlowField != nullptr && FlowField->QueryConstrainedMove(WorldLocation, MaxTravelDistance, ConstrainedMoveOffset);
	const AActor* MovementOwner = GetMovementOwner();

	UE_LOG(
		LogFlowField,
		Warning,
		TEXT("Flow move diagnostic. Actor=%s NetMode=%s LocalRole=%s Failure=%s FlowFieldSubsystem=%s HasFlowField=%s QueryNodeRef=%s NodeRef=%llu QueryDirection=%s QueryConstrainedMove=%s OutMoveOffset=%s UpdatedComponent=%s ComponentTick=%s Location=%s MaxTravelDistance=%.2f"),
		*GetNameSafe(MovementOwner),
		NetModeToString(World != nullptr ? World->GetNetMode() : NM_MAX),
		RoleToString(MovementOwner != nullptr ? MovementOwner->GetLocalRole() : ROLE_None),
		FlowMoveDiagnosticToString(FailureCode),
		FlowField != nullptr ? TEXT("true") : TEXT("false"),
		FlowField != nullptr && FlowField->HasFlowField() ? TEXT("true") : TEXT("false"),
		bHasNodeRef ? TEXT("true") : TEXT("false"),
		static_cast<unsigned long long>(NodeRef),
		bHasDirection ? TEXT("true") : TEXT("false"),
		bHasConstrainedMove ? TEXT("true") : TEXT("false"),
		*ConstrainedMoveOffset.ToCompactString(),
		*GetNameSafe(UpdatedComponent),
		PrimaryComponentTick.IsTickFunctionEnabled() ? TEXT("true") : TEXT("false"),
		*WorldLocation.ToCompactString(),
		MaxTravelDistance);
}

void UFlowFieldMovementComponent::ClearFlowMoveDiagnostic()
{
	LastMoveDiagnosticCode = FlowMoveDiagnosticNone;
	LastMoveDiagnosticLogTime = -1.0f;
}

void UFlowFieldMovementComponent::TickNormalFlowMovement(UFlowFieldSubsystem* FlowField, const float DeltaSeconds)
{
	if (SweepState == EFlowFieldPawnSweepState::SweepUp)
	{
		TickUpSweep(DeltaSeconds);
		return;
	}

	if (SweepState == EFlowFieldPawnSweepState::SweepForward)
	{
		FVector FlowDirection;
		if (bUseFlowDirectionForForwardSweep && FlowField != nullptr)
		{
			if (ShouldRefreshFlowQuery())
			{
				QueryDirection(GetMovementLocation(), FlowDirection);
				if (!FlowDirection.IsNearlyZero())
				{
					QueryCache.LastValidFlowDirection = FlowDirection.GetSafeNormal2D();
					QueryCache.FlowQueryAgeSeconds = 0.0f;
					QueryCache.bHasValidFlowDirection = true;
				}
			}
			if (FlowDirection.IsNearlyZero() && QueryCache.bHasValidFlowDirection)
			{
				FlowDirection = QueryCache.LastValidFlowDirection;
			}
		}
		TickForwardSweep(FlowDirection, DeltaSeconds);
		return;
	}

	FVector HorizontalMoveOffset = FVector::ZeroVector;
	if (const UFlowFieldDensityComponent* ResolvedDensityComponent = GetDensityComponent())
	{
		HorizontalMoveOffset = ResolvedDensityComponent->GetSeparationVelocity() * DeltaSeconds;
	}

	if (MoveSpeed > 0.0f)
	{
		FVector FlowMoveOffset;
		const bool bHasFreshMove = TryBuildFreshFlowMove(FlowField, DeltaSeconds, FlowMoveOffset);
		if (!bHasFreshMove)
		{
			TryBuildCachedFlowMove(DeltaSeconds, FlowMoveOffset);
		}

		if (!FlowMoveOffset.IsNearlyZero())
		{
			HorizontalMoveOffset += FlowMoveOffset;
			if (bHasFreshMove)
			{
				StoreFlowMoveResult(FlowMoveOffset);
			}
			else
			{
				QueryCache.LastFinalMoveDirection = FlowMoveOffset.GetSafeNormal2D();
			}
		}
	}

	UpdateRotationFromMoveDirection(HorizontalMoveOffset, DeltaSeconds);
	const FVector GravityMoveOffset = FVector::UpVector * VerticalVelocity * DeltaSeconds;
	MoveWithWallSweep(HorizontalMoveOffset + GravityMoveOffset);
}

void UFlowFieldMovementComponent::MoveWithWallSweep(const FVector& DesiredOffset, const bool bForceSweep)
{
	if (DesiredOffset.IsNearlyZero())
	{
		return;
	}

	const bool bShouldSweep = bSweepMovement || bForceSweep;
	if (!bShouldSweep)
	{
		MoveByOffset(DesiredOffset, false);
		return;
	}

	FHitResult Hit;
	MoveByOffset(DesiredOffset, true, &Hit);
	if (!Hit.bBlockingHit)
	{
		return;
	}

	UpdateVerticalVelocityFromHit(Hit);
	const FVector RemainingOffset = DesiredOffset * FMath::Clamp(1.0f - Hit.Time, 0.0f, 1.0f);
	const FVector SlideOffset = FVector::VectorPlaneProject(RemainingOffset, Hit.Normal);
	if (!SlideOffset.IsNearlyZero())
	{
		FHitResult SlideHit;
		MoveByOffset(SlideOffset, true, &SlideHit);
		if (SlideHit.bBlockingHit)
		{
			UpdateVerticalVelocityFromHit(SlideHit);
		}
	}
}

void UFlowFieldMovementComponent::TickUpSweep(const float DeltaSeconds)
{
	if (RemainingUpSweepDistance <= KINDA_SMALL_NUMBER)
	{
		SweepState = EFlowFieldPawnSweepState::SweepForward;
		return;
	}

	const float UpStep = FMath::Min(UpSweepSpeed * DeltaSeconds, RemainingUpSweepDistance);
	if (UpStep <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	FHitResult Hit;
	MoveByOffset(FVector::UpVector * UpStep, true, &Hit);
	RemainingUpSweepDistance -= UpStep * (Hit.bBlockingHit ? Hit.Time : 1.0f);

	if (Hit.bBlockingHit)
	{
		EndUpAndForwardSweep();
		return;
	}

	if (RemainingUpSweepDistance <= KINDA_SMALL_NUMBER)
	{
		SweepState = EFlowFieldPawnSweepState::SweepForward;
	}
}

void UFlowFieldMovementComponent::TickForwardSweep(const FVector& FlowDirection, const float DeltaSeconds)
{
	const AActor* MovementOwner = GetMovementOwner();
	FVector ForwardDirection = bUseFlowDirectionForForwardSweep
		? FlowDirection
		: (MovementOwner != nullptr ? MovementOwner->GetActorForwardVector() : FVector::ZeroVector);
	ForwardDirection = ForwardDirection.GetSafeNormal2D();

	FVector HorizontalMoveOffset = ForwardDirection * MoveSpeed * DeltaSeconds;
	if (const UFlowFieldDensityComponent* ResolvedDensityComponent = GetDensityComponent())
	{
		HorizontalMoveOffset += ResolvedDensityComponent->GetSeparationVelocity() * DeltaSeconds;
	}

	UpdateRotationFromMoveDirection(HorizontalMoveOffset, DeltaSeconds);
	MoveWithWallSweep(HorizontalMoveOffset + FVector::UpVector * VerticalVelocity * DeltaSeconds, true);
}

void UFlowFieldMovementComponent::ApplyGravity(const float DeltaSeconds)
{
	if (!bEnableGravity)
	{
		VerticalVelocity = 0.0f;
		return;
	}

	if (const UWorld* World = GetWorld())
	{
		VerticalVelocity += World->GetGravityZ() * GravityScale * DeltaSeconds;
		VerticalVelocity = FMath::Max(VerticalVelocity, -MaxFallSpeed);
	}
}

void UFlowFieldMovementComponent::UpdateVerticalVelocityFromHit(const FHitResult& Hit)
{
	if (Hit.ImpactNormal.Z >= WalkableFloorZ && VerticalVelocity < 0.0f)
	{
		VerticalVelocity = 0.0f;
	}
	else if (Hit.ImpactNormal.Z <= -WalkableFloorZ && VerticalVelocity > 0.0f)
	{
		VerticalVelocity = 0.0f;
	}
}

void UFlowFieldMovementComponent::UpdateRotationFromMoveDirection(const FVector& MoveDirection, const float DeltaSeconds)
{
	if (!bOrientRotationToMovement || MoveDirection.IsNearlyZero())
	{
		return;
	}

	const float TargetYaw = MoveDirection.GetSafeNormal2D().Rotation().Yaw;
	const FRotator TargetRotation(0.0f, TargetYaw, 0.0f);
	const FRotator NewRotation = RotationInterpolationSpeed > 0.0f
		? FMath::RInterpTo(GetMovementRotation(), TargetRotation, DeltaSeconds, RotationInterpolationSpeed)
		: TargetRotation;
	SetMovementRotation(NewRotation);
}

void UFlowFieldMovementComponent::UpdateVelocityFromMovement(const FVector& StartLocation, const float DeltaSeconds)
{
	Velocity = DeltaSeconds > SMALL_NUMBER
		? (GetMovementLocation() - StartLocation) / DeltaSeconds
		: FVector::ZeroVector;
	UpdateComponentVelocity();
}

bool UFlowFieldMovementComponent::MoveByOffset(const FVector& Offset, const bool bSweep, FHitResult* OutHit)
{
	if (Offset.IsNearlyZero())
	{
		return false;
	}

	if (UpdatedComponent != nullptr)
	{
		return MoveUpdatedComponent(Offset, UpdatedComponent->GetComponentQuat(), bSweep, OutHit);
	}

	if (AActor* Owner = GetOwner())
	{
		Owner->AddActorWorldOffset(Offset, bSweep, OutHit);
		return true;
	}

	return false;
}
