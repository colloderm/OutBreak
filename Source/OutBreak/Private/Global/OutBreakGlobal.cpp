#include "Global/OutBreakGlobal.h"

#include "AI/Spawning/EnemySpawnTypes.h"
#include "AI/Spawning/ZombieDirectorWorldSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AISense_Hearing.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundCue.h"

void UOutBreakGlobal::ReportNoiseToAI(
	UObject* WorldContextObject,
	const FVector& Location,
	AActor* Instigator,
	const FName NoiseTag,
	const float Loudness,
	const float MaxRange)
{
	if (!IsValid(WorldContextObject) ||
		!FMath::IsFinite(Location.X) ||
		!FMath::IsFinite(Location.Y) ||
		!FMath::IsFinite(Location.Z))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("%s: invalid world context or location."),
			TEXT(__FUNCTION__));
		return;
	}

	UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(
			WorldContextObject,
			EGetWorldErrorMode::LogAndReturnNull)
		: nullptr;
	if (!IsValid(World) || World->GetNetMode() == NM_Client)
	{
		return;
	}

	const float EffectiveLoudness = FMath::Clamp(Loudness, 0.0f, 10.0f);
	const float EffectiveMaxRange = FMath::Max(0.0f, MaxRange);
	UAISense_Hearing::ReportNoiseEvent(
		WorldContextObject,
		Location,
		EffectiveLoudness,
		Instigator,
		EffectiveMaxRange,
		NoiseTag);

	if (UZombieDirectorWorldSubsystem* Director =
		World->GetSubsystem<UZombieDirectorWorldSubsystem>())
	{
		FEnemyNoiseEvent NoiseEvent;
		NoiseEvent.Location = Location;
		NoiseEvent.Instigator = Instigator;
		NoiseEvent.NoiseTag = NoiseTag;
		NoiseEvent.Loudness = EffectiveLoudness;
		NoiseEvent.MaxRange = EffectiveMaxRange;
		Director->ReportNoise(NoiseEvent);
	}
}

void UOutBreakGlobal::PlaySoundAndReportNoise(
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
			TEXT("%s: WorldContextObject is invalid."),
			TEXT(__FUNCTION__));
		return;
	}

	float MaxNoiseRange = 0.0f;
	if (IsValid(SoundCue))
	{
		const FSoundAttenuationSettings* AttenuationSettings =
			SoundCue->GetAttenuationSettingsToApply();
		if (AttenuationSettings != nullptr && AttenuationSettings->bAttenuate)
		{
			MaxNoiseRange = AttenuationSettings->GetMaxFalloffDistance();
			MaxNoiseRange *= FMath::Max(0.0f, NoiseRangeScale);
		}

		UGameplayStatics::PlaySoundAtLocation(
			WorldContextObject,
			SoundCue,
			Location,
			FRotator::ZeroRotator,
			1.0f,
			1.0f,
			0.0f,
			nullptr);
	}
	else
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("%s: SoundCue is invalid; reporting AI noise without audio."),
			TEXT(__FUNCTION__));
	}

	ReportNoiseToAI(
		WorldContextObject,
		Location,
		Instigator,
		NoiseTag,
		1.0f,
		MaxNoiseRange);

	UE_LOG(
		LogTemp,
		Verbose,
		TEXT(
			"%s: Sound=%s, NoiseRange=%.2f, "
			"Instigator=%s, Tag=%s"),
		TEXT(__FUNCTION__),
		*GetNameSafe(SoundCue),
		MaxNoiseRange,
		*GetNameSafe(Instigator),
		*NoiseTag.ToString());
}
