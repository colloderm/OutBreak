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
		int32 RemainingCount = 0;
		double ExpireTime = 0.0;
	};

	struct FRecentNoise
	{
		TWeakObjectPtr<AActor> Instigator;
		FName NoiseTag = NAME_None;
		FVector Location = FVector::ZeroVector;
		double Time = 0.0;
	};

	bool IsAuthorityWorld() const;
	bool ShouldMergeNoise(const FEnemyNoiseEvent& NoiseEvent);
	int32 RedirectExistingEnemies(const FEnemyNoiseEvent& NoiseEvent, int32 DesiredCount);
	bool TryFulfillOne(FPendingSpawnRequest& Request);
	AEnemyCharacterSpawner* SelectSpawner(const FPendingSpawnRequest& Request) const;
	AEnemyCharacter* AcquireEnemy(AEnemyCharacterSpawner& Spawner);
	void WarmPoolForSpawner(AEnemyCharacterSpawner& Spawner);
	AEnemyCharacter* CreateEnemy(AEnemyCharacterSpawner& Spawner, bool bForPool);
	int32 CountBudgetedEnemies() const;
	int32 CountSectorEnemies(FName SectorId) const;
	int32 ResolveSectorHardCap(FName SectorId) const;
	void CompactRegistries();

	TArray<TWeakObjectPtr<AEnemyCharacterSpawner>> Spawners;
	TArray<TWeakObjectPtr<AEnemySpawnSectorVolume>> Sectors;
	TArray<TWeakObjectPtr<UEnemySpawnableComponent>> Enemies;
	TMap<FName, TArray<TWeakObjectPtr<AEnemyCharacter>>> Pools;
	TSet<FName> WarmedPoolKeys;
	TArray<FPendingSpawnRequest> PendingRequests;
	TArray<FRecentNoise> RecentNoises;
	int64 NextNoiseEventId = 1;
};
