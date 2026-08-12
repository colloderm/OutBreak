#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "EnemyDirectorSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Enemy Director"))
class OUTBREAK_API UEnemyDirectorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category="Budget", meta=(ClampMin="1"))
	int32 GlobalHardCap = 120;

	UPROPERTY(Config, EditAnywhere, Category="Budget", meta=(ClampMin="1"))
	int32 DefaultSectorSoftCap = 16;

	UPROPERTY(Config, EditAnywhere, Category="Budget", meta=(ClampMin="1"))
	int32 DefaultSectorHardCap = 24;

	UPROPERTY(Config, EditAnywhere, Category="Budget", meta=(ClampMin="1"))
	int32 SpawnBurstPerFrame = 4;

	/** Resident zombies maintained independently for every registered sector. */
	UPROPERTY(Config, EditAnywhere, Category="Population", meta=(ClampMin="0"))
	int32 DefaultBaseZombiesPerSector = 6;

	UPROPERTY(Config, EditAnywhere, Category="Population", meta=(ClampMin="0.1", Units="s"))
	float BasePopulationCheckInterval = 0.5f;

	/** Additional pooled reinforcements requested by each noise event. */
	UPROPERTY(Config, EditAnywhere, Category="Response", meta=(ClampMin="0"))
	int32 DefaultResponders = 6;

	UPROPERTY(Config, EditAnywhere, Category="Response", meta=(ClampMin="0"))
	int32 MaxRespondersPerNoise = 16;

	UPROPERTY(Config, EditAnywhere, Category="Response", meta=(ClampMin="0.0", Units="cm"))
	float DefaultNoiseRange = 10000.0f;

	UPROPERTY(Config, EditAnywhere, Category="Response", meta=(ClampMin="0.0", Units="s"))
	float MergeWindow = 0.2f;

	UPROPERTY(Config, EditAnywhere, Category="Response", meta=(ClampMin="0.0", Units="cm"))
	float MergeRadius = 500.0f;

	UPROPERTY(Config, EditAnywhere, Category="Response", meta=(ClampMin="0.1", Units="s"))
	float SpawnRequestTimeout = 3.0f;

	UPROPERTY(Config, EditAnywhere, Category="Pool", meta=(ClampMin="0"))
	int32 DefaultWarmPoolCount = 8;

	UPROPERTY(Config, EditAnywhere, Category="Pool", meta=(ClampMin="1000.0", Units="cm"))
	float PooledActorZOffset = 200000.0f;

	/** Shared charge values. Every spawner adds its instance adjustments to these. */
	UPROPERTY(Config, EditAnywhere, Category="Spawner Charge", meta=(ClampMin="0"))
	int32 DefaultSpawnerInitialCharges = 12;

	UPROPERTY(Config, EditAnywhere, Category="Spawner Charge", meta=(ClampMin="1"))
	int32 DefaultSpawnerMaxCharges = 12;

	UPROPERTY(Config, EditAnywhere, Category="Spawner Charge", meta=(ClampMin="0"))
	int32 DefaultSpawnerRechargeAmount = 2;

	UPROPERTY(Config, EditAnywhere, Category="Spawner Charge", meta=(ClampMin="0.1", Units="s"))
	float DefaultSpawnerRechargeInterval = 10.0f;

	/** Noise reinforcements are pooled after this long without a combat target. */
	UPROPERTY(Config, EditAnywhere, Category="Reinforcement Cleanup", meta=(ClampMin="0.0", Units="s"))
	float ReinforcementCombatExitPoolDelay = 30.0f;
};
