#include "AI/Spawning/ZombieDirectorWorldSubsystem.h"

#include "AI/Components/EnemySpawnableComponent.h"
#include "AI/Data/EnemyAsset.h"
#include "AI/EnemyCharacter.h"
#include "AI/Spawning/EnemyCharacterSpawner.h"
#include "AI/Spawning/EnemyDirectorSettings.h"
#include "AI/Spawning/EnemySpawnSectorVolume.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY(LogZombieDirector);

#if !UE_BUILD_SHIPPING
namespace
{
	void ReportZombieDirectorTestNoise(
		const TArray<FString>& Args,
		UWorld* World)
	{
		if (!IsValid(World) || !World->IsGameWorld())
		{
			UE_LOG(LogZombieDirector, Warning, TEXT("TestNoise requires a game world."));
			return;
		}

		FVector Location = FVector::ZeroVector;
		if (Args.Num() >= 3)
		{
			Location.X = FCString::Atof(*Args[0]);
			Location.Y = FCString::Atof(*Args[1]);
			Location.Z = FCString::Atof(*Args[2]);
		}
		else
		{
			for (TActorIterator<AEnemyCharacterSpawner> It(World); It; ++It)
			{
				Location = It->GetActorLocation();
				break;
			}
		}

		if (UZombieDirectorWorldSubsystem* Director =
			World->GetSubsystem<UZombieDirectorWorldSubsystem>())
		{
			FEnemyNoiseEvent NoiseEvent;
			NoiseEvent.Location = Location;
			NoiseEvent.NoiseTag = TEXT("Debug.SpawnTest");
			NoiseEvent.Loudness = 1.0f;
			NoiseEvent.MaxRange = 10000.0f;
			Director->ReportNoise(NoiseEvent);
			UE_LOG(
				LogZombieDirector,
				Log,
				TEXT("Test noise reported at %s."),
				*Location.ToCompactString());
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GReportZombieDirectorTestNoiseCommand(
		TEXT("OutBreak.ZombieDirector.TestNoise"),
		TEXT("Reports an authoritative test noise. Optional args: X Y Z"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
			&ReportZombieDirectorTestNoise));
}
#endif

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
	LatestSectorNoises.Reset();
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
	EnsureBasePopulations(Now);

	for (int32 Index = PendingRequests.Num() - 1; Index >= 0 && SpawnBudget > 0; --Index)
	{
		FPendingSpawnRequest& Request = PendingRequests[Index];
		if (Request.RemainingCount <= 0)
		{
			PendingRequests.RemoveAtSwap(Index);
			continue;
		}
		if (!Request.bPersistent && Request.ExpireTime <= Now)
		{
			UE_LOG(
				LogZombieDirector,
				Warning,
				TEXT("%s request %lld expired with %d remaining. LastFailure=%s"),
				*UEnum::GetValueAsString(Request.PopulationRole),
				static_cast<long long>(Request.NoiseEvent.EventId),
				Request.RemainingCount,
				Request.LastFailureReason.IsEmpty()
					? TEXT("unknown")
					: *Request.LastFailureReason);
			PendingRequests.RemoveAtSwap(Index);
			continue;
		}

		while (Request.RemainingCount > 0 && SpawnBudget > 0)
		{
			if (!TryFulfillOne(Request))
			{
				if (Now - Request.LastFailureLogTime >= 1.0)
				{
					if (Request.bPersistent || Request.LastFailureReason.EndsWith(TEXT("cooldown")))
					{
						UE_LOG(
							LogZombieDirector,
							Verbose,
							TEXT("%s request %lld waiting to spawn %d: %s"),
							*UEnum::GetValueAsString(Request.PopulationRole),
							static_cast<long long>(Request.NoiseEvent.EventId),
							Request.RemainingCount,
							*Request.LastFailureReason);
					}
					else
					{
						UE_LOG(
							LogZombieDirector,
							Warning,
							TEXT("%s request %lld waiting to spawn %d: %s"),
							*UEnum::GetValueAsString(Request.PopulationRole),
							static_cast<long long>(Request.NoiseEvent.EventId),
							Request.RemainingCount,
							Request.LastFailureReason.IsEmpty()
								? TEXT("unknown failure")
								: *Request.LastFailureReason);
					}
					Request.LastFailureLogTime = Now;
				}
				break;
			}

			--Request.RemainingCount;
			--SpawnBudget;
			if (!Request.bPersistent)
			{
				Request.ExpireTime = Now + Settings->SpawnRequestTimeout;
			}
			Request.LastFailureReason.Reset();
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
	const FName SectorId = ResolveSpawnerSectorId(*Spawner);
	const FName BasePoolKey = MakePoolBucketKey(
		Spawner->ResolvePoolKey(),
		SectorId,
		EEnemyPopulationRole::SectorBase);
	const FName ReinforcementPoolKey = MakePoolBucketKey(
		Spawner->ResolvePoolKey(),
		SectorId,
		EEnemyPopulationRole::NoiseReinforcement);
	const int32 BasePoolCountBefore = Pools.FindOrAdd(BasePoolKey).Num();
	const int32 ReinforcementPoolCountBefore = Pools.FindOrAdd(ReinforcementPoolKey).Num();
	WarmPoolForSpawner(*Spawner);
	const int32 BasePoolCountAfter = Pools.FindOrAdd(BasePoolKey).Num();
	const int32 ReinforcementPoolCountAfter = Pools.FindOrAdd(ReinforcementPoolKey).Num();

	FTransform SafeTransform;
	FString PlacementFailure;
	const bool bHasSafePlacement = Spawner->TryResolveSafeSpawnTransform(
		SafeTransform,
		&PlacementFailure);
	UE_LOG(
		LogZombieDirector,
		Log,
		TEXT("Registered spawner %s Sector=%s EnemyClass=%s ArchetypePool=%s BasePool=%s(+%d) ReinforcementPool=%s(+%d) Charges=%d/%d Recharge=+%d/%.1fs SafePlacement=%s%s"),
		*GetNameSafe(Spawner),
		*SectorId.ToString(),
		*GetNameSafe(Spawner->ResolveEnemyClass()),
		*Spawner->ResolvePoolKey().ToString(),
		*BasePoolKey.ToString(),
		FMath::Max(0, BasePoolCountAfter - BasePoolCountBefore),
		*ReinforcementPoolKey.ToString(),
		FMath::Max(0, ReinforcementPoolCountAfter - ReinforcementPoolCountBefore),
		Spawner->GetCurrentSpawnCharges(),
		Spawner->ResolveMaxSpawnCharges(),
		Spawner->ResolveRechargeAmount(),
		Spawner->ResolveRechargeInterval(),
		bHasSafePlacement ? TEXT("true") : TEXT("false"),
		bHasSafePlacement ? TEXT("") : *FString::Printf(TEXT(" Reason=%s"), *PlacementFailure));
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
		LastBasePopulationCheckTime = -DBL_MAX;
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

	const FName SectorId = ResolveSectorId(NoiseEvent.Location);
	FLatestSectorNoise& LatestNoise = LatestSectorNoises.FindOrAdd(SectorId);
	LatestNoise.Location = NoiseEvent.Location;
	LatestNoise.EventId = NoiseEvent.EventId;
	LatestNoise.Timestamp = NoiseEvent.Timestamp;
	DispatchLatestNoiseToReinforcements(SectorId, NoiseEvent.Location);

	if (ShouldMergeNoise(NoiseEvent))
	{
		UE_LOG(
			LogZombieDirector,
			Verbose,
			TEXT("Merged duplicate noise Tag=%s Instigator=%s"),
			*NoiseEvent.NoiseTag.ToString(),
			*GetNameSafe(NoiseEvent.Instigator));
		return;
	}

	const UEnemyDirectorSettings* Settings = GetDefault<UEnemyDirectorSettings>();
	const int32 DesiredCount = FMath::Clamp(
		FMath::RoundToInt(Settings->DefaultResponders * FMath::Max(0.25f, NoiseEvent.Loudness)),
		0,
		Settings->MaxRespondersPerNoise);
	const int32 ExistingCount = RedirectExistingEnemies(NoiseEvent, DesiredCount);
	const int32 ReinforcementCount = DesiredCount;

	if (ReinforcementCount > 0)
	{
		FPendingSpawnRequest* ExistingRequest = PendingRequests.FindByPredicate(
			[SectorId](const FPendingSpawnRequest& Candidate)
			{
				return !Candidate.bPersistent &&
					Candidate.SectorId == SectorId &&
					Candidate.PopulationRole == EEnemyPopulationRole::NoiseReinforcement;
			});
		FPendingSpawnRequest& Request = ExistingRequest
			? *ExistingRequest
			: PendingRequests.AddDefaulted_GetRef();
		Request.NoiseEvent = NoiseEvent;
		Request.SectorId = SectorId;
		Request.PopulationRole = EEnemyPopulationRole::NoiseReinforcement;
		Request.RemainingCount += ReinforcementCount;
		Request.ExpireTime = NoiseEvent.Timestamp + Settings->SpawnRequestTimeout;
		Request.LastFailureReason.Reset();
	}

	UE_LOG(
		LogZombieDirector,
		Log,
		TEXT("Noise %lld Tag=%s RedirectedBaseOrActive=%d AdditionalReinforcements=%d Sector=%s"),
		static_cast<long long>(NoiseEvent.EventId),
		*NoiseEvent.NoiseTag.ToString(),
		ExistingCount,
		ReinforcementCount,
		*SectorId.ToString());
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

void UZombieDirectorWorldSubsystem::DispatchLatestNoiseToReinforcements(
	const FName SectorId,
	const FVector& NoiseLocation)
{
	for (const TWeakObjectPtr<UEnemySpawnableComponent>& WeakEnemy : Enemies)
	{
		UEnemySpawnableComponent* Component = WeakEnemy.Get();
		if (!IsValid(Component) ||
			Component->GetPopulationRole() != EEnemyPopulationRole::NoiseReinforcement ||
			Component->GetSectorId() != SectorId)
		{
			continue;
		}

		Component->CommandInvestigateNoise(NoiseLocation);
	}
}

void UZombieDirectorWorldSubsystem::EnsureBasePopulations(const double Now)
{
	const UEnemyDirectorSettings* Settings = GetDefault<UEnemyDirectorSettings>();
	if (Now - LastBasePopulationCheckTime < Settings->BasePopulationCheckInterval)
	{
		return;
	}
	LastBasePopulationCheckTime = Now;

	AdoptPlacedEnemiesAsSectorBase();
	for (const TWeakObjectPtr<AEnemySpawnSectorVolume>& WeakSector : Sectors)
	{
		const AEnemySpawnSectorVolume* Sector = WeakSector.Get();
		if (!IsValid(Sector) || Sector->GetSectorId().IsNone())
		{
			continue;
		}

		const FName SectorId = Sector->GetSectorId();
		const int32 TargetCount = FMath::Min(
			ResolveSectorBaseTarget(SectorId),
			ResolveSectorHardCap(SectorId));
		const int32 ExistingCount = CountSectorEnemies(
			SectorId,
			EEnemyPopulationRole::SectorBase);
		const int32 PendingCount = CountPendingSpawns(
			SectorId,
			EEnemyPopulationRole::SectorBase);
		const int32 MissingCount = FMath::Max(0, TargetCount - ExistingCount - PendingCount);
		if (MissingCount <= 0)
		{
			continue;
		}

		FPendingSpawnRequest& Request = PendingRequests.AddDefaulted_GetRef();
		Request.NoiseEvent.EventId = NextNoiseEventId++;
		Request.NoiseEvent.Location = Sector->GetActorLocation();
		Request.NoiseEvent.NoiseTag = TEXT("System.SectorBasePopulation");
		Request.NoiseEvent.Loudness = 1.0f;
		Request.NoiseEvent.MaxRange = TNumericLimits<float>::Max();
		Request.NoiseEvent.Timestamp = Now;
		Request.SectorId = SectorId;
		Request.PopulationRole = EEnemyPopulationRole::SectorBase;
		Request.RemainingCount = MissingCount;
		Request.ExpireTime = TNumericLimits<double>::Max();
		Request.bPersistent = true;

		UE_LOG(
			LogZombieDirector,
			Log,
			TEXT("Sector %s base population refill queued: ActiveOrEmerging=%d Pending=%d Target=%d Added=%d"),
			*SectorId.ToString(),
			ExistingCount,
			PendingCount,
			TargetCount,
			MissingCount);
	}
}

void UZombieDirectorWorldSubsystem::AdoptPlacedEnemiesAsSectorBase()
{
	for (const TWeakObjectPtr<UEnemySpawnableComponent>& WeakEnemy : Enemies)
	{
		UEnemySpawnableComponent* Component = WeakEnemy.Get();
		AActor* Owner = IsValid(Component) ? Component->GetOwner() : nullptr;
		if (!IsValid(Owner) ||
			Component->GetPoolPhase() != EEnemyPoolPhase::Active ||
			Component->GetPopulationRole() != EEnemyPopulationRole::Unassigned)
		{
			continue;
		}

		const FName SectorId = ResolveSectorId(Owner->GetActorLocation());
		const FName PoolBucketKey = ResolveBasePoolBucketKey(SectorId);
		if (SectorId.IsNone() || PoolBucketKey.IsNone())
		{
			continue;
		}

		Component->AdoptAsSectorBase(PoolBucketKey, SectorId);
		UE_LOG(
			LogZombieDirector,
			Log,
			TEXT("Adopted placed enemy %s as sector base. Sector=%s Pool=%s"),
			*GetNameSafe(Owner),
			*SectorId.ToString(),
			*PoolBucketKey.ToString());
	}
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
		Request.LastFailureReason = TEXT("global hard cap reached");
		return false;
	}

	if (CountSectorEnemies(Request.SectorId) >= ResolveSectorHardCap(Request.SectorId))
	{
		Request.LastFailureReason = FString::Printf(
			TEXT("sector %s hard cap reached"),
			*Request.SectorId.ToString());
		return false;
	}

	AEnemyCharacterSpawner* Spawner = SelectSpawner(Request, &Request.LastFailureReason);
	if (!IsValid(Spawner))
	{
		return false;
	}

	FTransform SpawnTransform;
	FString PlacementFailure;
	if (!Spawner->TryResolveSafeSpawnTransform(SpawnTransform, &PlacementFailure))
	{
		Request.LastFailureReason = FString::Printf(
			TEXT("%s placement failed: %s"),
			*GetNameSafe(Spawner),
			*PlacementFailure);
		UE_LOG(
			LogZombieDirector,
			Verbose,
			TEXT("Spawner %s rejected placement: %s"),
			*GetNameSafe(Spawner),
			*PlacementFailure);
		return false;
	}

	const FName PoolBucketKey = MakePoolBucketKey(
		Spawner->ResolvePoolKey(),
		Request.SectorId,
		Request.PopulationRole);
	AEnemyCharacter* Enemy = AcquireEnemy(
		*Spawner,
		PoolBucketKey,
		Request.PopulationRole,
		SpawnTransform);
	if (!IsValid(Enemy))
	{
		Request.LastFailureReason = FString::Printf(
			TEXT("%s could not acquire or create an enemy"),
			*GetNameSafe(Spawner));
		return false;
	}

	UEnemySpawnableComponent* Spawnable =
		Enemy->FindComponentByClass<UEnemySpawnableComponent>();
	if (!IsValid(Spawnable))
	{
		Request.LastFailureReason = FString::Printf(
			TEXT("%s has no EnemySpawnableComponent"),
			*GetNameSafe(Enemy));
		Enemy->Destroy();
		return false;
	}

	Spawner->MarkUsed();
	Spawnable->ReserveForActivation(
		PoolBucketKey,
		Request.SectorId,
		Request.PopulationRole);
	FEnemyNoiseEvent DispatchNoiseEvent = Request.NoiseEvent;
	if (Request.PopulationRole == EEnemyPopulationRole::NoiseReinforcement)
	{
		if (const FLatestSectorNoise* LatestNoise = LatestSectorNoises.Find(Request.SectorId))
		{
			DispatchNoiseEvent.Location = LatestNoise->Location;
			DispatchNoiseEvent.EventId = LatestNoise->EventId;
			DispatchNoiseEvent.Timestamp = LatestNoise->Timestamp;
		}
	}
	Spawnable->BeginSpawnPresentation(
		SpawnTransform,
		Spawner->ResolveSpawnMontage(),
		Spawner->ResolvePresentationDuration(),
		DispatchNoiseEvent);
	UE_LOG(
		LogZombieDirector,
		Log,
		TEXT("Activated %s Role=%s from %s at %s (requested %s, sector %s, pool %s, latest noise %lld at %s)"),
		*GetNameSafe(Enemy),
		*UEnum::GetValueAsString(Request.PopulationRole),
		*GetNameSafe(Spawner),
		*SpawnTransform.GetLocation().ToCompactString(),
		*Spawner->ResolveSpawnTransform().GetLocation().ToCompactString(),
		*Request.SectorId.ToString(),
		*PoolBucketKey.ToString(),
		static_cast<long long>(DispatchNoiseEvent.EventId),
		*DispatchNoiseEvent.Location.ToCompactString());
	Request.LastFailureReason.Reset();
	Spawner->BP_OnEnemyEmerging(Enemy, DispatchNoiseEvent.Location);
	return true;
}

AEnemyCharacterSpawner* UZombieDirectorWorldSubsystem::SelectSpawner(
	const FPendingSpawnRequest& Request,
	FString* OutFailureReason) const
{
	AEnemyCharacterSpawner* BestSpawner = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	int32 ValidSpawnerCount = 0;
	int32 SectorMatchCount = 0;
	FString LastRejection;

	for (const TWeakObjectPtr<AEnemyCharacterSpawner>& WeakSpawner : Spawners)
	{
		AEnemyCharacterSpawner* Spawner = WeakSpawner.Get();
		if (!IsValid(Spawner))
		{
			continue;
		}
		++ValidSpawnerCount;

		const FName SpawnerSector = Spawner->GetSectorId().IsNone()
			? ResolveSectorId(Spawner->GetActorLocation())
			: Spawner->GetSectorId();
		if (!Request.SectorId.IsNone() && SpawnerSector != Request.SectorId)
		{
			continue;
		}
		++SectorMatchCount;

		FString RejectionReason;
		if (!Spawner->CanSpawnForNoise(Request.NoiseEvent, &RejectionReason))
		{
			LastRejection = FString::Printf(
				TEXT("%s rejected: %s"),
				*GetNameSafe(Spawner),
				*RejectionReason);
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

	if (!IsValid(BestSpawner) && OutFailureReason)
	{
		if (ValidSpawnerCount == 0)
		{
			*OutFailureReason = TEXT("no registered spawners");
		}
		else if (SectorMatchCount == 0)
		{
			*OutFailureReason = FString::Printf(
				TEXT("no spawner matches sector %s"),
				*Request.SectorId.ToString());
		}
		else
		{
			*OutFailureReason = LastRejection.IsEmpty()
				? TEXT("all matching spawners rejected")
				: LastRejection;
		}
	}

	return BestSpawner;
}

AEnemyCharacter* UZombieDirectorWorldSubsystem::AcquireEnemy(
	AEnemyCharacterSpawner& Spawner,
	const FName PoolBucketKey,
	const EEnemyPopulationRole PopulationRole,
	const FTransform& SpawnTransform)
{
	TArray<TWeakObjectPtr<AEnemyCharacter>>& Pool = Pools.FindOrAdd(PoolBucketKey);
	while (!Pool.IsEmpty())
	{
		TWeakObjectPtr<AEnemyCharacter> Candidate = Pool.Pop(EAllowShrinking::No);
		AEnemyCharacter* Enemy = Candidate.Get();
		if (IsValid(Enemy))
		{
			return Enemy;
		}
	}

	return CreateEnemy(
		Spawner,
		false,
		SpawnTransform,
		PoolBucketKey,
		PopulationRole);
}

void UZombieDirectorWorldSubsystem::WarmPoolForSpawner(
	AEnemyCharacterSpawner& Spawner)
{
	const FName ArchetypePoolKey = Spawner.ResolvePoolKey();
	if (ArchetypePoolKey.IsNone() || !Spawner.ResolveEnemyClass())
	{
		return;
	}

	const FName SectorId = ResolveSpawnerSectorId(Spawner);
	const FName BasePoolKey = MakePoolBucketKey(
		ArchetypePoolKey,
		SectorId,
		EEnemyPopulationRole::SectorBase);
	const FName ReinforcementPoolKey = MakePoolBucketKey(
		ArchetypePoolKey,
		SectorId,
		EEnemyPopulationRole::NoiseReinforcement);
	const int32 ReinforcementWarmCount = FMath::Max(
		0,
		Spawner.ResolveWarmPoolCount() >= 0
			? Spawner.ResolveWarmPoolCount()
			: GetDefault<UEnemyDirectorSettings>()->DefaultWarmPoolCount);

	WarmPoolBucket(
		Spawner,
		BasePoolKey,
		EEnemyPopulationRole::SectorBase,
		ResolveSectorBaseTarget(SectorId));
	WarmPoolBucket(
		Spawner,
		ReinforcementPoolKey,
		EEnemyPopulationRole::NoiseReinforcement,
		ReinforcementWarmCount);
}

void UZombieDirectorWorldSubsystem::WarmPoolBucket(
	AEnemyCharacterSpawner& Spawner,
	const FName PoolBucketKey,
	const EEnemyPopulationRole PopulationRole,
	const int32 DesiredWarmCount)
{
	if (PoolBucketKey.IsNone() || WarmedPoolKeys.Contains(PoolBucketKey))
	{
		return;
	}
	WarmedPoolKeys.Add(PoolBucketKey);

	for (int32 Index = 0; Index < DesiredWarmCount; ++Index)
	{
		if (CountBudgetedEnemies() + CountPooledEnemies() >=
			GetDefault<UEnemyDirectorSettings>()->GlobalHardCap)
		{
			break;
		}

		CreateEnemy(
			Spawner,
			true,
			Spawner.ResolveSpawnTransform(),
			PoolBucketKey,
			PopulationRole);
	}
}

AEnemyCharacter* UZombieDirectorWorldSubsystem::CreateEnemy(
	AEnemyCharacterSpawner& Spawner,
	const bool bForPool,
	const FTransform& RequestedSpawnTransform,
	const FName PoolBucketKey,
	const EEnemyPopulationRole PopulationRole)
{
	TSubclassOf<AEnemyCharacter> EnemyClass = Spawner.ResolveEnemyClass();
	if (!EnemyClass)
	{
		return nullptr;
	}

	const AEnemyCharacter* EnemyDefaults =
		EnemyClass->GetDefaultObject<AEnemyCharacter>();
	if (!IsValid(EnemyDefaults) || !IsValid(EnemyDefaults->GetEnemyAsset()))
	{
		UE_LOG(
			LogZombieDirector,
			Error,
			TEXT("CreateEnemy rejected class '%s' from spawner '%s': EnemyAsset is not assigned on the class defaults."),
			*GetNameSafe(EnemyClass.Get()),
			*GetNameSafe(&Spawner));
		return nullptr;
	}

	FTransform SpawnTransform = RequestedSpawnTransform;
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
			Spawnable->InitializeAsPooled(PoolBucketKey, PopulationRole);
			ReturnEnemyToPool(Enemy, PoolBucketKey);
		}
	}

	return Enemy;
}

FName UZombieDirectorWorldSubsystem::MakePoolBucketKey(
	const FName ArchetypePoolKey,
	const FName SectorId,
	const EEnemyPopulationRole PopulationRole) const
{
	const TCHAR* RoleSuffix = TEXT("Unassigned");
	switch (PopulationRole)
	{
	case EEnemyPopulationRole::SectorBase:
		RoleSuffix = TEXT("Base");
		break;
	case EEnemyPopulationRole::NoiseReinforcement:
		RoleSuffix = TEXT("Reinforcement");
		break;
	default:
		break;
	}

	return FName(*FString::Printf(
		TEXT("%s.%s.%s"),
		ArchetypePoolKey.IsNone() ? TEXT("DefaultZombie") : *ArchetypePoolKey.ToString(),
		SectorId.IsNone() ? TEXT("Global") : *SectorId.ToString(),
		RoleSuffix));
}

FName UZombieDirectorWorldSubsystem::ResolveSpawnerSectorId(
	const AEnemyCharacterSpawner& Spawner) const
{
	return Spawner.GetSectorId().IsNone()
		? ResolveSectorId(Spawner.GetActorLocation())
		: Spawner.GetSectorId();
}

FName UZombieDirectorWorldSubsystem::ResolveBasePoolBucketKey(const FName SectorId) const
{
	for (const TWeakObjectPtr<AEnemyCharacterSpawner>& WeakSpawner : Spawners)
	{
		const AEnemyCharacterSpawner* Spawner = WeakSpawner.Get();
		if (IsValid(Spawner) && ResolveSpawnerSectorId(*Spawner) == SectorId)
		{
			return MakePoolBucketKey(
				Spawner->ResolvePoolKey(),
				SectorId,
				EEnemyPopulationRole::SectorBase);
		}
	}

	return NAME_None;
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

int32 UZombieDirectorWorldSubsystem::CountPooledEnemies() const
{
	int32 Count = 0;
	for (const TPair<FName, TArray<TWeakObjectPtr<AEnemyCharacter>>>& Pair : Pools)
	{
		for (const TWeakObjectPtr<AEnemyCharacter>& WeakEnemy : Pair.Value)
		{
			Count += WeakEnemy.IsValid() ? 1 : 0;
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

int32 UZombieDirectorWorldSubsystem::CountSectorEnemies(
	const FName SectorId,
	const EEnemyPopulationRole PopulationRole) const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<UEnemySpawnableComponent>& WeakEnemy : Enemies)
	{
		if (const UEnemySpawnableComponent* Component = WeakEnemy.Get();
			IsValid(Component) && Component->IsBudgeted() &&
			Component->GetSectorId() == SectorId &&
			Component->GetPopulationRole() == PopulationRole)
		{
			++Count;
		}
	}
	return Count;
}

int32 UZombieDirectorWorldSubsystem::CountPendingSpawns(
	const FName SectorId,
	const EEnemyPopulationRole PopulationRole) const
{
	int32 Count = 0;
	for (const FPendingSpawnRequest& Request : PendingRequests)
	{
		if (Request.SectorId == SectorId && Request.PopulationRole == PopulationRole)
		{
			Count += FMath::Max(0, Request.RemainingCount);
		}
	}
	return Count;
}

int32 UZombieDirectorWorldSubsystem::ResolveSectorBaseTarget(const FName SectorId) const
{
	for (const TWeakObjectPtr<AEnemySpawnSectorVolume>& WeakSector : Sectors)
	{
		const AEnemySpawnSectorVolume* Sector = WeakSector.Get();
		if (IsValid(Sector) && Sector->GetSectorId() == SectorId)
		{
			return FMath::Max(0, Sector->GetBaseZombieTarget());
		}
	}

	return FMath::Max(
		0,
		GetDefault<UEnemyDirectorSettings>()->DefaultBaseZombiesPerSector);
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
