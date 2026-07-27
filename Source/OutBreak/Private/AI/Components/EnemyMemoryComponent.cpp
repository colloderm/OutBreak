// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Components/EnemyMemoryComponent.h"

#include "AI/EnemyController.h"
#include "AISystem.h"
#include "Perception/AIPerceptionSystem.h"
#include "Perception/AISense_Damage.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"

// Sets default values for this component's properties
UEnemyMemoryComponent::UEnemyMemoryComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
}


// Called when the game starts
void UEnemyMemoryComponent::BeginPlay()
{
	Super::BeginPlay();
}


// Called every frame
void UEnemyMemoryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const double CurrentTime = GetCurrentTimeSeconds();
	bool bMemoryUpdated = false;

	if (TargetActor && !IsValid(TargetActor))
	{
		TargetActor = nullptr;
		LastKnownTargetLocation = FVector::ZeroVector;
		bTargetVisible = false;
		bMemoryUpdated = true;
	}
	else if (HasValidTarget() && !bTargetVisible &&
		CurrentTime - LastTargetSeenTime >=
		FMath::Max(0.0f, TargetMemoryDuration))
	{
		TargetActor = nullptr;
		LastKnownTargetLocation = FVector::ZeroVector;
		bMemoryUpdated = true;
	}

	if (StimulusType != EEnemyStimulusType::None &&
		CurrentTime - LastStimulusTime >=
		FMath::Max(0.0f, StimulusMemoryDuration))
	{
		ClearStimulusMemory();
		bMemoryUpdated = true;
	}

	if (bMemoryUpdated)
	{
		BroadcastMemoryUpdated();
	}
}

void UEnemyMemoryComponent::UpdateFromPerception(
	AActor* UpdatedActor,
	const FAIStimulus& Stimulus)
{
	if (!IsValid(UpdatedActor))
	{
		return;
	}

	const TSubclassOf<UAISense> SenseClass =
		UAIPerceptionSystem::GetSenseClassForStimulus(
			GetOwner(),
			Stimulus);

	if (SenseClass == UAISense_Sight::StaticClass())
	{
		UpdateSightMemory(UpdatedActor, Stimulus);
	}
	else if (SenseClass == UAISense_Hearing::StaticClass())
	{
		UpdateHearingMemory(UpdatedActor, Stimulus);
	}
	else if (SenseClass == UAISense_Damage::StaticClass())
	{
		UpdateDamageMemory(UpdatedActor, Stimulus);
	}
}

void UEnemyMemoryComponent::HandlePerceptionForgotten(
	AActor* ForgottenActor)
{
	if (TargetActor != ForgottenActor || !bTargetVisible)
	{
		return;
	}

	bTargetVisible = false;
	BroadcastMemoryUpdated();
}

float UEnemyMemoryComponent::GetTimeSinceTargetSeen() const
{
	if (!HasValidTarget() || bTargetVisible)
	{
		return 0.0f;
	}

	return static_cast<float>(
		FMath::Max(0.0, GetCurrentTimeSeconds() - LastTargetSeenTime));
}

void UEnemyMemoryComponent::UpdateSightMemory(
	AActor* UpdatedActor,
	const FAIStimulus& Stimulus)
{
	const AEnemyController* EnemyController =
		Cast<AEnemyController>(GetOwner());

	if (!IsValid(EnemyController) ||
		EnemyController->GetTeamAttitudeTowards(*UpdatedActor) !=
		ETeamAttitude::Hostile)
	{
		return;
	}

	const double CurrentTime = GetCurrentTimeSeconds();

	if (Stimulus.WasSuccessfullySensed())
	{
		TargetActor = UpdatedActor;
		bTargetVisible = true;
		LastKnownTargetLocation = UpdatedActor->GetActorLocation();
		LastTargetSeenTime = CurrentTime;

		if (StimulusType == EEnemyStimulusType::LostSight)
		{
			ClearStimulusMemory();
		}

		BroadcastMemoryUpdated();
		return;
	}

	if (TargetActor != UpdatedActor)
	{
		return;
	}

	bTargetVisible = false;
	LastKnownTargetLocation =
		ResolveStimulusLocation(*UpdatedActor, Stimulus);
	LastTargetSeenTime = CurrentTime;

	StimulusType = EEnemyStimulusType::LostSight;
	LastStimulusLocation = LastKnownTargetLocation;
	LastHeardLocation = FVector::ZeroVector;
	LastDamageDirection = FVector::ZeroVector;
	LastStimulusTime = CurrentTime;

	BroadcastMemoryUpdated();
}

void UEnemyMemoryComponent::UpdateHearingMemory(
	AActor* UpdatedActor,
	const FAIStimulus& Stimulus)
{
	if (!Stimulus.WasSuccessfullySensed())
	{
		return;
	}

	StimulusType = EEnemyStimulusType::Hearing;
	LastHeardLocation =
		ResolveStimulusLocation(*UpdatedActor, Stimulus);
	LastStimulusLocation = LastHeardLocation;
	LastDamageDirection = FVector::ZeroVector;
	LastStimulusTime = GetCurrentTimeSeconds();

	BroadcastMemoryUpdated();
}

void UEnemyMemoryComponent::UpdateDamageMemory(
	AActor* UpdatedActor,
	const FAIStimulus& Stimulus)
{
	if (!Stimulus.WasSuccessfullySensed())
	{
		return;
	}

	const bool bWasVisible =
		TargetActor == UpdatedActor && bTargetVisible;
	const double CurrentTime = GetCurrentTimeSeconds();

	TargetActor = UpdatedActor;
	bTargetVisible = bWasVisible;
	LastKnownTargetLocation =
		ResolveStimulusLocation(*UpdatedActor, Stimulus);
	LastTargetSeenTime = CurrentTime;

	StimulusType = EEnemyStimulusType::Damage;
	LastStimulusLocation = LastKnownTargetLocation;
	LastHeardLocation = FVector::ZeroVector;
	LastDamageDirection =
		ResolveDamageDirection(*UpdatedActor, Stimulus);
	LastStimulusTime = CurrentTime;

	BroadcastMemoryUpdated();
}

void UEnemyMemoryComponent::ClearStimulusMemory()
{
	StimulusType = EEnemyStimulusType::None;
	LastStimulusLocation = FVector::ZeroVector;
	LastHeardLocation = FVector::ZeroVector;
	LastDamageDirection = FVector::ZeroVector;
}

void UEnemyMemoryComponent::BroadcastMemoryUpdated()
{
	OnMemoryUpdated.Broadcast();
}

FVector UEnemyMemoryComponent::ResolveStimulusLocation(
	const AActor& UpdatedActor,
	const FAIStimulus& Stimulus) const
{
	return FAISystem::IsValidLocation(Stimulus.StimulusLocation)
		? Stimulus.StimulusLocation
		: UpdatedActor.GetActorLocation();
}

FVector UEnemyMemoryComponent::ResolveDamageDirection(
	const AActor& UpdatedActor,
	const FAIStimulus& Stimulus) const
{
	const FVector DamageSourceLocation =
		ResolveStimulusLocation(UpdatedActor, Stimulus);

	FVector ReceiverLocation = Stimulus.ReceiverLocation;
	if (!FAISystem::IsValidLocation(ReceiverLocation))
	{
		const AEnemyController* EnemyController =
			Cast<AEnemyController>(GetOwner());
		const APawn* ControlledPawn =
			IsValid(EnemyController)
				? EnemyController->GetPawn()
				: nullptr;

		ReceiverLocation = IsValid(ControlledPawn)
			? ControlledPawn->GetActorLocation()
			: GetOwner()->GetActorLocation();
	}

	FVector SourceDirection =
		(DamageSourceLocation - ReceiverLocation).GetSafeNormal();
	if (SourceDirection.IsNearlyZero())
	{
		SourceDirection =
			(UpdatedActor.GetActorLocation() - ReceiverLocation)
			.GetSafeNormal();
	}

	return SourceDirection;
}

double UEnemyMemoryComponent::GetCurrentTimeSeconds() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.0;
}

