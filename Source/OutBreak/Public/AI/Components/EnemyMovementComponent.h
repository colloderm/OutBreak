// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AI/Interface/EnemyComponentInterface.h"
#include "AI/Nav/EnemyGenNavLinksProxy.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "AI/Struct/EnemyTraversalData.h"
#include "AI/Interface/EnemyComponentInterface.h"
#include "AI/Data/EnemyState.h"
#include "EnemyMovementComponent.generated.h"


class UAnimMontage;

class AEnemyCharacter;
class UCapsuleComponent;
class USkeletalMeshComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class OUTBREAK_API UEnemyMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UEnemyMovementComponent();
	
	ETraversalType GetTraversalType() const { return TraversalType; }
	

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Traversal")
	ETraversalType TraversalType = ETraversalType::Walk;
	

	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Traversal|Debug")
	bool bTraversalDrawDebug;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Traversal|Debug")
	float DrawTime = 10.f;
	
	

	UPROPERTY()
	FVector TraversalDestination = FVector::ZeroVector;

	UPROPERTY()
	TObjectPtr<class UPathFollowingComponent> ActivePathFollowing = nullptr;

	UPROPERTY()
	TObjectPtr<class UEnemyGenNavLinksProxy> ActiveCustomLink = nullptr;

	UPROPERTY()
	bool bIsTraversingNavLink = false;
	
	UPROPERTY()
	FVector CacheMeshWorldLocation = FVector::ZeroVector;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Asset", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnemyAsset> EnemyAsset = nullptr;
	
public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
							   FActorComponentTickFunction* ThisTickFunction) override;
	virtual void RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed) override;
	virtual void RequestPathMove(const FVector& MoveInput) override;

	bool IsTraversingNavLink() const { return bIsTraversingNavLink; }

	void StartNavLinkTraversal(const FVector& Destination, UPathFollowingComponent* PathFollowing, UEnemyGenNavLinksProxy* EnemyGenNavLinksProxy, FVector
	                           Start, FVector End, ETraversalLinkType LinkType);
	void FinishNavLinkTraversal();
	
	void SetLocomotationState(ELocomotionWalkRunState State);
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Movement|State")
	ELocomotionWalkRunState GetLocomotionState() { return WalkingRunState; }

	
private:
	void TickTraversalDrop();
	void TickTraversalVault();
	void TickTraversalMantle();
	void TickTraversalClimbUp();
	
	
	void BeginParkour();
	void EndParkour();
	void BeginTraversalVault(FVector& Start, FVector& Destination);
	void BegineTraversalClimbUp(FVector& Start, FVector& Destination);

protected:
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Movement|Rotation")
	float RotationRateDegrees = 360.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Movement|Rotation",
		meta = (ClampMin = "0.0"))
	float MinimumRotationSpeed = 5.0f;

	void SetTraversalType(ETraversalType NewTraversalType)
	{
		TraversalType = NewTraversalType;
	}
private:
	/* Falling Traversal */
	bool bFallingStart = false;
	FTimerHandle AfterDropToReturnHandle;
	
	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	
	UPROPERTY(Transient)
	TObjectPtr<AEnemyCharacter> Character;
	
	UPROPERTY(Transient)
	TObjectPtr<UCapsuleComponent> CapsuleComponent;
	
	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent;
	
	UPROPERTY(Transient)
	ELocomotionWalkRunState WalkingRunState = ELocomotionWalkRunState::Walking;
	
};
