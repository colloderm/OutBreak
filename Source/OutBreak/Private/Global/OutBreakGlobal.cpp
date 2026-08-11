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

	if (!IsValid(SoundCue))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("%s: SoundCue is invalid."),
			TEXT(__FUNCTION__));

		return;
	}

	const FSoundAttenuationSettings* AttenuationSettings =
		SoundCue->GetAttenuationSettingsToApply();

	float MaxNoiseRange = 0.0f;

	if (AttenuationSettings != nullptr &&
		AttenuationSettings->bAttenuate)
	{
		MaxNoiseRange =
			AttenuationSettings->GetMaxFalloffDistance();

		MaxNoiseRange *=
			FMath::Max(0.0f, NoiseRangeScale);
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

	constexpr float NoiseLoudness = 1.0f;


	UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(
			WorldContextObject,
			EGetWorldErrorMode::LogAndReturnNull)
		: nullptr;

	// Hearing and Director orchestration are authoritative. This prevents a
	// listen client/server pair from turning one gunshot into two spawn requests.
	if (IsValid(World) && World->GetNetMode() != NM_Client)
	{
		UAISense_Hearing::ReportNoiseEvent(
			WorldContextObject,
			Location,
			NoiseLoudness,
			Instigator,
			MaxNoiseRange,
			NoiseTag);

		if (UZombieDirectorWorldSubsystem* Director =
			World->GetSubsystem<UZombieDirectorWorldSubsystem>())
		{
			FEnemyNoiseEvent NoiseEvent;
			NoiseEvent.Location = Location;
			NoiseEvent.Instigator = Instigator;
			NoiseEvent.NoiseTag = NoiseTag;
			NoiseEvent.Loudness = NoiseLoudness;
			NoiseEvent.MaxRange = MaxNoiseRange;
			Director->ReportNoise(NoiseEvent);
		}
	}

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
