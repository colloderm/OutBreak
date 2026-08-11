#include "AI/Components/EnemySpawnableComponent.h"

#include "AI/Components/EnemyPhysicalComponent.h"
#include "AI/EnemyCharacter.h"
#include "AI/EnemyController.h"
#include "AI/Spawning/ZombieDirectorWorldSubsystem.h"
#include "Animation/AnimInstance.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UEnemySpawnableComponent::UEnemySpawnableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UEnemySpawnableComponent::BeginPlay()
{
	Super::BeginPlay();

	CaptureCollisionSnapshot();

	if (AActor* Owner = GetOwner(); Owner && Owner->HasAuthority())
	{
		if (UZombieDirectorWorldSubsystem* Director =
			Owner->GetWorld()->GetSubsystem<UZombieDirectorWorldSubsystem>())
		{
			Director->RegisterEnemy(this);
		}
	}
}

void UEnemySpawnableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PresentationTimerHandle);
		World->GetTimerManager().ClearTimer(PoolReturnTimerHandle);

		if (UZombieDirectorWorldSubsystem* Director =
			World->GetSubsystem<UZombieDirectorWorldSubsystem>())
		{
			Director->UnregisterEnemy(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UEnemySpawnableComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UEnemySpawnableComponent, SpawnState);
	DOREPLIFETIME(UEnemySpawnableComponent, ActiveSpawnMontage);
}

void UEnemySpawnableComponent::InitializeAsPooled(const FName InPoolKey)
{
	AEnemyCharacter* Enemy = GetEnemyCharacter();
	if (!IsValid(Enemy) || !Enemy->HasAuthority())
	{
		return;
	}

	SpawnState.PoolKey = InPoolKey;
	SpawnState.SectorId = NAME_None;
	PendingNoiseLocation = FVector::ZeroVector;
	ActiveSpawnMontage = nullptr;
	Enemy->PrepareForPoolStorage();
	SuspendOwner();
	SetSpawnPhase(EEnemyPoolPhase::InactivePooled);
	Enemy->SetActorHiddenInGame(true);
	Enemy->SetNetDormancy(DORM_DormantAll);
	Enemy->ForceNetUpdate();
}

void UEnemySpawnableComponent::ReserveForActivation(
	const FName InPoolKey,
	const FName InSectorId)
{
	AEnemyCharacter* Enemy = GetEnemyCharacter();
	if (!IsValid(Enemy) || !Enemy->HasAuthority())
	{
		return;
	}

	Enemy->FlushNetDormancy();
	++SpawnState.ActivationId;
	SpawnState.PoolKey = InPoolKey;
	SpawnState.SectorId = InSectorId;
	SpawnState.PresentationStartServerTime = 0.0;
	SpawnState.PresentationDuration = 0.0f;
	SetSpawnPhase(EEnemyPoolPhase::Reserved);
	SuspendOwner();
}

void UEnemySpawnableComponent::BeginSpawnPresentation(
	const FTransform& SpawnTransform,
	UAnimMontage* SpawnMontage,
	const float PresentationDuration,
	const FEnemyNoiseEvent& NoiseEvent)
{
	AEnemyCharacter* Enemy = GetEnemyCharacter();
	if (!IsValid(Enemy) || !Enemy->HasAuthority() ||
		SpawnState.Phase != EEnemyPoolPhase::Reserved)
	{
		return;
	}

	Enemy->FlushNetDormancy();
	Enemy->SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
	Enemy->ResetForPoolActivation();
	Enemy->SetActorHiddenInGame(false);
	PendingNoiseLocation = NoiseEvent.Location;
	ActiveSpawnMontage = SpawnMontage;
	SpawnState.PresentationStartServerTime = GetServerTimeSeconds();
	SpawnState.PresentationDuration = FMath::Max(0.0f, PresentationDuration);
	DisableOwnerCollision();
	SuspendOwner();
	SetSpawnPhase(EEnemyPoolPhase::Emerging);
	Enemy->ForceNetUpdate();

	const int32 ExpectedActivationId = SpawnState.ActivationId;
	if (SpawnState.PresentationDuration <= KINDA_SMALL_NUMBER)
	{
		FinishSpawnPresentationServer(ExpectedActivationId);
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
		PresentationTimerHandle,
		FTimerDelegate::CreateUObject(
			this,
			&UEnemySpawnableComponent::FinishSpawnPresentationServer,
			ExpectedActivationId),
		SpawnState.PresentationDuration,
		false);
}

void UEnemySpawnableComponent::ScheduleReturnToPool(const float Delay)
{
	AEnemyCharacter* Enemy = GetEnemyCharacter();
	if (!IsValid(Enemy) || !Enemy->HasAuthority())
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(PresentationTimerHandle);
	SetSpawnPhase(EEnemyPoolPhase::Dying);

	if (Delay <= KINDA_SMALL_NUMBER)
	{
		ReturnToPoolNow();
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
		PoolReturnTimerHandle,
		this,
		&UEnemySpawnableComponent::ReturnToPoolNow,
		Delay,
		false);
}

void UEnemySpawnableComponent::ReturnToPoolNow()
{
	AEnemyCharacter* Enemy = GetEnemyCharacter();
	if (!IsValid(Enemy) || !Enemy->HasAuthority() ||
		SpawnState.Phase == EEnemyPoolPhase::InactivePooled)
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(PresentationTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(PoolReturnTimerHandle);
	++SpawnState.ActivationId;
	PendingNoiseLocation = FVector::ZeroVector;
	ActiveSpawnMontage = nullptr;
	Enemy->PrepareForPoolStorage();
	SuspendOwner();
	SpawnState.SectorId = NAME_None;
	SetSpawnPhase(EEnemyPoolPhase::InactivePooled);
	Enemy->SetActorHiddenInGame(true);
	Enemy->ForceNetUpdate();
	Enemy->SetNetDormancy(DORM_DormantAll);

	if (UZombieDirectorWorldSubsystem* Director =
		GetWorld()->GetSubsystem<UZombieDirectorWorldSubsystem>())
	{
		Director->ReturnEnemyToPool(Enemy, SpawnState.PoolKey);
	}
}

void UEnemySpawnableComponent::CommandInvestigateNoise(
	const FVector& NoiseLocation)
{
	AEnemyCharacter* Enemy = GetEnemyCharacter();
	if (!IsValid(Enemy) || !Enemy->HasAuthority() ||
		SpawnState.Phase != EEnemyPoolPhase::Active)
	{
		return;
	}

	PendingNoiseLocation = NoiseLocation;
	if (AEnemyController* Controller = Cast<AEnemyController>(Enemy->GetController()))
	{
		Controller->InvestigateNoise(NoiseLocation);
	}
}

void UEnemySpawnableComponent::NotifySpawnPresentationReady()
{
	FinishSpawnPresentationServer(SpawnState.ActivationId);
}

bool UEnemySpawnableComponent::IsBudgeted() const
{
	return SpawnState.Phase == EEnemyPoolPhase::Reserved ||
		SpawnState.Phase == EEnemyPoolPhase::Emerging ||
		SpawnState.Phase == EEnemyPoolPhase::Active;
}

void UEnemySpawnableComponent::CaptureCollisionSnapshot()
{
	if (bCollisionSnapshotCaptured)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}

	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(Owner);
	for (UPrimitiveComponent* Primitive : PrimitiveComponents)
	{
		if (!IsValid(Primitive))
		{
			continue;
		}

		FPrimitiveCollisionSnapshot& Snapshot = CollisionSnapshots.AddDefaulted_GetRef();
		Snapshot.Component = Primitive;
		Snapshot.CollisionEnabled = Primitive->GetCollisionEnabled();
		Snapshot.ObjectType = Primitive->GetCollisionObjectType();
		Snapshot.Responses = Primitive->GetCollisionResponseToChannels();
		Snapshot.RelativeTransform = Primitive->GetRelativeTransform();
		Snapshot.bGenerateOverlapEvents = Primitive->GetGenerateOverlapEvents();
		Snapshot.bSimulatePhysics = Primitive->IsSimulatingPhysics();
	}

	bCollisionSnapshotCaptured = true;
}

void UEnemySpawnableComponent::DisableOwnerCollision()
{
	CaptureCollisionSnapshot();
	for (const FPrimitiveCollisionSnapshot& Snapshot : CollisionSnapshots)
	{
		if (UPrimitiveComponent* Primitive = Snapshot.Component.Get())
		{
			Primitive->SetGenerateOverlapEvents(false);
			Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
}

void UEnemySpawnableComponent::RestoreCollisionSnapshot()
{
	for (const FPrimitiveCollisionSnapshot& Snapshot : CollisionSnapshots)
	{
		if (UPrimitiveComponent* Primitive = Snapshot.Component.Get())
		{
			Primitive->SetRelativeTransform(Snapshot.RelativeTransform);
			Primitive->SetCollisionObjectType(Snapshot.ObjectType);
			Primitive->SetCollisionResponseToChannels(Snapshot.Responses);
			Primitive->SetGenerateOverlapEvents(Snapshot.bGenerateOverlapEvents);
			Primitive->SetCollisionEnabled(Snapshot.CollisionEnabled);
			if (Snapshot.bSimulatePhysics)
			{
				Primitive->SetSimulatePhysics(true);
			}
		}
	}
}

void UEnemySpawnableComponent::SuspendOwner()
{
	DisableOwnerCollision();

	if (AEnemyCharacter* Enemy = GetEnemyCharacter())
	{
		if (UCharacterMovementComponent* Movement = Enemy->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
			Movement->DisableMovement();
		}

		if (AEnemyController* Controller = Cast<AEnemyController>(Enemy->GetController()))
		{
			Controller->SuspendForPool();
		}
	}
}

void UEnemySpawnableComponent::ResumeOwner()
{
	AEnemyCharacter* Enemy = GetEnemyCharacter();
	if (!IsValid(Enemy))
	{
		return;
	}

	RestoreCollisionSnapshot();
	if (UCharacterMovementComponent* Movement = Enemy->GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Walking);
	}

	if (!IsValid(Enemy->GetController()) && Enemy->HasAuthority())
	{
		Enemy->SpawnDefaultController();
	}

	if (AEnemyController* Controller = Cast<AEnemyController>(Enemy->GetController()))
	{
		Controller->ResumeFromPool();
	}
}

void UEnemySpawnableComponent::SetSpawnPhase(const EEnemyPoolPhase NewPhase)
{
	const EEnemyPoolPhase PreviousPhase = SpawnState.Phase;
	SpawnState.Phase = NewPhase;
	ApplySpawnState(PreviousPhase);
}

void UEnemySpawnableComponent::ApplySpawnState(const EEnemyPoolPhase PreviousPhase)
{
	AEnemyCharacter* Enemy = GetEnemyCharacter();
	if (!IsValid(Enemy))
	{
		return;
	}

	switch (SpawnState.Phase)
	{
	case EEnemyPoolPhase::InactivePooled:
		DisableOwnerCollision();
		Enemy->SetActorHiddenInGame(true);
		break;
	case EEnemyPoolPhase::Reserved:
		DisableOwnerCollision();
		break;
	case EEnemyPoolPhase::Emerging:
		Enemy->SetActorHiddenInGame(false);
		DisableOwnerCollision();
		PlayPresentationCosmetics();
		break;
	case EEnemyPoolPhase::Active:
		Enemy->SetActorHiddenInGame(false);
		RestoreCollisionSnapshot();
		break;
	case EEnemyPoolPhase::Dying:
		break;
	default:
		break;
	}

	OnSpawnPhaseChanged.Broadcast(PreviousPhase, SpawnState.Phase);
}

void UEnemySpawnableComponent::PlayPresentationCosmetics()
{
	if (LastPresentedActivationId == SpawnState.ActivationId ||
		!IsValid(ActiveSpawnMontage) ||
		GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	AEnemyCharacter* Enemy = GetEnemyCharacter();
	if (!IsValid(Enemy))
	{
		return;
	}

	LastPresentedActivationId = SpawnState.ActivationId;
	Enemy->PlayAnimMontage(ActiveSpawnMontage);

	const double Elapsed = FMath::Max(0.0, GetServerTimeSeconds() - SpawnState.PresentationStartServerTime);
	if (Elapsed > 0.0 && Elapsed < SpawnState.PresentationDuration)
	{
		if (UAnimInstance* AnimInstance = Enemy->GetMesh()->GetAnimInstance())
		{
			AnimInstance->Montage_SetPosition(ActiveSpawnMontage, static_cast<float>(Elapsed));
		}
	}
}

void UEnemySpawnableComponent::FinishSpawnPresentationServer(
	const int32 ExpectedActivationId)
{
	AEnemyCharacter* Enemy = GetEnemyCharacter();
	if (!IsValid(Enemy) || !Enemy->HasAuthority() ||
		ExpectedActivationId != SpawnState.ActivationId ||
		SpawnState.Phase != EEnemyPoolPhase::Emerging)
	{
		return;
	}

	ResumeOwner();
	SetSpawnPhase(EEnemyPoolPhase::Active);
	Enemy->ForceNetUpdate();
	CommandInvestigateNoise(PendingNoiseLocation);
}

AEnemyCharacter* UEnemySpawnableComponent::GetEnemyCharacter() const
{
	return Cast<AEnemyCharacter>(GetOwner());
}

double UEnemySpawnableComponent::GetServerTimeSeconds() const
{
	const UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return 0.0;
	}

	if (const AGameStateBase* GameState = World->GetGameState())
	{
		return GameState->GetServerWorldTimeSeconds();
	}

	return World->GetTimeSeconds();
}

void UEnemySpawnableComponent::OnRep_SpawnState(
	const FEnemySpawnRepState PreviousState)
{
	ApplySpawnState(PreviousState.Phase);
}

void UEnemySpawnableComponent::OnRep_ActiveSpawnMontage()
{
	if (SpawnState.Phase == EEnemyPoolPhase::Emerging)
	{
		PlayPresentationCosmetics();
	}
}
