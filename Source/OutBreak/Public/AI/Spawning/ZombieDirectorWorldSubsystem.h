#pragma once

#include "CoreMinimal.h"
#include "AI/Spawning/EnemySpawnTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "ZombieDirectorWorldSubsystem.generated.h"

class AEnemyCharacter;
class AEnemyCharacterSpawner;
class AEnemySpawnSectorVolume;
class UEnemySpawnableComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogZombieDirector, Log, All);

UCLASS()
class OUTBREAK_API UZombieDirectorWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	void RegisterSpawner(AEnemyCharacterSpawner* Spawner);
	void UnregisterSpawner(AEnemyCharacterSpawner* Spawner);
	void RegisterSector(AEnemySpawnSectorVolume* Sector);
	void UnregisterSector(AEnemySpawnSectorVolume* Sector);
	void RegisterEnemy(UEnemySpawnableComponent* SpawnableComponent);
	void UnregisterEnemy(UEnemySpawnableComponent* SpawnableComponent);
	void ReturnEnemyToPool(AEnemyCharacter* Enemy, FName PoolKey);

	UFUNCTION(BlueprintCallable, Category="Enemy Director")
	void ReportNoise(const FEnemyNoiseEvent& NoiseEvent);

	FName ResolveSectorId(const FVector& Location) const;

private:
	struct FPendingSpawnRequest
	{
		FEnemyNoiseEvent NoiseEvent;
		FName SectorId = NAME_None;
		EEnemyPopulationRole PopulationRole = EEnemyPopulationRole::NoiseReinforcement;
		int32 RemainingCount = 0;
		double ExpireTime = 0.0;
		bool bPersistent = false;
		double LastFailureLogTime = -DBL_MAX;
		FString LastFailureReason;
	};

	struct FRecentNoise
	{
		TWeakObjectPtr<AActor> Instigator;
		FName NoiseTag = NAME_None;
		FVector Location = FVector::ZeroVector;
		double Time = 0.0;
	};

	struct FLatestSectorNoise
	{
		FVector Location = FVector::ZeroVector;
		int64 EventId = 0;
		double Timestamp = 0.0;
	};

	bool IsAuthorityWorld() const;
	bool ShouldMergeNoise(const FEnemyNoiseEvent& NoiseEvent);
	void DispatchLatestNoiseToReinforcements(FName SectorId, const FVector& NoiseLocation);
	void EnsureBasePopulations(double Now);
	void AdoptPlacedEnemiesAsSectorBase();
	int32 RedirectExistingEnemies(const FEnemyNoiseEvent& NoiseEvent, int32 DesiredCount);
	bool TryFulfillOne(FPendingSpawnRequest& Request);
	AEnemyCharacterSpawner* SelectSpawner(
		const FPendingSpawnRequest& Request,
		FString* OutFailureReason = nullptr) const;
	AEnemyCharacter* AcquireEnemy(
		AEnemyCharacterSpawner& Spawner,
		FName PoolBucketKey,
		EEnemyPopulationRole PopulationRole,
		const FTransform& SpawnTransform);
	void WarmPoolForSpawner(AEnemyCharacterSpawner& Spawner);
	void WarmPoolBucket(
		AEnemyCharacterSpawner& Spawner,
		FName PoolBucketKey,
		EEnemyPopulationRole PopulationRole,
		int32 DesiredWarmCount);
	AEnemyCharacter* CreateEnemy(
		AEnemyCharacterSpawner& Spawner,
		bool bForPool,
		const FTransform& SpawnTransform,
		FName PoolBucketKey,
		EEnemyPopulationRole PopulationRole);
	FName MakePoolBucketKey(
		FName ArchetypePoolKey,
		FName SectorId,
		EEnemyPopulationRole PopulationRole) const;
	FName ResolveSpawnerSectorId(const AEnemyCharacterSpawner& Spawner) const;
	FName ResolveBasePoolBucketKey(FName SectorId) const;
	int32 CountBudgetedEnemies() const;
	int32 CountPooledEnemies() const;
	int32 CountSectorEnemies(FName SectorId) const;
	int32 CountSectorEnemies(
		FName SectorId,
		EEnemyPopulationRole PopulationRole) const;
	int32 CountPendingSpawns(
		FName SectorId,
		EEnemyPopulationRole PopulationRole) const;
	int32 ResolveSectorBaseTarget(FName SectorId) const;
	int32 ResolveSectorHardCap(FName SectorId) const;
	void CompactRegistries();
	void UpdateEnemyReplicationLOD(double Now);

	TArray<TWeakObjectPtr<AEnemyCharacterSpawner>> Spawners;
	TArray<TWeakObjectPtr<AEnemySpawnSectorVolume>> Sectors;
	TArray<TWeakObjectPtr<UEnemySpawnableComponent>> Enemies;
	TMap<FName, TArray<TWeakObjectPtr<AEnemyCharacter>>> Pools;
	TSet<FName> WarmedPoolKeys;
	TArray<FPendingSpawnRequest> PendingRequests;
	TArray<FRecentNoise> RecentNoises;
	TMap<FName, FLatestSectorNoise> LatestSectorNoises;
	int64 NextNoiseEventId = 1;
	double LastBasePopulationCheckTime = -DBL_MAX;
	double LastReplicationLODUpdateTime = -DBL_MAX;
};
