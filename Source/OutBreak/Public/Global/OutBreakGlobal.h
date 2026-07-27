// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"


/**
 * 
 */

class AActor;
class USoundCue;

namespace OutBreakGlobal
{
	void PlaySoundAndReportNoise(
		UObject* WorldContextObject,
		USoundCue* SoundCue,
		const FVector& Location,
		AActor* Instigator,
		FName NoiseTag = NAME_None,
		float NoiseRangeScale = 1.0f);
}
