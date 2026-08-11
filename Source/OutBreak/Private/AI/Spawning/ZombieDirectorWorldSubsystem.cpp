#include "AI/Spawning/ZombieDirectorWorldSubsystem.h"

#include "AI/Components/EnemySpawnableComponent.h"
#include "AI/EnemyCharacter.h"
#include "AI/Spawning/EnemyCharacterSpawner.h"
#include "AI/Spawning/EnemyDirectorSettings.h"
#include "AI/Spawning/EnemySpawnSectorVolume.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY(LogZombieDirector);

bool UZombieDirectorWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	const UWorld* World = Cast<UWorld>(Outer);
	return IsValid(World) && World->IsGameWorld();
}

void UZombieDirectorWorldSubsystem::Deinitialize()
{
	Spawners.Reset();
	Sectors.Reset();
	Enemies.Reset();
	Pools.Reset();
	WarmedPoolKeys.Reset();
	PendingRequests.Reset();
	RecentNoises.Reset();
	Super::Deinitialize();
}

void UZombieDirectorWorldSubsystem::Tick(float DeltaTime)
{
	if (!IsAuthorityWorld())
	{
		return;
	}

	CompactRegistries();
	const UEnemyDirectorSettings* Settings = GetDefault<UEnemyDirectorSettings>();
	int32 SpawnBudget = FMath::Max(1, Settings->SpawnBurstPerFrame);
	const double Now = GetWorld()->GetTimeSeconds();

	for (int32 Index = PendingRequests.Num() - 1; Index >= 0 && SpawnBudget > 0; --Index)
	{
		FPendingSpawnRequest& Request = PendingRequests[Index];
		if (Request.RemainingCount <= 0 || Request.ExpireTime <= Now)
		{
			PendingRequests.RemoveAtSwap(Index);
			continue;
		}

		while (Request.RemainingCount > 0 && SpawnBudget > 0)
		{
			if (!TryFulfillOne(Request))
			{
				break;
			}

			--Request.RemainingCount;
			--SpawnBudget;
		}

		if (Request.RemainingCount <= 0)
		{
			PendingRequests.RemoveAtSwap(Index);
		}
	}

	RecentNoises.RemoveAll([Now, Settings](const FRecentNoise& Noise)
	{
		return Now - Noise.Time > FMath::Max(1.0f, Settings->MergeWindow * 4.0f);
	});
}

TStatId UZombieDirectorWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UZombieDirectorWorldSubsystem, STATGROUP_Tickables);
}

void UZombieDirectorWorldSubsystem::RegisterSpawner(AEnemyCharacterSpawner* Spawner)
{
	if (!IsAuthorityWorld() || !IsValid(Spawner))
	{
		return;
	}

	Spawners.AddUnique(Spawner);
	WarmPoolForSpawner(*Spawner);
}

void UZombieDirectorWorldSubsystem::UnregisterSpawner(AEnemyCharacterSpawner* Spawner)
{
	Spawners.Remove(Spawner);
}

void UZombieDirectorWorldSubsystem::RegisterSector(AEnemySpawnSectorVolume* Sector)
{
	if (IsAuthorityWorld() && IsValid(Sector))
	{
		Sectors.AddUnique(Sector);
	}
}

void UZombieDirectorWorldSubsystem::UnregisterSector(AEnemySpawnSectorVolume* Sector)
{
	Sectors.Remove(Sector);
}

void UZombieDirectorWorldSubsystem::RegisterEnemy(UEnemySpawnableComponent* SpawnableComponent)
{
	if (IsAuthorityWorld() && IsValid(SpawnableComponent))
	{
		Enemies.AddUnique(SpawnableComponent);
	}
}

void UZombieDirectorWorldSubsystem::UnregisterEnemy(UEnemySpawnableComponent* SpawnableComponent)
{
	Enemies.Remove(SpawnableComponent);
}

void UZombieDirectorWorldSubsystem::ReturnEnemyToPool(
	AEnemyCharacter* Enemy,
	const FName PoolKey)
{
	if (!IsAuthorityWorld() || !IsValid(Enemy))
	{
		return;
	}

	Pools.FindOrAdd(PoolKey).AddUnique(Enemy);
}

void UZombieDirectorWorldSubsystem::ReportNoise(const FEnemyNoiseEvent& InNoiseEvent)
{
	if (!IsAuthorityWorld() || !FMath::IsFinite(InNoiseEvent.Location.X) ||
		!FMath::IsFinite(InNoiseEvent.Location.Y) || !FMath::IsFinite(InNoiseEvent.Location.Z))
	{
		return;
	}

	FEnemyNoiseEvent NoiseEvent = InNoiseEvent;
	NoiseEvent.EventId = NoiseEvent.EventId == 0 ? NextNoiseEventId++ : NoiseEvent.EventId;
	NoiseEvent.Timestamp = GetWorld()->GetTimeSeconds();
	NoiseEvent.Loudness = FMath::Clamp(NoiseEvent.Loudness, 0.0f, 10.0f);
	if (NoiseEvent.MaxRange <= 0.0f)
	{
		NoiseEvent.MaxRange = GetDefault<UEnemyDirectorSettings>()->DefaultNoiseRange;
	}

	if (ShouldMergeNoise(NoiseEvent))
	{
		return;
	}

	const UEnemyDirectorSettings* Settings = GetDefault<UEnemyDirectorSettings>();
	const int32 DesiredCount = FMath::Clamp(
		FMath::RoundToInt(Settings->DefaultResponders * FMath::Max(0.25f, NoiseEvent.Loudness)),
		0,
		Settings->MaxRespondersPerNoise);
	const int32 ExistingCount = RedirectExistingEnemies(NoiseEvent, DesiredCount);
	const int32 Deficit = FMath::Max(0, DesiredCount - ExistingCount);

	if (Deficit > 0)
	{
		FPendingSpawnRequest& Request = PendingRequests.AddDefaulted_GetRef();
		Request.NoiseEvent = NoiseEvent;
		Request.SectorId = ResolveSectorId(NoiseEvent.Location);
		Request.RemainingCount = Deficit;
		Request.ExpireTime = NoiseEvent.Timestamp + Settings->SpawnRequestTimeout;
	}

	UE_LOG(
		LogZombieDirector,
		Verbose,
		TEXT("Noise %lld Tag=%s Desired=%d Redirected=%d SpawnDeficit=%d Sector=%s"),
		static_cast<long long>(NoiseEvent.EventId),
		*NoiseEvent.NoiseTag.ToString(),
		DesiredCount,
		ExistingCount,
		Deficit,
		*ResolveSectorId(NoiseEvent.Location).ToString());
}

FName UZombieDirectorWorldSubsystem::ResolveSectorId(const FVector& Location) const
{
	for (const TWeakObjectPtr<AEnemySpawnSectorVolume>& WeakSector : Sectors)
	{
		const AEnemySpawnSectorVolume* Sector = WeakSector.Get();
		if (IsValid(Sector) && Sector->ContainsLocation(Location))
		{
			return Sector->GetSectorId();
		}
	}

	return NAME_None;
}

bool UZombieDirectorWorldSubsystem::IsAuthorityWorld() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) && World->GetNetMode() != NM_Client;
}

bool UZombieDirectorWorldSubsystem::ShouldMergeNoise(const FEnemyNoiseEvent& NoiseEvent)
{
	const UEnemyDirectorSettings* Settings = GetDefault<UEnemyDirectorSettings>();
	for (FRecentNoise& Recent : RecentNoises)
	{
		if (Recent.Instigator == NoiseEvent.Instigator &&
			Recent.NoiseTag == NoiseEvent.NoiseTag &&
			NoiseEvent.Timestamp - Recent.Time <= Settings->MergeWindow &&
			FVector::DistSquared(Recent.Location, NoiseEvent.Location) <= FMath::Square(Settings->MergeRadius))
		{
			Recent.Location = NoiseEvent.Location;
			Recent.Time = NoiseEvent.Timestamp;
			return true;
		}
	}

	FRecentNoise& Recent = RecentNoises.AddDefaulted_GetRef();
	Recent.Instigator = NoiseEvent.Instigator;
	Recent.NoiseTag = NoiseEvent.NoiseTag;
	Recent.Location = NoiseEvent.Location;
	Recent.Time = NoiseEvent.Timestamp;
	return false;
}

int32 UZombieDirectorWorldSubsystem::RedirectExistingEnemies(
	const FEnemyNoiseEvent& NoiseEvent,
	const int32 DesiredCount)
{
	struct FCandidate
	{
		TWeakObjectPtr<UEnemySpawnableComponent> Component;
		float DistanceSquared = 0.0f;
	};

	TArray<FCandidate> Candidates;
	for (const TWeakObjectPtr<UEnemySpawnableComponent>& WeakEnemy : Enemies)
	{
		UEnemySpawnableComponent* Component = WeakEnemy.Get();
		const AActor* Owner = IsValid(Component) ? Component->GetOwner() : nullptr;
		if (!IsValid(Owner) || Component->GetPoolPhase() != EEnemyPoolPhase::Active)
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared2D(Owner->GetActorLocation(), NoiseEvent.Location);
		if (DistanceSquared <= FMath::Square(NoiseEvent.MaxRange))
		{
			Candidates.Add({Component, DistanceSquared});
		}
	}

	Candidates.Sort([](const FCandidate& Left, const FCandidate& Right)
	{
		return Left.DistanceSquared < Right.DistanceSquared;
	});

	const int32 RedirectCount = FMath::Min(DesiredCount, Candidates.Num());
	for (int32 Index = 0; Index < RedirectCount; ++Index)
	{
		if (UEnemySpawnableComponent* Component = Candidates[Index].Component.Get())
		{
			Component->CommandInvestigateNoise(NoiseEvent.Location);
		}
	}

	return RedirectCount;
}

bool UZombieDirectorWorldSubsystem::TryFulfillOne(FPendingSpawnRequest& Request)
{
	if (CountBudgetedEnemies() >= GetDefault<UEnemyDirectorSettings>()->GlobalHardCap)
	{
		return false;
	}

	if (CountSectorEnemies(Request.SectorId) >= ResolveSectorHardCap(Request.SectorId))
	{
		return false;
	}

	AEnemyCharacterSpawner* Spawner = SelectSpawner(Request);
	if (!IsValid(Spawner))
	{
		return false;
	}

	AEnemyCharacter* Enemy = AcquireEnemy(*Spawner);
	if (!IsValid(Enemy))
	{
		return false;
	}

	UEnemySpawnableComponent* Spawnable =
		Enemy->FindComponentByClass<UEnemySpawnableComponent>();
	if (!IsValid(Spawnable))
	{
		Enemy->Destroy();
		return false;
	}

	Spawner->MarkUsed();
	Spawnable->ReserveForActivation(Spawner->ResolvePoolKey(), Request.SectorId);
	Spawnable->BeginSpawnPresentation(
		Spawner->ResolveSpawnTransform(),
		Spawner->ResolveSpawnMontage(),
		Spawner->ResolvePresentationDuration(),
		Request.NoiseEvent);
	Spawner->BP_OnEnemyEmerging(Enemy, Request.NoiseEvent.Location);
	return true;
}

AEnemyCharacterSpawner* UZombieDirectorWorldSubsystem::SelectSpawner(
	const FPendingSpawnRequest& Request) const
{
	AEnemyCharacterSpawner* BestSpawner = nullptr;
	float BestScore = TNumericLimits<float>::Max();

	for (const TWeakObjectPtr<AEnemyCharacterSpawner>& WeakSpawner : Spawners)
	{
		AEnemyCharacterSpawner* Spawner = WeakSpawner.Get();
		if (!IsValid(Spawner))
		{
			continue;
		}

		const FName SpawnerSector = Spawner->GetSectorId().IsNone()
			? ResolveSectorId(Spawner->GetActorLocation())
			: Spawner->GetSectorId();
		if (!Request.SectorId.IsNone() && SpawnerSector != Request.SectorId)
		{
			continue;
		}

		if (!Spawner->CanSpawnForNoise(Request.NoiseEvent))
		{
			continue;
		}

		const float Score = FVector::DistSquared2D(
			Spawner->GetActorLocation(),
			Request.NoiseEvent.Location);
		if (Score < BestScore)
		{
			BestScore = Score;
			BestSpawner = Spawner;
		}
	}

	return BestSpawner;
}

AEnemyCharacter* UZombieDirectorWorldSubsystem::AcquireEnemy(
	AEnemyCharacterSpawner& Spawner)
{
	const FName PoolKey = Spawner.ResolvePoolKey();
	TArray<TWeakObjectPtr<AEnemyCharacter>>& Pool = Pools.FindOrAdd(PoolKey);
	while (!Pool.IsEmpty())
	{
		TWeakObjectPtr<AEnemyCharacter> Candidate = Pool.Pop(EAllowShrinking::No);
		AEnemyCharacter* Enemy = Candidate.Get();
		if (IsValid(Enemy))
		{
			return Enemy;
		}
	}

	return CreateEnemy(Spawner, false);
}

void UZombieDirectorWorldSubsystem::WarmPoolForSpawner(
	AEnemyCharacterSpawner& Spawner)
{
	const FName PoolKey = Spawner.ResolvePoolKey();
	if (PoolKey.IsNone() || !Spawner.ResolveEnemyClass() ||
		WarmedPoolKeys.Contains(PoolKey))
	{
		return;
	}

	WarmedPoolKeys.Add(PoolKey);
	const int32 DesiredWarmCount = FMath::Max(
		0,
		Spawner.ResolveWarmPoolCount() >= 0
			? Spawner.ResolveWarmPoolCount()
			: GetDefault<UEnemyDirectorSettings>()->DefaultWarmPoolCount);

	for (int32 Index = 0; Index < DesiredWarmCount; ++Index)
	{
		if (CountBudgetedEnemies() + Pools.FindOrAdd(PoolKey).Num() >=
			GetDefault<UEnemyDirectorSettings>()->GlobalHardCap)
		{
			break;
		}

		CreateEnemy(Spawner, true);
	}
}

AEnemyCharacter* UZombieDirectorWorldSubsystem::CreateEnemy(
	AEnemyCharacterSpawner& Spawner,
	const bool bForPool)
{
	TSubclassOf<AEnemyCharacter> EnemyClass = Spawner.ResolveEnemyClass();
	if (!EnemyClass)
	{
		return nullptr;
	}

	FTransform SpawnTransform = Spawner.ResolveSpawnTransform();
	if (bForPool)
	{
		SpawnTransform.AddToTranslation(FVector(
			0.0f,
			0.0f,
			-GetDefault<UEnemyDirectorSettings>()->PooledActorZOffset));
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = &Spawner;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AEnemyCharacter* Enemy = GetWorld()->SpawnActor<AEnemyCharacter>(
		EnemyClass,
		SpawnTransform,
		SpawnParameters);
	if (!IsValid(Enemy))
	{
		return nullptr;
	}

	Enemy->SpawnDefaultController();
	if (bForPool)
	{
		if (UEnemySpawnableComponent* Spawnable =
			Enemy->FindComponentByClass<UEnemySpawnableComponent>())
		{
			Spawnable->InitializeAsPooled(Spawner.ResolvePoolKey());
			ReturnEnemyToPool(Enemy, Spawner.ResolvePoolKey());
		}
	}

	return Enemy;
}

int32 UZombieDirectorWorldSubsystem::CountBudgetedEnemies() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<UEnemySpawnableComponent>& WeakEnemy : Enemies)
	{
		if (const UEnemySpawnableComponent* Component = WeakEnemy.Get();
			IsValid(Component) && Component->IsBudgeted())
		{
			++Count;
		}
	}
	return Count;
}

int32 UZombieDirectorWorldSubsystem::CountSectorEnemies(const FName SectorId) const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<UEnemySpawnableComponent>& WeakEnemy : Enemies)
	{
		if (const UEnemySpawnableComponent* Component = WeakEnemy.Get();
			IsValid(Component) && Component->IsBudgeted() && Component->GetSectorId() == SectorId)
		{
			++Count;
		}
	}
	return Count;
}

int32 UZombieDirectorWorldSubsystem::ResolveSectorHardCap(const FName SectorId) const
{
	for (const TWeakObjectPtr<AEnemySpawnSectorVolume>& WeakSector : Sectors)
	{
		const AEnemySpawnSectorVolume* Sector = WeakSector.Get();
		if (IsValid(Sector) && Sector->GetSectorId() == SectorId)
		{
			return FMath::Max(1, Sector->GetHardCap());
		}
	}

	return FMath::Max(1, GetDefault<UEnemyDirectorSettings>()->DefaultSectorHardCap);
}

void UZombieDirectorWorldSubsystem::CompactRegistries()
{
	Spawners.RemoveAll([](const TWeakObjectPtr<AEnemyCharacterSpawner>& Item) { return !Item.IsValid(); });
	Sectors.RemoveAll([](const TWeakObjectPtr<AEnemySpawnSectorVolume>& Item) { return !Item.IsValid(); });
	Enemies.RemoveAll([](const TWeakObjectPtr<UEnemySpawnableComponent>& Item) { return !Item.IsValid(); });

	for (TPair<FName, TArray<TWeakObjectPtr<AEnemyCharacter>>>& Pair : Pools)
	{
		Pair.Value.RemoveAll([](const TWeakObjectPtr<AEnemyCharacter>& Item) { return !Item.IsValid(); });
	}
}
