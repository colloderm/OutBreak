#pragma once

#include "CoreMinimal.h"
#include "EnemySpawnTypes.generated.h"

class AActor;

UENUM(BlueprintType)
enum class EEnemyPoolPhase : uint8
{
	InactivePooled,
	Reserved,
	Emerging,
	Active,
	Dying,
};

/** Population ownership is independent from the current pool lifecycle phase. */
UENUM(BlueprintType)
enum class EEnemyPopulationRole : uint8
{
	Unassigned,
	SectorBase,
	NoiseReinforcement,
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FEnemySpawnRepState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 ActivationId = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	EEnemyPoolPhase Phase = EEnemyPoolPhase::Active;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FName PoolKey = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FName SectorId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	EEnemyPopulationRole PopulationRole = EEnemyPopulationRole::Unassigned;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	double PresentationStartServerTime = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float PresentationDuration = 0.0f;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FEnemyNoiseEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int64 EventId = 0;

	UPROPERTY(BlueprintReadOnly)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<AActor> Instigator = nullptr;

	UPROPERTY(BlueprintReadOnly)
	FName NoiseTag = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	float Loudness = 1.0f;

	UPROPERTY(BlueprintReadOnly)
	float MaxRange = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	double Timestamp = 0.0;
};
