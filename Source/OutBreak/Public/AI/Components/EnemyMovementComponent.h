// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AI/Nav/EnemyGenNavLinksProxy.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "AI/Struct/EnemyTraversalData.h"
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
	
	float GetVaultSpan() const { return VaultSpan; }
	float GetVaultMinHeight() const { return VaultMinHeight; }
	float GetVaultMaxHeight() const { return VaultMaxHeight; }
	float GetMantleMinHeight() const { return MantleMinHeight; }
	float GetMantleMaxHeight() const { return MantleMaxHeight; }
	float GetClimbUpMinHeight() const { return ClimbUpMinHeight; }
	float GetClimbUpMaxHeight() const { return ClimbUpMaxHeight; }

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Traversal")
	ETraversalType TraversalType = ETraversalType::Walk;
	
	/* Traversal Values */
	
	/* Vault X:75, Z:110~112 (cm.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Traversal|Vault")
	float VaultSpan = 75.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Traversal|Vault")
	float VaultMinHeight = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Traversal|Vault")
	float VaultMaxHeight = 112.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Traversal|Vault|Animation")
	TObjectPtr<UAnimMontage> VaultMontage;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Traversal|Vault|Animation")
	float VaultPlayRate = 0.7f;

	/* Mantle */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Traversal")
	float MantleMinHeight = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Traversal")
	float MantleMaxHeight = 240.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Traversal|Mantle|Animation")
	TObjectPtr<UAnimMontage> MantleMontage;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Traversal|Mantle|Animation")
	float MantlePlayRate = 0.7f;
	
	/* Climb Up */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Traversal|ClimbUp")
	float ClimbUpMinHeight = 360.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Traversal|ClimbUp")
	float ClimbUpMaxHeight = 370.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Traversal|ClimbUp|Animation")
	TObjectPtr<UAnimMontage> ClimbUpMontage;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Traversal|ClimbUp|Animation")
	float ClimbUpPlayRate = 0.7f;
	
	

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

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
							   FActorComponentTickFunction* ThisTickFunction) override;
	virtual void RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed) override;
	virtual void RequestPathMove(const FVector& MoveInput) override;

	bool IsTraversingNavLink() const { return bIsTraversingNavLink; }

	void StartNavLinkTraversal(const FVector& Destination, UPathFollowingComponent* PathFollowing, UEnemyGenNavLinksProxy* EnemyGenNavLinksProxy, FVector
	                           Start, FVector End, ETraversalLinkType LinkType);
	void FinishNavLinkTraversal();

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
	
	
};
