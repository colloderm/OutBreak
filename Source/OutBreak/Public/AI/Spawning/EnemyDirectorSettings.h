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
};
