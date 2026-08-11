#include "AI/Spawning/EnemyCharacterSpawner.h"

#include "AI/EnemyCharacter.h"
#include "AI/Spawning/EnemySpawnProfile.h"
#include "AI/Spawning/EnemySpawnTypes.h"
#include "AI/Spawning/ZombieDirectorWorldSubsystem.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "NavigationSystem.h"

AEnemyCharacterSpawner::AEnemyCharacterSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SpawnDirection = CreateDefaultSubobject<UArrowComponent>(TEXT("SpawnDirection"));
	SpawnDirection->SetupAttachment(SceneRoot);

	OccupancyPreview = CreateDefaultSubobject<UBoxComponent>(TEXT("OccupancyPreview"));
	OccupancyPreview->SetupAttachment(SceneRoot);
	OccupancyPreview->SetBoxExtent(FVector(55.0f, 55.0f, 100.0f));
	OccupancyPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OccupancyPreview->SetGenerateOverlapEvents(false);
}

void AEnemyCharacterSpawner::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		if (UZombieDirectorWorldSubsystem* Director =
			GetWorld()->GetSubsystem<UZombieDirectorWorldSubsystem>())
		{
			Director->RegisterSpawner(this);
		}
	}
}

void AEnemyCharacterSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UZombieDirectorWorldSubsystem* Director =
			World->GetSubsystem<UZombieDirectorWorldSubsystem>())
		{
			Director->UnregisterSpawner(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

bool AEnemyCharacterSpawner::CanSpawnForNoise(
	const FEnemyNoiseEvent& NoiseEvent,
	FString* OutFailureReason) const
{
	auto Fail = [OutFailureReason](const TCHAR* Reason)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = Reason;
		}
		return false;
	};

	if (!bEnabled || !ResolveEnemyClass())
	{
		return Fail(TEXT("disabled or enemy class missing"));
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return Fail(TEXT("world missing"));
	}

	if (World->GetTimeSeconds() - LastUsedTime < ReuseCooldown)
	{
		return Fail(TEXT("cooldown"));
	}

	const FVector SpawnLocation = ResolveSpawnTransform().GetLocation();
	const float EffectiveRange = NoiseEvent.MaxRange > 0.0f
		? NoiseEvent.MaxRange
		: TNumericLimits<float>::Max();
	if (FVector::DistSquared2D(SpawnLocation, NoiseEvent.Location) > FMath::Square(EffectiveRange))
	{
		return Fail(TEXT("outside noise range"));
	}

	if (MinPlayerDistance > 0.0f)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* PlayerController = It->Get();
			const APawn* Pawn = IsValid(PlayerController) ? PlayerController->GetPawn() : nullptr;
			if (IsValid(Pawn) &&
				FVector::DistSquared(SpawnLocation, Pawn->GetActorLocation()) < FMath::Square(MinPlayerDistance))
			{
				return Fail(TEXT("too close to player"));
			}
		}
	}

	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQuery.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectQuery.AddObjectTypesToQuery(ECC_Vehicle);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemySpawnerOccupancy), false, this);
	const FCollisionShape Shape = FCollisionShape::MakeBox(OccupancyPreview->GetScaledBoxExtent());
	if (World->OverlapAnyTestByObjectType(
		SpawnLocation,
		ResolveSpawnTransform().GetRotation(),
		ObjectQuery,
		Shape,
		QueryParams))
	{
		return Fail(TEXT("occupied"));
	}

	if (bRequireNavigation)
	{
		const UNavigationSystemV1* Navigation = UNavigationSystemV1::GetCurrent(World);
		FNavLocation ProjectedLocation;
		if (!IsValid(Navigation) ||
			!Navigation->ProjectPointToNavigation(SpawnLocation, ProjectedLocation, FVector(150.0f, 150.0f, 300.0f)))
		{
			return Fail(TEXT("navigation projection failed"));
		}
	}

	return true;
}

FTransform AEnemyCharacterSpawner::ResolveSpawnTransform() const
{
	FTransform Result = SpawnDirection->GetComponentTransform();
	if (IsValid(SpawnProfile))
	{
		Result.AddToTranslation(FVector(0.0f, 0.0f, SpawnProfile->GroundOffset));
	}
	return Result;
}

TSubclassOf<AEnemyCharacter> AEnemyCharacterSpawner::ResolveEnemyClass() const
{
	return IsValid(SpawnProfile) && SpawnProfile->EnemyClass
		? SpawnProfile->EnemyClass
		: EnemyClass;
}

FName AEnemyCharacterSpawner::ResolvePoolKey() const
{
	return IsValid(SpawnProfile) && !SpawnProfile->PoolKey.IsNone()
		? SpawnProfile->PoolKey
		: PoolKey;
}

UAnimMontage* AEnemyCharacterSpawner::ResolveSpawnMontage() const
{
	return IsValid(SpawnProfile) ? SpawnProfile->SpawnMontage.Get() : SpawnMontage.Get();
}

float AEnemyCharacterSpawner::ResolvePresentationDuration() const
{
	return IsValid(SpawnProfile)
		? SpawnProfile->PresentationDuration
		: PresentationDuration;
}

int32 AEnemyCharacterSpawner::ResolveWarmPoolCount() const
{
	return IsValid(SpawnProfile) ? SpawnProfile->WarmPoolCount : WarmPoolCount;
}

void AEnemyCharacterSpawner::MarkUsed()
{
	LastUsedTime = IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.0;
}
