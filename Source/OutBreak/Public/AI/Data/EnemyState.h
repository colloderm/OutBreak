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

/** Identifies which arm is currently missing from the enemy. */
UENUM(BlueprintType)
enum class EEnemyMissingArmState : uint8
{
	None,
	Left,
	Right,
	Both,
};

/** Body region selected by the authoritative hit-reaction resolver. */
UENUM(BlueprintType)
enum class EEnemyHitReactRegion : uint8
{
	None,
	Head,
	Torso,
	ArmRight,
	ArmLeft,
	LegRight,
	LegLeft,
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
