// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnemyState.generated.h"

/** Physical locomotion capability. Owned by UEnemyMovementComponent. */
UENUM(BlueprintType)
enum class ELocomotionWalkRunState : uint8
{
	Walking,
	Crawling,
	SlowCrawling,
	Dead,
};

/**
 * Authoritative action lock state. UEnemyStatusComponent is the only runtime
 * owner allowed to mutate this value.
 */
UENUM(BlueprintType)
enum class EEnemyActionState : uint8
{
	Active,
	Stunned,
	Knockdown,
	Dead,
};

/** Last meaningful non-visual stimulus retained by the AI memory. */
UENUM(BlueprintType)
enum class EEnemyStimulusType : uint8
{
	None,
	Hearing,
	Damage,
	LostSight,
};

/** Source selected by the evaluator for the current Alert location. */
UENUM(BlueprintType)
enum class EEnemyAlertSource : uint8
{
	None,
	RememberedTarget,
	Stimulus,
};

/**
 * Wake-up notification emitted by UEnemyAIMemoryComponent.
 *
 * The notification deliberately does not describe a transition. Consumers
 * must re-read the memory snapshot to decide between Combat, Alert, or Passive.
 */
UENUM()
enum class EEnemyMemoryChange : uint8
{
	ContextChanged,
};