// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FlowField/Subsystem/BaseHordeWorldSubsystem.h"
#include "HordeEventSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class OUTBREAK_API UHordeEventSubsystem : public UBaseHordeWorldSubsystem
{
	GENERATED_BODY()
	
	
	TArray<HordeDamageEvent> HordeDamageEvents;
	
};
