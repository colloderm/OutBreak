#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "EnemyDirectorSettings.generated.h"

/**
 * Distance-based server replication settings for active zombies.
 * MaxDistance is measured from a zombie to the closest player pawn.
 */
USTRUCT(BlueprintType)
struct OUTBREAK_API FEnemyReplicationLODLevel
{
	GENERATED_BODY()

	/** Upper distance for this LOD. Levels are evaluated by distance, not array order. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Network Replication", meta=(ClampMin="100.0", Units="cm"))
	float MaxDistance = 3000.0f;

	/** Maximum actor/property and CharacterMovement replication rate in this LOD. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Network Replication", meta=(ClampMin="0.1", Units="Hz"))
	float NetUpdateFrequency = 30.0f;

	/** Lower bound used by Unreal's adaptive network update frequency. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Network Replication", meta=(ClampMin="0.1", Units="Hz"))
	float MinNetUpdateFrequency = 15.0f;

	/** Per-connection priority multiplier when this zombie competes for bandwidth. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Network Replication", meta=(ClampMin="0.01", ClampMax="1.0"))
	float NetPriorityScale = 1.0f;

	FEnemyReplicationLODLevel() = default;
	FEnemyReplicationLODLevel(
		float InMaxDistance,
		float InNetUpdateFrequency,
		float InMinNetUpdateFrequency,
		float InNetPriorityScale)
		: MaxDistance(InMaxDistance)
		, NetUpdateFrequency(InNetUpdateFrequency)
		, MinNetUpdateFrequency(InMinNetUpdateFrequency)
		, NetPriorityScale(InNetPriorityScale)
	{
	}
};

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Enemy Director"))
class OUTBREAK_API UEnemyDirectorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UEnemyDirectorSettings();

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

	/** Enables distance LOD, per-viewer culling/priority, and all-player-far dormancy for zombies. */
	UPROPERTY(Config, EditAnywhere, Category="Network Replication")
	bool bEnableReplicationLOD = true;

	/** How often the authoritative Enemy Director re-evaluates zombie distance LODs. */
	UPROPERTY(Config, EditAnywhere, Category="Network Replication", meta=(ClampMin="0.05", ClampMax="5.0", Units="s"))
	float ReplicationLODUpdateInterval = 0.25f;

	/** Prevents rapid LOD switching when a zombie moves around a distance boundary. */
	UPROPERTY(Config, EditAnywhere, Category="Network Replication", meta=(ClampMin="0.0", Units="cm"))
	float ReplicationLODHysteresis = 500.0f;

	/**
	 * Replication bands. Defaults: 30 Hz within 30 m, 12 Hz within 70 m,
	 * and 3 Hz within 150 m. The largest MaxDistance is also the per-viewer
	 * network cull distance.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Network Replication", meta=(TitleProperty="MaxDistance"))
	TArray<FEnemyReplicationLODLevel> ReplicationLODLevels;

	/**
	 * DormantAll is used only when the zombie is outside the last LOD for every
	 * player (or no player pawn exists). Critical state changes wake it immediately.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Network Replication")
	bool bUseDormancyBeyondLastLOD = true;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
