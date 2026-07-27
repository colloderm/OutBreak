// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/EnemyController.h"

#include "AI/Components/EnemyMemoryComponent.h"
#include "AI/EnemyCharacter.h"
#include "Ability/Tags/OBGameplayTags.h"
#include "Character/OBCharacterBase.h"
#include "Components/StateTreeAIComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"
#include "StateTreeEvents.h"

AEnemyController::AEnemyController()
{
	PrimaryActorTick.bCanEverTick = true;

	SetGenericTeamId(FGenericTeamId(2));
	InitializeComponents();
}

ETeamAttitude::Type AEnemyController::GetTeamAttitudeTowards(
	const AActor& Other) const
{
	if (Other.IsA<AEnemyCharacter>())
	{
		return ETeamAttitude::Friendly;
	}

	if (Other.IsA<AOBCharacterBase>())
	{
		return ETeamAttitude::Hostile;
	}

	return ETeamAttitude::Neutral;
}

void AEnemyController::InitializeComponents()
{
	InitializeStateTree();
	InitializeAIPerception();
	InitializeMemoryComponent();
}

void AEnemyController::InitializeStateTree()
{
	StateTreeComponent =
		CreateDefaultSubobject<UStateTreeAIComponent>(
			TEXT("StateTreeComponent"));

	BrainComponent = StateTreeComponent;
	StateTreeComponent->SetStartLogicAutomatically(false);
}

void AEnemyController::InitializeAIPerception()
{
	AIPerceptionComponent =
		CreateDefaultSubobject<UAIPerceptionComponent>(
			TEXT("AIPerception"));

	SetPerceptionComponent(*AIPerceptionComponent);

	SightConfig =
		CreateDefaultSubobject<UAISenseConfig_Sight>(
			TEXT("SightConfig"));
	SightConfig->SightRadius = 2500.0f;
	SightConfig->LoseSightRadius = SightConfig->SightRadius * 2.0f;
	SightConfig->PeripheralVisionAngleDegrees = 70.0f;
	SightConfig->SetMaxAge(3.0f);
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = false;

	HearingConfig =
		CreateDefaultSubobject<UAISenseConfig_Hearing>(
			TEXT("HearingConfig"));
	HearingConfig->HearingRange = 3500.0f;
	HearingConfig->SetMaxAge(5.0f);
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;

	DamageConfig =
		CreateDefaultSubobject<UAISenseConfig_Damage>(
			TEXT("DamageConfig"));
	DamageConfig->SetMaxAge(10.0f);

	AIPerceptionComponent->ConfigureSense(*SightConfig);
	AIPerceptionComponent->ConfigureSense(*HearingConfig);
	AIPerceptionComponent->ConfigureSense(*DamageConfig);
	AIPerceptionComponent->SetDominantSense(
		UAISense_Sight::StaticClass());
}

void AEnemyController::ApplySightAffiliationFilter()
{
	if (!IsValid(AIPerceptionComponent))
	{
		return;
	}

	UAISenseConfig_Sight* ActiveSightConfig =
		AIPerceptionComponent->GetSenseConfig<UAISenseConfig_Sight>();

	if (!IsValid(ActiveSightConfig))
	{
		ActiveSightConfig = SightConfig.Get();
	}

	if (!IsValid(ActiveSightConfig))
	{
		return;
	}

	ActiveSightConfig->DetectionByAffiliation.bDetectEnemies = true;
	ActiveSightConfig->DetectionByAffiliation.bDetectFriendlies = false;
	ActiveSightConfig->DetectionByAffiliation.bDetectNeutrals = false;
	AIPerceptionComponent->ConfigureSense(*ActiveSightConfig);
}

void AEnemyController::InitializeMemoryComponent()
{
	// Keep the existing subobject name for Blueprint template compatibility.
	EnemyMemoryComponent =
		CreateDefaultSubobject<UEnemyMemoryComponent>(
			TEXT("MemoryComponet"));
}

void AEnemyController::Dead()
{
	bIsDead = true;

	APawn* ControlledPawn = GetPawn();
	if (IsValid(ControlledPawn))
	{
		StopMovement();
		UnPossess();
	}
}

void AEnemyController::BeginPlay()
{
	Super::BeginPlay();

	if (!ensureAlwaysMsgf(
		IsValid(AIPerceptionComponent),
		TEXT("%s::%s: AIPerceptionComponent is invalid or missing."),
		*GetClass()->GetName(),
		TEXT(__FUNCTION__)))
	{
		return;
	}

	ApplySightAffiliationFilter();

	AIPerceptionComponent->OnTargetPerceptionUpdated.AddUniqueDynamic(
		this,
		&AEnemyController::HandleTargetPerceptionUpdated);
	AIPerceptionComponent->OnTargetPerceptionForgotten.AddUniqueDynamic(
		this,
		&AEnemyController::HandleTargetPerceptionForgotten);

	if (ensureAlwaysMsgf(
		IsValid(EnemyMemoryComponent),
		TEXT("%s::%s: EnemyMemoryComponent is invalid."),
		*GetClass()->GetName(),
		TEXT(__FUNCTION__)))
	{
		EnemyMemoryComponent->OnMemoryUpdated.AddUObject(
			this,
			&AEnemyController::HandleMemoryUpdated);
	}
}

void AEnemyController::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(EnemyMemoryComponent))
	{
		EnemyMemoryComponent->OnMemoryUpdated.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AEnemyController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!ensureAlwaysMsgf(
		IsValid(StateTreeComponent),
		TEXT("%s::%s: StateTreeComponent is invalid."),
		*GetClass()->GetName(),
		TEXT(__FUNCTION__)))
	{
		return;
	}

	StateTreeComponent->StartLogic();
}

void AEnemyController::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AEnemyController::HandleTargetPerceptionUpdated(
	AActor* UpdatedActor,
	FAIStimulus Stimulus)
{
	if (!IsValid(EnemyMemoryComponent))
	{
		return;
	}

	EnemyMemoryComponent->UpdateFromPerception(
		UpdatedActor,
		Stimulus);
}

void AEnemyController::HandleTargetPerceptionForgotten(
	AActor* UpdatedActor)
{
	if (!IsValid(EnemyMemoryComponent))
	{
		return;
	}

	EnemyMemoryComponent->HandlePerceptionForgotten(
		UpdatedActor);
}

void AEnemyController::HandleMemoryUpdated()
{
	if (!IsValid(StateTreeComponent))
	{
		return;
	}

	StateTreeComponent->SendStateTreeEvent(
		FStateTreeEvent(
			OBGameplayTags::TAG_StateTree_Event_MemoryUpdated));
}
