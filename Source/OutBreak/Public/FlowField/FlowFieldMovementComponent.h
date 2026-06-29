// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AI/Navigation/NavigationTypes.h"
#include "Struct/FlowFieldTypes.h"
#include "GameFramework/PawnMovementComponent.h"
#include "FlowFieldMovementComponent.generated.h"

class UCapsuleComponent;
class UFlowFieldDensityComponent;
class UFlowFieldSubsystem;
class AActor;

DECLARE_LOG_CATEGORY_EXTERN(LogFlowFieldTraversal, Log, All);

UENUM(BlueprintType)
enum class EFlowFieldPawnSweepState : uint8
{
	FollowFlow,
	SweepUp,
	SweepForward,
};

UENUM(BlueprintType)
enum class EFlowFieldPawnTraversalState : uint8
{
	Normal,
	ClimbApproach,
	Climbing,
	ClimbExit,
	DropApproach,
	Dropping,
	DropExit,
};

USTRUCT()
struct OUTBREAK_API FFlowFieldMovementQueryCache
{
	GENERATED_BODY()

	/** Last successful raw Flow Field direction. Kept through short query gaps/failures. */
	FVector LastValidFlowDirection = FVector::ZeroVector;

	/** Last NavMesh-constrained movement offset produced by QueryConstrainedMove. */
	FVector LastConstrainedMoveOffset = FVector::ZeroVector;

	/** Last horizontal movement direction that actually drove normal movement. */
	FVector LastFinalMoveDirection = FVector::ZeroVector;

	float FlowQueryAgeSeconds = 0.0f;
	float ConstrainedMoveAgeSeconds = 0.0f;

	bool bHasValidFlowDirection = false;
	bool bHasConstrainedMoveOffset = false;
};

UCLASS(ClassGroup = (FlowField), meta = (BlueprintSpawnableComponent))
class OUTBREAK_API UFlowFieldMovementComponent : public UPawnMovementComponent
{
	GENERATED_BODY()

public:
	UFlowFieldMovementComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Movement", meta = (ClampMin = "0.0"))
	float MoveSpeed = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Movement")
	bool bSweepMovement = true;

	/** Enables frame-phased Flow queries. Movement still applies every frame by using QueryCache. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Movement Budget")
	bool bEnableFlowQueryThrottling = false;

	/** Per-agent query cadence. 1 keeps the old every-frame query behavior. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Movement Budget", meta = (ClampMin = "1", ClampMax = "120"))
	int32 FlowQueryIntervalFrames = 1;

	/** Cached flow data older than this is ignored so stale goals do not drive agents forever. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Movement Budget", meta = (ClampMin = "0.0"))
	float MaxCachedFlowQueryAgeSeconds = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Gravity")
	bool bEnableGravity = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Gravity", meta = (ClampMin = "0.0"))
	float GravityScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Gravity", meta = (ClampMin = "0.0"))
	float MaxFallSpeed = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Gravity", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WalkableFloorZ = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Rotation")
	bool bOrientRotationToMovement = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Rotation", meta = (ClampMin = "0.0"))
	float RotationInterpolationSpeed = 10.0f;

	/** Current special traversal state. SweepUp and SweepForward always use collision sweeps. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlowField|Traversal")
	EFlowFieldPawnSweepState SweepState = EFlowFieldPawnSweepState::FollowFlow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Traversal", meta = (ClampMin = "0.0"))
	float DefaultUpSweepDistance = 144.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Traversal", meta = (ClampMin = "0.0"))
	float UpSweepSpeed = 300.0f;

	/** When false, SweepForward uses the Pawn's facing direction instead of the Flow Field direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Traversal")
	bool bUseFlowDirectionForForwardSweep = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlowField|Nav Link")
	EFlowFieldPawnTraversalState TraversalState = EFlowFieldPawnTraversalState::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Nav Link|Climb", meta = (ClampMin = "0.0"))
	float ClimbSpeed = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Nav Link|Climb", meta = (ClampMin = "0.0"))
	float ClimbAcceptanceRadius = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Nav Link|Climb", meta = (ClampMin = "0.0"))
	float ClimbMinHeight = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Nav Link|Climb", meta = (ClampMin = "0.0"))
	float ClimbBlockedTimeout = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Nav Link|Drop", meta = (ClampMin = "0.0"))
	float DropHorizontalSpeed = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Nav Link|Drop", meta = (ClampMin = "0.0"))
	float DropHorizontalAcceptanceRadius = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Nav Link|Debug")
	bool bDrawClimbTraversalDebug = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlowField|Density")
	TObjectPtr<UFlowFieldDensityComponent> DensityComponent;

	/** Begins a wall/link traversal: sweep upward first, then sweep only forward until ended. */
	UFUNCTION(BlueprintCallable, Category = "FlowField|Traversal")
	void BeginUpAndForwardSweep(float UpDistance = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "FlowField|Traversal")
	void EndUpAndForwardSweep();

	bool QueryDirection(const FVector& WorldLocation, FVector& OutDirection) const;
	bool QueryNodeRef(const FVector& WorldLocation, NavNodeRef& OutNodeRef) const;
	bool QueryConstrainedMove(const FVector& WorldLocation, float MaxTravelDistance, FVector& OutMoveOffset) const;
	bool QueryNavLink(const FVector& WorldLocation, FFlowFieldNavLinkInfo& OutNavLink) const;

private:
	UFlowFieldSubsystem* GetFlowFieldSubsystem() const;
	AActor* GetMovementOwner() const;
	FVector GetMovementLocation() const;
	FRotator GetMovementRotation() const;
	void SetMovementRotation(const FRotator& NewRotation);
	UCapsuleComponent* GetCapsuleComponent() const;
	UFlowFieldDensityComponent* GetDensityComponent();
	const UFlowFieldDensityComponent* GetDensityComponent() const;

	void SetTraversalState(EFlowFieldPawnTraversalState NewState);
	bool TickActiveTraversal(UFlowFieldSubsystem* FlowField, float DeltaSeconds);
	void QueryAndBeginNavLinkTraversal();
	bool CanBeginClimbTraversal(const FFlowFieldNavLinkInfo& NavLinkInfo, NavNodeRef CurrentNodeRef) const;
	void BeginClimbTraversal(const FFlowFieldNavLinkInfo& NavLinkInfo);
	void TickClimbTraversal(UFlowFieldSubsystem* FlowField, float DeltaSeconds);
	void FinishClimbTraversal(UFlowFieldSubsystem* FlowField);
	void AbortClimbTraversal(const TCHAR* Reason);
	bool CanBeginDropTraversal(const FFlowFieldNavLinkInfo& NavLinkInfo, NavNodeRef CurrentNodeRef) const;
	void BeginDropTraversal(const FFlowFieldNavLinkInfo& NavLinkInfo);
	void TickDropTraversal(UFlowFieldSubsystem* FlowField, float DeltaSeconds);
	void FinishDropTraversal(UFlowFieldSubsystem* FlowField);
	void AbortDropTraversal(const TCHAR* Reason);
	void ClearActiveClimbTraversal();
	bool MoveClimbWithSweep(const FVector& DesiredOffset, FVector& OutActualDelta, FHitResult& OutHit);
	void DrawClimbTraversalDebug() const;
	void TickNormalFlowMovement(UFlowFieldSubsystem* FlowField, float DeltaSeconds);
	void AgeQueryCache(float DeltaSeconds);
	bool ShouldRefreshFlowQuery() const;
	bool TryBuildFreshFlowMove(UFlowFieldSubsystem* FlowField, float DeltaSeconds, FVector& OutMoveOffset);
	bool TryBuildCachedFlowMove(float DeltaSeconds, FVector& OutMoveOffset) const;
	void StoreFlowMoveResult(const FVector& MoveOffset);
	void LogFlowMoveDiagnostic(uint8 FailureCode, UFlowFieldSubsystem* FlowField, const FVector& WorldLocation, float MaxTravelDistance);
	void ClearFlowMoveDiagnostic();
	void MoveWithWallSweep(const FVector& DesiredOffset, bool bForceSweep = false);
	void TickUpSweep(float DeltaSeconds);
	void TickForwardSweep(const FVector& FlowDirection, float DeltaSeconds);
	void ApplyGravity(float DeltaSeconds);
	void UpdateVerticalVelocityFromHit(const FHitResult& Hit);
	void UpdateRotationFromMoveDirection(const FVector& MoveDirection, float DeltaSeconds);
	void UpdateVelocityFromMovement(const FVector& StartLocation, float DeltaSeconds);
	bool MoveByOffset(const FVector& Offset, bool bSweep, FHitResult* OutHit = nullptr);

	FFlowFieldNavLinkInfo ActiveNavLink;
	NavNodeRef ActiveClimbStartNodeRef = INVALID_NAVNODEREF;
	NavNodeRef ActiveClimbTargetNodeRef = INVALID_NAVNODEREF;
	NavNodeRef LastCompletedLinkStartNodeRef = INVALID_NAVNODEREF;
	NavNodeRef LastCompletedLinkTargetNodeRef = INVALID_NAVNODEREF;
	FVector ActiveClimbStart = FVector::ZeroVector;
	FVector ActiveClimbTarget = FVector::ZeroVector;
	FVector LastClimbMoveDirection = FVector::ZeroVector;
	FFlowFieldMovementQueryCache QueryCache;
	uint32 StableMovementBudgetId = 0;
	float RemainingUpSweepDistance = 0.0f;
	float ClimbBlockedElapsedTime = 0.0f;
	float VerticalVelocity = 0.0f;
	float LastMoveDiagnosticLogTime = -1.0f;
	uint8 LastMoveDiagnosticCode = 0;
	bool bDropHasLeftGround = false;
};
