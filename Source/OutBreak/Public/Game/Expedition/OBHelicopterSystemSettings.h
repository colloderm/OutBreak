#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OBHelicopterSystemSettings.generated.h"

class AOBSignalFlare;
class UNiagaraSystem;
class USoundBase;
class USoundCue;

/** Project-wide defaults used by the insertion and extraction helicopter system. */
UCLASS(Config = Game, DefaultConfig, Meta = (DisplayName = "OutBreak Helicopter System"))
class OUTBREAK_API UOBHelicopterSystemSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/**
	 * Default flare spawned when an extraction call starts. Create a Blueprint
	 * child of AOBSignalFlare and assign it here. An extraction-zone Blueprint can
	 * still override this with its own Signal Flare Class.
	 */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Extraction|Presentation",
		Meta = (DisplayName = "Default Extraction Signal Flare Class"))
	TSoftClassPtr<AOBSignalFlare> ExtractionSignalFlareClass;

	/**
	 * Non-spatial radio message played only for the player whose call-trigger
	 * overlap successfully starts an extraction. This does not report AI noise.
	 */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Extraction|Audio",
		Meta = (DisplayName = "Call Accepted Radio Sound"))
	TSoftObjectPtr<USoundBase> ExtractionCallRadioSound;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Extraction|Audio",
		Meta = (DisplayName = "Call Accepted Radio Volume", ClampMin = "0.0", UIMin = "0.0"))
	float ExtractionCallRadioVolume = 1.f;

	/** Optional Niagara trail. When empty, components authored on the flare Blueprint remain usable. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Extraction|Signal Flare|Effects",
		Meta = (DisplayName = "Trail Niagara System"))
	TSoftObjectPtr<UNiagaraSystem> SignalFlareTrailSystem;

	/** One-shot Niagara system spawned in world space when the flare reaches its burst height. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Extraction|Signal Flare|Effects",
		Meta = (DisplayName = "Burst Niagara System"))
	TSoftObjectPtr<UNiagaraSystem> SignalFlareBurstSystem;

	/** Optional independent smoke/effect spawned with the burst and allowed to outlive the flare actor. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Extraction|Signal Flare|Effects",
		Meta = (DisplayName = "Post Burst Niagara System"))
	TSoftObjectPtr<UNiagaraSystem> SignalFlarePostBurstSystem;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Extraction|Signal Flare|Effects",
		Meta = (DisplayName = "Burst Effect Scale", ClampMin = "0.01", Units = "x"))
	float SignalFlareBurstEffectScale = 1.f;

	/** Speed applied straight up along world +Z by the authoritative projectile movement component. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Extraction|Signal Flare|Flight",
		Meta = (DisplayName = "Launch Speed", ClampMin = "100.0", Units = "cm/s"))
	float SignalFlareLaunchSpeed = 4000.f;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Extraction|Signal Flare|Flight",
		Meta = (DisplayName = "Projectile Gravity Scale", ClampMin = "0.0"))
	float SignalFlareGravityScale = 0.15f;

	/** World-space height gained above the launch point before the flare bursts. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Extraction|Signal Flare|Flight",
		Meta = (DisplayName = "Burst Height Above Launch", ClampMin = "100.0", Units = "cm"))
	float SignalFlareBurstHeight = 5000.f;

	/** Safety timeout used if gravity or a blocked flight prevents reaching the configured height. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Extraction|Signal Flare|Flight",
		Meta = (DisplayName = "Maximum Flight Time", ClampMin = "0.1", Units = "s"))
	float SignalFlareMaxFlightSeconds = 6.f;

	/** Keeps the replicated actor alive briefly so clients receive and present the burst before deletion. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Extraction|Signal Flare|Flight",
		Meta = (DisplayName = "Destroy Delay After Burst", ClampMin = "0.25", Units = "s"))
	float SignalFlareDestroyDelay = 2.f;

	/** Sound Cue played through Play Sound And Report Noise when the flare launches. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Extraction|Signal Flare|Audio",
		Meta = (DisplayName = "Launch Sound Cue"))
	TSoftObjectPtr<USoundCue> SignalFlareLaunchSound;

	/** Sound Cue played through Play Sound And Report Noise at the final burst location. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Extraction|Signal Flare|Audio",
		Meta = (DisplayName = "Burst Sound Cue"))
	TSoftObjectPtr<USoundCue> SignalFlareBurstSound;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Extraction|Signal Flare|Audio",
		Meta = (DisplayName = "Launch Noise Tag"))
	FName SignalFlareLaunchNoiseTag = TEXT("Flare");

	UPROPERTY(Config, EditDefaultsOnly, Category = "Extraction|Signal Flare|Audio",
		Meta = (DisplayName = "Burst Noise Tag"))
	FName SignalFlareBurstNoiseTag = TEXT("Flare");

	/** Multiplies the maximum attenuation distance reported to AI for the launch sound. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Extraction|Signal Flare|Audio",
		Meta = (DisplayName = "Launch Noise Range Scale", ClampMin = "0.01", Units = "x"))
	float SignalFlareLaunchNoiseRangeScale = 1.f;

	/** Multiplies the maximum attenuation distance reported to AI for the burst sound. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Extraction|Signal Flare|Audio",
		Meta = (DisplayName = "Burst Noise Range Scale", ClampMin = "0.01", Units = "x"))
	float SignalFlareBurstNoiseRangeScale = 1.f;

	/** Delay before departure after every active member of the calling team is seated. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Extraction|Timing",
		Meta = (ClampMin = "2.0", ClampMax = "3.0", UIMin = "2.0", UIMax = "3.0", Units = "s"))
	float AllTeamBoardedDepartureDelay = 3.f;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
