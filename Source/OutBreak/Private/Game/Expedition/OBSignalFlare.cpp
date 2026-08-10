#include "Game/Expedition/OBSignalFlare.h"

#include "Components/SceneComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"

AOBSignalFlare::AOBSignalFlare()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(true);
	SetNetUpdateFrequency(20.f);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->InitialSpeed = LaunchSpeed;
	ProjectileMovement->MaxSpeed = LaunchSpeed;
	ProjectileMovement->ProjectileGravityScale = 0.15f;
	ProjectileMovement->bRotationFollowsVelocity = true;
}

void AOBSignalFlare::BeginPlay()
{
	Super::BeginPlay();
	PlayLaunchPresentation();

	if (HasAuthority())
	{
		ProjectileMovement->Velocity = GetActorUpVector() * LaunchSpeed;
		GetWorldTimerManager().SetTimer(BurstTimer, this, &AOBSignalFlare::Burst, FuseSeconds, false);
	}
	else
	{
		ProjectileMovement->Deactivate();
	}
}

void AOBSignalFlare::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AOBSignalFlare, bBurst);
}

void AOBSignalFlare::Burst()
{
	if (!HasAuthority() || bBurst)
	{
		return;
	}

	bBurst = true;
	ProjectileMovement->StopMovementImmediately();
	ProjectileMovement->Deactivate();
	ForceNetUpdate();
	PlayBurstPresentation();
	SetLifeSpan(LifeAfterBurst);
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
	if (TrailSystem)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(
			TrailSystem, SceneRoot, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset, true);
	}
	if (LaunchSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, LaunchSound, GetActorLocation());
	}
	BP_OnFlareLaunched();
}

void AOBSignalFlare::PlayBurstPresentation()
{
	if (BurstSystem)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, BurstSystem, GetActorLocation());
	}
	if (PersistentSmokeSystem)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(
			PersistentSmokeSystem, SceneRoot, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset, true);
	}
	if (BurstSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, BurstSound, GetActorLocation());
	}
	BP_OnFlareBurst();
}
