#include "AI/Components/EnemySpawnableComponent.h"

#include "AI/Components/EnemyPhysicalComponent.h"
#include "AI/Components/EnemyMemoryComponent.h"
#include "AI/EnemyCharacter.h"
#include "AI/EnemyController.h"
#include "AI/Spawning/EnemyDirectorSettings.h"
#include "AI/Spawning/ZombieDirectorWorldSubsystem.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UEnemySpawnableComponent::UEnemySpawnableComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickInterval = 0.5f;
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

void UEnemySpawnableComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateReinforcementCleanup();
}

void UEnemySpawnableComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UEnemySpawnableComponent, SpawnState);
	DOREPLIFETIME(UEnemySpawnableComponent, ActiveSpawnMontage);
}

void UEnemySpawnableComponent::InitializeAsPooled(
	const FName InPoolKey,
	const EEnemyPopulationRole InPopulationRole)
{
	AEnemyCharacter* Enemy = GetEnemyCharacter();
	if (!IsValid(Enemy) || !Enemy->HasAuthority())
	{
		return;
	}

	SpawnState.PoolKey = InPoolKey;
	SpawnState.SectorId = NAME_None;
	SpawnState.PopulationRole = InPopulationRole;
	PendingNoiseLocation = FVector::ZeroVector;
	bHasPendingNoiseCommand = false;
	LastCombatActivityTime = 0.0;
	SetComponentTickEnabled(false);
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
	const FName InSectorId,
	const EEnemyPopulationRole InPopulationRole)
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
	SpawnState.PopulationRole = InPopulationRole;
	SpawnState.PresentationStartServerTime = 0.0;
	SpawnState.PresentationDuration = 0.0f;
	LastCombatActivityTime = 0.0;
	SetComponentTickEnabled(false);
	SetSpawnPhase(EEnemyPoolPhase::Reserved);
	SuspendOwner();
}

void UEnemySpawnableComponent::AdoptAsSectorBase(
	const FName InPoolKey,
	const FName InSectorId)
{
	AEnemyCharacter* Enemy = GetEnemyCharacter();
	if (!IsValid(Enemy) || !Enemy->HasAuthority() ||
		SpawnState.Phase != EEnemyPoolPhase::Active)
	{
		return;
	}

	SpawnState.PoolKey = InPoolKey;
	SpawnState.SectorId = InSectorId;
	SpawnState.PopulationRole = EEnemyPopulationRole::SectorBase;
	Enemy->ForceNetUpdate();
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
	bHasPendingNoiseCommand =
		SpawnState.PopulationRole == EEnemyPopulationRole::NoiseReinforcement;
	PendingNoiseLocation = bHasPendingNoiseCommand
		? NoiseEvent.Location
		: FVector::ZeroVector;
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
	bHasPendingNoiseCommand = false;
	LastCombatActivityTime = 0.0;
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
	if (!IsValid(Enemy) || !Enemy->HasAuthority())
	{
		return;
	}

	if (SpawnState.PopulationRole == EEnemyPopulationRole::NoiseReinforcement &&
		(SpawnState.Phase == EEnemyPoolPhase::Reserved ||
			SpawnState.Phase == EEnemyPoolPhase::Emerging))
	{
		PendingNoiseLocation = NoiseLocation;
		bHasPendingNoiseCommand = true;
		LastCombatActivityTime = GetServerTimeSeconds();
		return;
	}

	if (SpawnState.Phase != EEnemyPoolPhase::Active)
	{
		return;
	}

	PendingNoiseLocation = NoiseLocation;
	if (SpawnState.PopulationRole == EEnemyPopulationRole::NoiseReinforcement)
	{
		LastCombatActivityTime = GetServerTimeSeconds();
	}
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
		Snapshot.bIsRootComponent = Primitive == Owner->GetRootComponent();
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
			// A root component's relative transform is its world transform. Pooled
			// actors are constructed at the off-map storage location, so restoring
			// this value would teleport an emerged enemy back under the map.
			if (!Snapshot.bIsRootComponent)
			{
				Primitive->SetRelativeTransform(Snapshot.RelativeTransform);
			}
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

bool UEnemySpawnableComponent::SnapOwnerToGround(FString& OutFailureReason)
{
	AEnemyCharacter* Enemy = GetEnemyCharacter();
	UCapsuleComponent* Capsule = IsValid(Enemy)
		? Enemy->GetCapsuleComponent()
		: nullptr;
	UWorld* World = GetWorld();
	if (!IsValid(Enemy) || !IsValid(Capsule) || !IsValid(World))
	{
		OutFailureReason = TEXT("enemy, capsule, or world missing");
		return false;
	}

	const FVector ActorLocation = Enemy->GetActorLocation();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector TraceStart = ActorLocation + FVector(0.0f, 0.0f, FMath::Max(100.0f, HalfHeight));
	const FVector TraceEnd = ActorLocation - FVector(0.0f, 0.0f, 1200.0f);

	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyPoolGroundSnap), false, Enemy);

	TArray<FHitResult> Hits;
	if (!World->LineTraceMultiByObjectType(
		Hits,
		TraceStart,
		TraceEnd,
		ObjectQuery,
		QueryParams))
	{
		OutFailureReason = TEXT("no world-static/dynamic surface below spawn");
		return false;
	}

	const UCharacterMovementComponent* Movement = Enemy->GetCharacterMovement();
	const float MinimumWalkableZ = IsValid(Movement)
		? Movement->GetWalkableFloorZ()
		: 0.5f;
	const FHitResult* BestGroundHit = nullptr;
	for (const FHitResult& Hit : Hits)
	{
		const UPrimitiveComponent* HitComponent = Hit.GetComponent();
		if (!Hit.bBlockingHit || !IsValid(HitComponent) ||
			HitComponent->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block ||
			Hit.ImpactNormal.Z < MinimumWalkableZ ||
			Hit.ImpactPoint.Z > TraceStart.Z)
		{
			continue;
		}

		if (!BestGroundHit || Hit.ImpactPoint.Z > BestGroundHit->ImpactPoint.Z)
		{
			BestGroundHit = &Hit;
		}
	}

	if (!BestGroundHit)
	{
		OutFailureReason = TEXT("trace found no Pawn-blocking walkable surface");
		return false;
	}

	FVector GroundedLocation = ActorLocation;
	GroundedLocation.Z = BestGroundHit->ImpactPoint.Z + HalfHeight + 2.0f;
	Enemy->SetActorLocation(
		GroundedLocation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	return true;
}

void UEnemySpawnableComponent::RestoreOwnerForActivation()
{
	AEnemyCharacter* Enemy = GetEnemyCharacter();
	if (!IsValid(Enemy))
	{
		return;
	}

	FString GroundFailure;
	const bool bGrounded = SnapOwnerToGround(GroundFailure);
	RestoreCollisionSnapshot();

	UCapsuleComponent* Capsule = Enemy->GetCapsuleComponent();
	if (IsValid(Capsule))
	{
		// A live Character must always have query collision against the floor,
		// even when an old Blueprint instance saved an invalid disabled value.
		if (Capsule->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
		{
			Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		}
		Capsule->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		Capsule->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	}

	if (UCharacterMovementComponent* Movement = Enemy->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->bForceNextFloorCheck = true;
		Movement->SetMovementMode(MOVE_Walking);
	}

	if (!IsValid(Enemy->GetController()) && Enemy->HasAuthority())
	{
		Enemy->SpawnDefaultController();
	}

	UE_LOG(
		LogZombieDirector,
		Log,
		TEXT("Activation restored %s Role=%s Location=%s GroundSnap=%s%s CapsuleCollision=%s WorldStatic=%s WorldDynamic=%s Movement=%s"),
		*GetNameSafe(Enemy),
		*UEnum::GetValueAsString(SpawnState.PopulationRole),
		*Enemy->GetActorLocation().ToCompactString(),
		bGrounded ? TEXT("true") : TEXT("false"),
		bGrounded ? TEXT("") : *FString::Printf(TEXT(" Failure=%s"), *GroundFailure),
		IsValid(Capsule) ? *UEnum::GetValueAsString(Capsule->GetCollisionEnabled()) : TEXT("Missing"),
		IsValid(Capsule) ? *UEnum::GetValueAsString(Capsule->GetCollisionResponseToChannel(ECC_WorldStatic)) : TEXT("Missing"),
		IsValid(Capsule) ? *UEnum::GetValueAsString(Capsule->GetCollisionResponseToChannel(ECC_WorldDynamic)) : TEXT("Missing"),
		*UEnum::GetValueAsString(Enemy->GetCharacterMovement()->MovementMode.GetValue()));
	if (!bGrounded)
	{
		UE_LOG(
			LogZombieDirector,
			Warning,
			TEXT("%s activated without a ground snap: %s"),
			*GetNameSafe(Enemy),
			*GroundFailure);
	}
}

void UEnemySpawnableComponent::ResumeOwnerAI()
{
	AEnemyCharacter* Enemy = GetEnemyCharacter();
	if (!IsValid(Enemy))
	{
		return;
	}

	if (AEnemyController* Controller = Cast<AEnemyController>(Enemy->GetController()))
	{
		Controller->ResumeFromPool();
	}
}

void UEnemySpawnableComponent::UpdateReinforcementCleanup()
{
	AEnemyCharacter* Enemy = GetEnemyCharacter();
	if (!IsValid(Enemy) || !Enemy->HasAuthority() ||
		SpawnState.Phase != EEnemyPoolPhase::Active ||
		SpawnState.PopulationRole != EEnemyPopulationRole::NoiseReinforcement)
	{
		return;
	}

	const double Now = GetServerTimeSeconds();
	const AEnemyController* Controller = Cast<AEnemyController>(Enemy->GetController());
	const UEnemyMemoryComponent* Memory = IsValid(Controller)
		? Controller->GetEnemyMemoryComponent()
		: nullptr;
	const bool bInCombat = IsValid(Memory) &&
		(Memory->HasValidTarget() || Memory->HasDamageStimulus());
	if (bInCombat)
	{
		LastCombatActivityTime = Now;
		return;
	}

	if (LastCombatActivityTime <= 0.0)
	{
		LastCombatActivityTime = Now;
		return;
	}

	const float PoolDelay = FMath::Max(
		0.0f,
		GetDefault<UEnemyDirectorSettings>()->ReinforcementCombatExitPoolDelay);
	if (Now - LastCombatActivityTime < PoolDelay)
	{
		return;
	}

	UE_LOG(
		LogZombieDirector,
		Log,
		TEXT("Reinforcement %s left combat for %.1fs and is returning to pool %s (sector %s)."),
		*GetNameSafe(Enemy),
		PoolDelay,
		*SpawnState.PoolKey.ToString(),
		*SpawnState.SectorId.ToString());
	ReturnToPoolNow();
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

	const bool bShouldMonitorReinforcement = Enemy->HasAuthority() &&
		SpawnState.Phase == EEnemyPoolPhase::Active &&
		SpawnState.PopulationRole == EEnemyPopulationRole::NoiseReinforcement;
	SetComponentTickEnabled(bShouldMonitorReinforcement);
	if (bShouldMonitorReinforcement && PreviousPhase != EEnemyPoolPhase::Active)
	{
		LastCombatActivityTime = GetServerTimeSeconds();
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
		if (IsValid(ActiveSpawnMontage))
		{
			Enemy->StopAnimMontage(ActiveSpawnMontage);
		}
		RestoreCollisionSnapshot();
		if (UCharacterMovementComponent* Movement = Enemy->GetCharacterMovement())
		{
			Movement->bForceNextFloorCheck = true;
			Movement->SetMovementMode(MOVE_Walking);
		}
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

	RestoreOwnerForActivation();
	SetSpawnPhase(EEnemyPoolPhase::Active);
	ResumeOwnerAI();
	Enemy->ForceNetUpdate();
	if (bHasPendingNoiseCommand)
	{
		CommandInvestigateNoise(PendingNoiseLocation);
	}
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
