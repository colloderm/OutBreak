// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Components/EnemyMovementComponent.h"

#include "InputBehavior.h"
#include "AI/EnemyCharacter.h"
#include "AnimationBudgetAllocator/Public/SkeletalMeshComponentBudgeted.h"
#include "Chaos/Deformable/Utilities.h"
#include "Components/CapsuleComponent.h"
#include "AI/EnemyCharacter.h"
#include "Animation/AnimMontage.h"
#include "MotionWarpingComponent.h"
#include "Kismet/KismetMathLibrary.h"


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
	
	AEnemyCharacter* OwnerCharacter = Cast<AEnemyCharacter>(GetOwner());
	
	if (!IsValid(OwnerCharacter))
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s: Owmer Pawn is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	
	UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
	if (!IsValid(Capsule))
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s: Capsule is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	
	USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh();
	if (!IsValid(Mesh))
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s: Mesh is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	
	Character = OwnerCharacter;
	CapsuleComponent = Capsule;
	SkeletalMeshComponent = Mesh;

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
                                                    UEnemyGenNavLinksProxy* EnemyGenNavLinksProxy, FVector Start, FVector End, ETraversalLinkType LinkType)
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

	const FVector ActorPos = Start;
	const FVector ActorToDestination = TraversalDestination - ActorPos;

	const FVector NormalizedDestination = ActorToDestination.GetSafeNormal();
	const float CurrentToTargetHeight = ActorToDestination.Z;
	const FVector ActorPosXY = ActorPos * FVector(1.f, 1.f, 0.f);
	const FVector DestinationPosXY = TraversalDestination * FVector(1.f, 1.f, 0.f);
	const float LinkSpan =
		FVector::Dist2D(Start, TraversalDestination);;
	
	
	// Parkour ======================================
	switch (LinkType)
	{
	case ETraversalLinkType::Vault:
		BeginTraversalVault(Start, TraversalDestination);
		break;
	case ETraversalLinkType::Mantle:
	case ETraversalLinkType::ClimbUp:
		if (CurrentToTargetHeight < 0.f)
		{
			SetTraversalType(ETraversalType::Drop);
			break;
		}
		else
		{
			
		}
		break;
	case ETraversalLinkType::None:
	default:
		break;
	}
	
	bFallingStart = false;

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
	SetTraversalType(ETraversalType::Walk);
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
	CacheMeshWorldLocation = SkeletalMeshComponent->GetComponentLocation();
	if (!IsFalling())
	{
		// 이전 상태가 떨어짐 이였고.
		if (bFallingStart)
		{
			UWorld* World = GetWorld();
			if (IsValid(World))
			{
				// 3초 후 다시 움직임.
				// 일어서는 애니메이션도 추가해야함.
				// 소멸 시점에 타이머 정리 하도록 추가해야함
				World->GetTimerManager().SetTimer(
					AfterDropToReturnHandle,
					[&]()
				{
					UE_LOG(LogTemp, Display, TEXT("%s::%s: Timer Lamda On"), *GetClass()->GetName(), TEXT(__FUNCTION__));
					if (IsValid(Character))
					{

						if (IsValid(SkeletalMeshComponent) && IsValid(CapsuleComponent))
						{
							SkeletalMeshComponent->SetAllBodiesSimulatePhysics(false);
							SkeletalMeshComponent->SetSimulatePhysics(false);

							SkeletalMeshComponent->SetCollisionEnabled(
								ECollisionEnabled::QueryAndPhysics);
							
							SkeletalMeshComponent->SetWorldLocation(CacheMeshWorldLocation);
							
							SkeletalMeshComponent->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
							SkeletalMeshComponent->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
							
							CapsuleComponent->SetCollisionResponseToChannel(
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
		if (IsValid(Character))
		{
			
			if (IsValid(SkeletalMeshComponent) && IsValid(CapsuleComponent))
			{
				// 모든 Physics Body의 운동량 제거
				SkeletalMeshComponent->SetAllPhysicsLinearVelocity(
					FVector::ZeroVector,
					false);

				SkeletalMeshComponent->SetSimulatePhysics(true);

				SkeletalMeshComponent->SetCollisionEnabled(
					ECollisionEnabled::PhysicsOnly);

				CapsuleComponent->SetCollisionResponseToChannel(
					ECC_Pawn,
					ECollisionResponse::ECR_Ignore);
				
				const FVector ActorPos = GetActorLocation();

				FVector ActorToDestination =
					TraversalDestination - ActorPos;

				ActorToDestination.Z = 0.0f;

				const FVector NormalizedDestination =
					ActorToDestination.GetSafeNormal();
				
				SkeletalMeshComponent->AddImpulse(NormalizedDestination * GetCurrentAcceleration().Length(), NAME_None, true);

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


/* 파쿠르 준비 */
void UEnemyMovementComponent::BeginParkour()
{
	if (!IsValid(CapsuleComponent))
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s: Capsule Component is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	SetMovementMode(MOVE_Flying);
	CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void UEnemyMovementComponent::EndParkour()
{
	if (!IsValid(CapsuleComponent))
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s: Capsule Component is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetMovementMode(MOVE_Walking);
	SetTraversalType(ETraversalType::Walk);
	FinishNavLinkTraversal();
}

void UEnemyMovementComponent::BeginTraversalVault(FVector& Start, FVector& Destination)
{
	if (!ensureAlwaysMsgf(
		IsValid(VaultMontage),
		TEXT("%s::%s: VaultMontage is invalid."),
		*GetClass()->GetName(),
		TEXT(__FUNCTION__)))
	{
		SetTraversalType(ETraversalType::Walk);
		return;	
	}
	
	if (!IsValid(Character))
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s: Owmer Pawn is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	
	if (!IsValid(CapsuleComponent))
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s: Capsule is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	
	if (!IsValid(SkeletalMeshComponent))
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s: Mesh is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	
	UAnimInstance* AnimInstance = SkeletalMeshComponent->GetAnimInstance();
	
	if (!IsValid(AnimInstance))
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s: AnimInstance is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	
	if (AnimInstance->IsAnyMontagePlaying())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::%s: Any Montage is Playing."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	
	UMotionWarpingComponent* MotionWarping = Character->GetMotionWarpingComponent();
	
	const FVector ActorPos = Start;
	
	const FVector DestinationXY = Destination * FVector(1.f,1.f,0.f);
	const FVector ActorPosXY = ActorPos * FVector(1.f,1.f,0.f);
	const FVector ActorToDestinationDifference = DestinationXY - ActorPosXY;
	
	const FRotator VaultRotation = UKismetMathLibrary::FindLookAtRotation(ActorPosXY, DestinationXY);
	
	const FVector WarpTarget1 =  ActorPos;
	const FVector WarpTarget2 =  ActorPos + (ActorToDestinationDifference * 0.5) + FVector(0.f, 0.f, VaultMinHeight);
	const FVector WarpTarget3 =	 Destination;
	
	BeginParkour();
	
	MotionWarping->AddOrUpdateWarpTargetFromLocationAndRotation(
		FName(TEXT("1")),
		WarpTarget1,
		VaultRotation);
	
	MotionWarping->AddOrUpdateWarpTargetFromLocationAndRotation(
		FName(TEXT("2")),
		WarpTarget2,
		VaultRotation);
	
	MotionWarping->AddOrUpdateWarpTargetFromLocationAndRotation(
		FName(TEXT("3")),
		WarpTarget3,
		VaultRotation);
	
	const float MontageDuration = Character->PlayAnimMontage(VaultMontage, VaultPlayRate);
	
	if (!ensureAlwaysMsgf(
		MontageDuration > 0.f,
		TEXT("%s::%s: Failed to play montage : %s"),
		*GetClass()->GetName(),
		TEXT(__FUNCTION__),
		*GetNameSafe(VaultMontage)))
	{
		return;
	}
	
	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(
		this,
		&UEnemyMovementComponent::OnMontageEnded);
	
	// 재생 성공 후 연결해야 활성 Montage Instace에 들어감
	AnimInstance->Montage_SetEndDelegate(
		EndDelegate,
		VaultMontage);
}

void UEnemyMovementComponent::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	EndParkour();
}
