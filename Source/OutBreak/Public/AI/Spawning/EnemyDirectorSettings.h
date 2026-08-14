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

	/** Limits each connection to the highest-value zombies instead of relying on distance alone. */
	UPROPERTY(Config, EditAnywhere, Category="Network Replication|Interest Budget")
	bool bEnablePerPlayerInterestBudget = true;

	/** Hard normal-state cap for zombie actor channels relevant to one player. */
	UPROPERTY(Config, EditAnywhere, Category="Network Replication|Interest Budget", meta=(ClampMin="1"))
	int32 MaxRelevantZombiesPerPlayer = 48;

	/** Lower cap reached under connection saturation. Immediate threats still rank first. */
	UPROPERTY(Config, EditAnywhere, Category="Network Replication|Interest Budget", meta=(ClampMin="1"))
	int32 MinRelevantZombiesPerPlayer = 12;

	/** Global server cap for the sum of zombie-to-player relevancy pairs. */
	UPROPERTY(Config, EditAnywhere, Category="Network Replication|Interest Budget", meta=(ClampMin="1"))
	int32 MaxTotalZombieViewerPairs = 160;

	/** Per player, only this many top-ranked zombies may use their distance-selected (highest) LOD. */
	UPROPERTY(Config, EditAnywhere, Category="Network Replication|Interest Budget", meta=(ClampMin="1"))
	int32 HighDetailZombiesPerPlayer = 8;

	/** Per player, zombies after this rank are forced to the lowest-frequency LOD. */
	UPROPERTY(Config, EditAnywhere, Category="Network Replication|Interest Budget", meta=(ClampMin="1"))
	int32 MediumDetailZombiesPerPlayer = 24;

	/** Zombies outside the view focus are considered only inside this radius unless they are a threat. */
	UPROPERTY(Config, EditAnywhere, Category="Network Replication|Interest Budget", meta=(ClampMin="100.0", Units="cm"))
	float PeripheralReplicationDistance = 5000.0f;

	/** Dot product threshold for the player's forward focus cone. */
	UPROPERTY(Config, EditAnywhere, Category="Network Replication|Interest Budget", meta=(ClampMin="-1.0", ClampMax="1.0"))
	float ViewFocusDotThreshold = 0.2f;

	UPROPERTY(Config, EditAnywhere, Category="Network Replication|Interest Budget", meta=(ClampMin="0.0"))
	float ViewFocusScoreBonus = 1.25f;

	UPROPERTY(Config, EditAnywhere, Category="Network Replication|Interest Budget", meta=(ClampMin="0.0"))
	float CombatTargetScoreBonus = 4.0f;

	UPROPERTY(Config, EditAnywhere, Category="Network Replication|Interest Budget", meta=(ClampMin="0.0"))
	float RecentCriticalScoreBonus = 3.0f;

	/** Reduces actor-channel churn when candidates have similar scores. */
	UPROPERTY(Config, EditAnywhere, Category="Network Replication|Interest Budget", meta=(ClampMin="0.0"))
	float SelectionRetentionScoreBonus = 0.35f;

	/** Death, attacks, limb loss, spawn, and action changes remain priority candidates for this long. */
	UPROPERTY(Config, EditAnywhere, Category="Network Replication|Interest Budget", meta=(ClampMin="0.0", Units="s"))
	float CriticalRelevancyDuration = 2.0f;

	/** Small temporary overflow used so an immediate threat can open a channel before the next budget pass. */
	UPROPERTY(Config, EditAnywhere, Category="Network Replication|Interest Budget", meta=(ClampMin="0", ClampMax="16"))
	int32 CriticalInterestOverflowPerPlayer = 4;

	/** Shrinks per-player zombie budgets when the corresponding connection approaches saturation. */
	UPROPERTY(Config, EditAnywhere, Category="Network Replication|Congestion Control")
	bool bEnableAdaptiveBandwidthBudget = true;

	/** Begin reducing the zombie budget above this fraction of negotiated connection bandwidth. */
	UPROPERTY(Config, EditAnywhere, Category="Network Replication|Congestion Control", meta=(ClampMin="0.1", ClampMax="0.95"))
	float TargetConnectionUtilization = 0.70f;

	/** Immediately drive toward the minimum budget above this utilization. */
	UPROPERTY(Config, EditAnywhere, Category="Network Replication|Congestion Control", meta=(ClampMin="0.2", ClampMax="1.0"))
	float EmergencyConnectionUtilization = 0.90f;

	/** Congestion reacts quickly but recovers slowly to avoid oscillation. */
	UPROPERTY(Config, EditAnywhere, Category="Network Replication|Congestion Control", meta=(ClampMin="0.1"))
	float CongestionAttackPerSecond = 3.0f;

	UPROPERTY(Config, EditAnywhere, Category="Network Replication|Congestion Control", meta=(ClampMin="0.01"))
	float CongestionRecoveryPerSecond = 0.20f;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
