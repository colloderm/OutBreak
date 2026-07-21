// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EnemyState.generated.h"

/**
 * 
 */
UENUM(BlueprintType)
enum class ELocomotionWalkRunState : uint8
{
	Walking,
	Crawling,
	SlowCrawling,
	Dead,
};
