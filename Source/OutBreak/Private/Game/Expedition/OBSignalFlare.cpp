#include "Game/Expedition/OBSignalFlare.h"

#include "Components/SceneComponent.h"
#include "Game/Expedition/OBHelicopterSystemSettings.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Global/OutBreakGlobal.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Sound/SoundCue.h"
#include "TimerManager.h"

AOBSignalFlare::AOBSignalFlare()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bReplicates = true;
	SetReplicatingMovement(true);
	SetNetUpdateFrequency(20.f);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->InitialSpeed = LaunchSpeed;
	ProjectileMovement->MaxSpeed = LaunchSpeed;
	ProjectileMovement->ProjectileGravityScale = 0.15f;
	// The extraction flare always travels along world +Z. Rotating the actor to
	// follow velocity would also rotate attached Niagara systems onto their side.
	ProjectileMovement->bRotationFollowsVelocity = false;
}

void AOBSignalFlare::BeginPlay()
{
	Super::BeginPlay();
	ConfigureFromProjectSettings();
	// Keep the flare and any attached trail upright even when the launch anchor
	// was authored with a pitch or roll rotation.
	SetActorRotation(FRotator::ZeroRotator, ETeleportType::TeleportPhysics);
	LaunchLocation = GetActorLocation();
	// Do not inherit the extraction zone / launch anchor rotation. A tilted
	// anchor must never turn the signal flare into a forward-fired projectile.
	LaunchDirection = FVector::UpVector;

	if (HasAuthority())
	{
		ProjectileMovement->Velocity = LaunchDirection * ProjectileMovement->InitialSpeed;
		const UOBHelicopterSystemSettings* Settings = GetDefault<UOBHelicopterSystemSettings>();
		const float MaxFlightSeconds = Settings
			? FMath::Max(0.1f, Settings->SignalFlareMaxFlightSeconds)
			: FMath::Max(0.1f, FuseSeconds);
		GetWorldTimerManager().SetTimer(
			SafetyBurstTimer, this, &AOBSignalFlare::Burst, MaxFlightSeconds, false);
	}
	else
	{
		ProjectileMovement->Deactivate();
		SetActorTickEnabled(false);
	}

	if (bBurst)
	{
		PlayBurstPresentation();
	}
	else
	{
		PlayLaunchPresentation();
	}
}

void AOBSignalFlare::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || bBurst)
	{
		return;
	}

	const float HeightAboveLaunch = GetActorLocation().Z - LaunchLocation.Z;
	if (HeightAboveLaunch >= BurstHeight)
	{
		Burst();
	}
}

void AOBSignalFlare::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AOBSignalFlare, BurstLocation);
	DOREPLIFETIME(AOBSignalFlare, bBurst);
}

void AOBSignalFlare::ConfigureFromProjectSettings()
{
	const UOBHelicopterSystemSettings* Settings = GetDefault<UOBHelicopterSystemSettings>();
	if (!Settings)
	{
		ProjectileMovement->InitialSpeed = FMath::Max(100.f, LaunchSpeed);
		ProjectileMovement->MaxSpeed = ProjectileMovement->InitialSpeed;
		BurstHeight = 5000.f;
		DestroyDelayAfterBurst = FMath::Max(0.25f, LifeAfterBurst);
		return;
	}

	ProjectileMovement->InitialSpeed = FMath::Max(100.f, Settings->SignalFlareLaunchSpeed);
	ProjectileMovement->MaxSpeed = ProjectileMovement->InitialSpeed;
	ProjectileMovement->ProjectileGravityScale = FMath::Max(0.f, Settings->SignalFlareGravityScale);
	BurstHeight = FMath::Max(100.f, Settings->SignalFlareBurstHeight);
	DestroyDelayAfterBurst = FMath::Max(0.25f, Settings->SignalFlareDestroyDelay);
}

void AOBSignalFlare::Burst()
{
	if (!HasAuthority() || bBurst)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(SafetyBurstTimer);
	BurstLocation = GetActorLocation();
	bBurst = true;
	ProjectileMovement->StopMovementImmediately();
	ProjectileMovement->Deactivate();
	SetActorTickEnabled(false);
	ForceNetUpdate();
	PlayBurstPresentation();
	SetLifeSpan(DestroyDelayAfterBurst);
}

void AOBSignalFlare::OnRep_Burst()
{
	if (bBurst)
	{
		PlayBurstPresentation();
	}
}

void AOBSignalFlare::PlayLaunchPresentation()
{
	if (bLaunchPresentationPlayed || bBurst)
	{
		return;
	}
	bLaunchPresentationPlayed = true;

	const UOBHelicopterSystemSettings* Settings = GetDefault<UOBHelicopterSystemSettings>();
	UNiagaraSystem* ResolvedTrailSystem = Settings && !Settings->SignalFlareTrailSystem.IsNull()
		? Settings->SignalFlareTrailSystem.LoadSynchronous()
		: TrailSystem.Get();
	if (ResolvedTrailSystem)
	{
		SpawnedTrailComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			ResolvedTrailSystem, SceneRoot, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset, true);
	}

	USoundCue* ResolvedLaunchSound = Settings && !Settings->SignalFlareLaunchSound.IsNull()
		? Settings->SignalFlareLaunchSound.LoadSynchronous()
		: Cast<USoundCue>(LaunchSound.Get());
	if (ResolvedLaunchSound)
	{
		UOutBreakGlobal::PlaySoundAndReportNoise(
			this,
			ResolvedLaunchSound,
			GetActorLocation(),
			this,
			Settings ? Settings->SignalFlareLaunchNoiseTag : FName(TEXT("Flare")),
			Settings ? FMath::Max(0.01f, Settings->SignalFlareLaunchNoiseRangeScale) : 1.f);
	}
	BP_OnFlareLaunched();
}

void AOBSignalFlare::PlayBurstPresentation()
{
	if (bBurstPresentationPlayed)
	{
		return;
	}
	bBurstPresentationPlayed = true;
	StopOwnedNiagaraComponents();

	const UOBHelicopterSystemSettings* Settings = GetDefault<UOBHelicopterSystemSettings>();
	const FVector EffectLocation = bBurst ? FVector(BurstLocation) : GetActorLocation();
	const float EffectScale = Settings ? FMath::Max(0.01f, Settings->SignalFlareBurstEffectScale) : 1.f;
	UNiagaraSystem* ResolvedBurstSystem = Settings && !Settings->SignalFlareBurstSystem.IsNull()
		? Settings->SignalFlareBurstSystem.LoadSynchronous()
		: BurstSystem.Get();
	if (ResolvedBurstSystem)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			ResolvedBurstSystem,
			EffectLocation,
			FRotator::ZeroRotator,
			FVector(EffectScale),
			true,
			true,
			ENCPoolMethod::None,
			true);
	}

	UNiagaraSystem* ResolvedPostBurstSystem = Settings && !Settings->SignalFlarePostBurstSystem.IsNull()
		? Settings->SignalFlarePostBurstSystem.LoadSynchronous()
		: PersistentSmokeSystem.Get();
	if (ResolvedPostBurstSystem)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			ResolvedPostBurstSystem,
			EffectLocation,
			FRotator::ZeroRotator,
			FVector::OneVector,
			true,
			true,
			ENCPoolMethod::None,
			true);
	}

	USoundCue* ResolvedBurstSound = Settings && !Settings->SignalFlareBurstSound.IsNull()
		? Settings->SignalFlareBurstSound.LoadSynchronous()
		: Cast<USoundCue>(BurstSound.Get());
	if (ResolvedBurstSound)
	{
		UOutBreakGlobal::PlaySoundAndReportNoise(
			this,
			ResolvedBurstSound,
			EffectLocation,
			this,
			Settings ? Settings->SignalFlareBurstNoiseTag : FName(TEXT("Flare")),
			Settings ? FMath::Max(0.01f, Settings->SignalFlareBurstNoiseRangeScale) : 1.f);
	}
	BP_OnFlareBurst();
}

void AOBSignalFlare::StopOwnedNiagaraComponents()
{
	TInlineComponentArray<UNiagaraComponent*> NiagaraComponents(this);
	for (UNiagaraComponent* NiagaraComponent : NiagaraComponents)
	{
		if (IsValid(NiagaraComponent))
		{
			NiagaraComponent->Deactivate();
		}
	}
	SpawnedTrailComponent = nullptr;
}
