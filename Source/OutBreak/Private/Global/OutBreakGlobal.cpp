// Fill out your copyright notice in the Description page of Project Settings.


#include "Global/OutBreakGlobal.h"

#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AISense_Hearing.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundCue.h"

#include "Kismet/GameplayStatics.h"
#include "Perception/AISense_Hearing.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundAttenuation.h"

void OutBreakGlobal::PlaySoundAndReportNoise(
	UObject* WorldContextObject,
	USoundCue* SoundCue,
	const FVector& Location,
	AActor* Instigator,
	const FName NoiseTag,
	const float NoiseRangeScale)
{
	if (!IsValid(WorldContextObject))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("OutBreakGlobal::%s: WorldContextObject is invalid."),
			TEXT(__FUNCTION__));

		return;
	}

	if (!IsValid(SoundCue))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("OutBreakGlobal::%s: SoundCue is invalid."),
			TEXT(__FUNCTION__));

		return;
	}

	/*
	 * SoundCue에 실제로 적용되는 감쇠 설정을 가져옵니다.
	 *
	 * Attenuation Asset 또는 SoundCue의 Override Attenuation을
	 * 직접 구분할 필요가 없습니다.
	 */
	const FSoundAttenuationSettings* AttenuationSettings =
		SoundCue->GetAttenuationSettingsToApply();

	float MaxNoiseRange = 0.0f;

	if (AttenuationSettings != nullptr &&
		AttenuationSettings->bAttenuate)
	{
		/*
		 * Sphere, Capsule, Box, Cone에 따른 최대 감쇠 거리를
		 * 엔진의 계산 함수로 가져옵니다.
		 */
		MaxNoiseRange =
			AttenuationSettings->GetMaxFalloffDistance();

		MaxNoiseRange *=
			FMath::Max(0.0f, NoiseRangeScale);
	}
	else
	{
		/*
		 * MaxRange가 0 이하이면 Noise Event 자체의 거리 제한은 없고,
		 * 최종적으로 AI Hearing 설정의 HearingRange에 제한됩니다.
		 */
		MaxNoiseRange = 0.0f;
	}

	/*
	 * SoundCue가 가진 자체 감쇠 설정을 사용하므로
	 * 별도의 USoundAttenuation Override를 전달하지 않습니다.
	 */
	UGameplayStatics::PlaySoundAtLocation(
		WorldContextObject,
		SoundCue,
		Location,
		FRotator::ZeroRotator,
		1.0f,
		1.0f,
		0.0f,
		nullptr);

	/*
	 * ReportNoiseEvent에서는 MaxRange에 Loudness가 곱해집니다.
	 *
	 * Sound Attenuation에서 최종 범위를 이미 가져왔으므로
	 * Loudness는 1.0으로 고정합니다.
	 */
	constexpr float NoiseLoudness = 1.0f;

	UAISense_Hearing::ReportNoiseEvent(
		WorldContextObject,
		Location,
		NoiseLoudness,
		Instigator,
		MaxNoiseRange,
		NoiseTag);

	UE_LOG(
		LogTemp,
		Verbose,
		TEXT(
			"OutBreakGlobal::%s: Sound=%s, "
			"NoiseRange=%.2f, Tag=%s"),
		TEXT(__FUNCTION__),
		*GetNameSafe(SoundCue),
		MaxNoiseRange,
		*NoiseTag.ToString());
}
