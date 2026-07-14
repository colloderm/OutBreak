// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AI/Nav/EnemyGenNavLinksProxy.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "EnemyMovementComponent.generated.h"




UENUM(Blueprintable)
enum class ETraversalType : uint8
{
	Walk UMETA(DisplayName="Walk"),
	Drop UMETA(DisplayName="Drop"),
	Vault UMETA(DisplayName="Vault"),
	Mantle UMETA(DisplayName="Mantle"),
	ClimbUp UMETA(DisplayName="ClimbUp"),
};



UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class OUTBREAK_API UEnemyMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UEnemyMovementComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Traversal")
	ETraversalType TraversalType = ETraversalType::Walk;
	/* Traversal Values */
	
	/* Vault X:75, Z:110 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Traversal")
	float VaultMinHeight = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Traversal")
	float VaultMaxHeight = 112.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Traversal")
	float MantleMinHeight = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Traversal")
	float MantleMaxHeight = 240.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Traversal")
	float ClimbUpMinHeight = 360.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Traversal")
	float ClimbUpMaxHeight = 370.f;

	UPROPERTY()
	FVector TraversalDestination = FVector::ZeroVector;

	UPROPERTY()
	TObjectPtr<class UPathFollowingComponent> ActivePathFollowing = nullptr;

	UPROPERTY()
	TObjectPtr<class UEnemyGenNavLinksProxy> ActiveCustomLink = nullptr;

	UPROPERTY()
	bool bIsTraversingNavLink = false;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
							   FActorComponentTickFunction* ThisTickFunction) override;
	virtual void RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed) override;
	virtual void RequestPathMove(const FVector& MoveInput) override;

	bool IsTraversingNavLink() const { return bIsTraversingNavLink; }

	void StartNavLinkTraversal(const FVector& Destination, UPathFollowingComponent* PathFollowing, UEnemyGenNavLinksProxy* EnemyGenNavLinksProxy);
	void FinishNavLinkTraversal();

	void TickTraversalDrop();
	void TickTraversalVault();
	void TickTraversalMantle();
	void TickTraversalClimbUp();
	// Called every frame

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

private:
	/* Falling Traversal */
	bool bFallingStart = false;
	FTransform LatestRelativeMeshTransform = FTransform::Identity;
	FTimerHandle AfterDropToReturnHandle;
};
