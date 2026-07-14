// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Components/EnemyMovementComponent.h"

#include "AI/EnemyCharacter.h"
#include "AnimationBudgetAllocator/Public/SkeletalMeshComponentBudgeted.h"
#include "Chaos/Deformable/Utilities.h"
#include "Components/CapsuleComponent.h"

DEFINE_LOG_CATEGORY_STATIC(
	LogEnemyDropTraversal,
	Log,
	All);

// Sets default values for this component's properties
UEnemyMovementComponent::UEnemyMovementComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UEnemyMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
}

// Called every frame
void UEnemyMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                            FActorComponentTickFunction* ThisTickFunction)
{
	if (bIsTraversingNavLink)
	{
		switch (TraversalType)
		{
		case ETraversalType::Drop:
			TickTraversalDrop();
			break;

		case ETraversalType::Vault:
			TickTraversalVault();
			break;

		case ETraversalType::Mantle:
			TickTraversalMantle();
			break;

		case ETraversalType::ClimbUp:
			TickTraversalClimbUp();
			break;

		case ETraversalType::Walk:
		default:
			break;
		}
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UEnemyMovementComponent::StartNavLinkTraversal(const FVector& Destination, UPathFollowingComponent* PathFollowing,
                                                    UEnemyGenNavLinksProxy* EnemyGenNavLinksProxy)
{
	if (!IsValid(PathFollowing) ||
		!IsValid(EnemyGenNavLinksProxy))
	{
		return;
	}

	if (bIsTraversingNavLink)
	{
		if (ActivePathFollowing == PathFollowing &&
			ActiveCustomLink == EnemyGenNavLinksProxy)
		{
			UE_LOG(
				LogEnemyDropTraversal,
				VeryVerbose,
				TEXT(
					"%s::%s ignored duplicate nav link start. "
					"Actor=%s Destination=%s TraversalType=%s"),
				*GetClass()->GetName(),
				TEXT(__FUNCTION__),
				*GetNameSafe(GetOwner()),
				*Destination.ToCompactString(),
				*UEnum::GetValueAsString(TraversalType));

			return;
		}

		FinishNavLinkTraversal();
	}

	TraversalDestination = Destination;
	ActivePathFollowing = PathFollowing;
	ActiveCustomLink = EnemyGenNavLinksProxy;

	const FVector ActorPos = GetActorLocation();
	const FVector ActorToDestination = TraversalDestination - ActorPos;

	const FVector NormalizedDestination = ActorToDestination.GetSafeNormal();
	const float CurrentToTargetHeight = ActorToDestination.Z;
	// Parkour ======================================
	if (NormalizedDestination.Z < 0.f)
	{
		TraversalType = ETraversalType::Drop;
	}
	else if (NormalizedDestination.Z > 0.f)
	{
		if (CurrentToTargetHeight >= VaultMinHeight && CurrentToTargetHeight <= VaultMaxHeight) /* Vault */
		{
			TraversalType = ETraversalType::Vault;
		}
		else if (CurrentToTargetHeight >= MantleMinHeight && CurrentToTargetHeight <= MantleMaxHeight) /* Mantle */
		{
			TraversalType = ETraversalType::Mantle;
		}
		else if (CurrentToTargetHeight >= ClimbUpMinHeight && CurrentToTargetHeight <= ClimbUpMaxHeight) /* ClimbUp */
		{
			TraversalType = ETraversalType::ClimbUp;
		}
		else
		{
			TraversalType = ETraversalType::Walk;
		}
	}
	else
	{
		TraversalType = ETraversalType::Walk;
	}

	bFallingStart = false;
	LatestRelativeMeshTransform = FTransform::Identity;

	StopMovementImmediately();
	bIsTraversingNavLink = true;

	UE_LOG(
		LogEnemyDropTraversal,
		VeryVerbose,
		TEXT(
			"%s::%s started. Actor=%s Destination=%s "
			"TraversalType=%s PathFollowing=%s Link=%s"),
		*GetClass()->GetName(),
		TEXT(__FUNCTION__),
		*GetNameSafe(GetOwner()),
		*TraversalDestination.ToCompactString(),
		*UEnum::GetValueAsString(TraversalType),
		*GetNameSafe(ActivePathFollowing),
		*GetNameSafe(ActiveCustomLink));

	// ===================================================
}

void UEnemyMovementComponent::FinishNavLinkTraversal()
{
	if (!bIsTraversingNavLink &&
		ActivePathFollowing == nullptr &&
		ActiveCustomLink == nullptr)
	{
		return;
	}

	UPathFollowingComponent* PathFollowing =
		ActivePathFollowing;

	UEnemyGenNavLinksProxy* CustomLink =
		ActiveCustomLink;

	bIsTraversingNavLink = false;
	TraversalType = ETraversalType::Walk;
	TraversalDestination = FVector::ZeroVector;
	ActivePathFollowing = nullptr;
	ActiveCustomLink = nullptr;

	if (IsValid(PathFollowing) &&
		IsValid(CustomLink))
	{
		PathFollowing->FinishUsingCustomLink(CustomLink);
	}
}

void UEnemyMovementComponent::RequestDirectMove(
	const FVector& MoveVelocity,
	const bool bForceMaxSpeed)
{
	if (bIsTraversingNavLink)
	{
		UE_LOG(
			LogEnemyDropTraversal,
			VeryVerbose,
			TEXT(
				"%s::%s ignored path-following direct move during traversal. "
				"Actor=%s RequestedVelocity=%s CurrentVelocity=%s TraversalType=%s"),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__),
			*GetNameSafe(GetOwner()),
			*MoveVelocity.ToCompactString(),
			*Velocity.ToCompactString(),
			*UEnum::GetValueAsString(TraversalType));

		return;
	}

	Super::RequestDirectMove(MoveVelocity, bForceMaxSpeed);
}

void UEnemyMovementComponent::RequestPathMove(const FVector& MoveInput)
{
	if (bIsTraversingNavLink)
	{
		UE_LOG(
			LogEnemyDropTraversal,
			VeryVerbose,
			TEXT(
				"%s::%s ignored path-following path move during traversal. "
				"Actor=%s MoveInput=%s CurrentVelocity=%s TraversalType=%s"),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__),
			*GetNameSafe(GetOwner()),
			*MoveInput.ToCompactString(),
			*Velocity.ToCompactString(),
			*UEnum::GetValueAsString(TraversalType));

		return;
	}

	Super::RequestPathMove(MoveInput);
}

void UEnemyMovementComponent::TickTraversalDrop()
{
	if (!IsFalling())
	{
		if (bFallingStart)
		{
			UWorld* World = GetWorld();
			if (IsValid(World))
			{
				// 소멸 시점에 타이머 정리 하도록 추가해야함
				World->GetTimerManager().SetTimer(
					AfterDropToReturnHandle,
					[&]()
				{
					UE_LOG(LogTemp, Display, TEXT("%s::%s: Timer Lamda On"), *GetClass()->GetName(), TEXT(__FUNCTION__));
					AEnemyCharacter* OwnerPawn = Cast<AEnemyCharacter>(GetOwner());
					if (IsValid(OwnerPawn))
					{
						USkeletalMeshComponentBudgeted* Mesh =
							Cast<USkeletalMeshComponentBudgeted>(
								OwnerPawn->GetMesh());

						UCapsuleComponent* Capsule =
							OwnerPawn->GetCapsuleComponent();

						if (IsValid(Mesh) && IsValid(Capsule))
						{
							Mesh->SetAllBodiesSimulatePhysics(false);
							Mesh->SetSimulatePhysics(false);

							Mesh->SetCollisionEnabled(
								ECollisionEnabled::NoCollision);
							
							const FVector MeshWorldLocation = Mesh->GetComponentLocation();

							Capsule->SetWorldLocation(MeshWorldLocation);
							
							Mesh->SetRelativeTransform(
								LatestRelativeMeshTransform);
							
							Capsule->SetCollisionResponseToChannel(
								ECC_Pawn,
								ECR_Block);
							
							FinishNavLinkTraversal();
						}
					}
				},3.f,false);
			}
			bFallingStart = false;
			return;
		}

		const FVector ActorPos =
			GetActorLocation();

		FVector ActorToDestination =
			TraversalDestination - ActorPos;

		ActorToDestination.Z = 0.0f;

		const FVector NormalizedDestination =
			ActorToDestination.GetSafeNormal();

		if (NormalizedDestination.IsNearlyZero())
		{
			return;
		}

		AddInputVector(
			NormalizedDestination, true);

		return;
	}

	if (!bFallingStart)
	{
		AEnemyCharacter* OwnerPawn =
			Cast<AEnemyCharacter>(GetOwner());

		if (IsValid(OwnerPawn))
		{
			USkeletalMeshComponentBudgeted* Mesh =
				Cast<USkeletalMeshComponentBudgeted>(
					OwnerPawn->GetMesh());

			UCapsuleComponent* Capsule =
				OwnerPawn->GetCapsuleComponent();

			if (IsValid(Mesh) && IsValid(Capsule))
			{
				LatestRelativeMeshTransform =
					Mesh->GetRelativeTransform();

				Mesh->SetSimulatePhysics(true);

				Mesh->SetCollisionEnabled(
					ECollisionEnabled::PhysicsOnly);

				Capsule->SetCollisionResponseToChannel(
					ECC_Pawn,
					ECollisionResponse::ECR_Ignore);
				
				const FVector ActorPos = GetActorLocation();

				FVector ActorToDestination =
					TraversalDestination - ActorPos;

				ActorToDestination.Z = 0.0f;

				const FVector NormalizedDestination =
					ActorToDestination.GetSafeNormal();
				
				Mesh->AddImpulse(NormalizedDestination * GetCurrentAcceleration().Length(), NAME_None, true);

				bFallingStart = true;
			}
		}
	}
}


void UEnemyMovementComponent::TickTraversalVault()
{
}

void UEnemyMovementComponent::TickTraversalMantle()
{
}

void UEnemyMovementComponent::TickTraversalClimbUp()
{
}
