// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/EnemyController.h"

#include "Components/StateTreeAIComponent.h"
#include "Perception/AIPerceptionComponent.h"

#include "Perception/AISense.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Damage.h"

#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Damage.h"

#include "Ability/Tags/OBGameplayTags.h"
#include "StateTreeEvents.h"
#include "Evaluation/IMovieSceneEvaluationHook.h"


// Sets default values
AEnemyController::AEnemyController()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	InitializeComponents();
	
}

void AEnemyController::InitializeComponents()
{
	InitializeStateTree();
	InitializeAIPerception();
}

void AEnemyController::InitializeStateTree()
{
	StateTreeComponent = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeComponent"));
	
	/* Brain Component로 지정. */
	BrainComponent = StateTreeComponent;
	
	/* Pawn Possess 이후 명시적으로 시작하기 위해 자동 시작을 비활성화. */
	StateTreeComponent->SetStartLogicAutomatically(false);
}

void AEnemyController::InitializeAIPerception()
{
	AIPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));
	
	SetPerceptionComponent(*AIPerceptionComponent);
	
	/* ================================ Sight Setting ================================ */
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	
	SightConfig->SightRadius = 2500.0f;
	SightConfig->LoseSightRadius = SightConfig->SightRadius * 2;
	
	SightConfig->PeripheralVisionAngleDegrees = 70.f;
	
	SightConfig->SetMaxAge(3.f);
	
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	/* =============================================================================== */
	
	/* ================================ Hearing Setting ================================ */
	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	
	HearingConfig->HearingRange = 3500.f;
	HearingConfig->SetMaxAge(5.f);
	
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	/* =============================================================================== */
	
	/* ================================ Damage Setting ================================ */
	DamageConfig = CreateDefaultSubobject<UAISenseConfig_Damage>(TEXT("DamageConfig"));
	
	DamageConfig->SetMaxAge(10.f);
	/* =============================================================================== */
	
	AIPerceptionComponent->ConfigureSense(*SightConfig);
	AIPerceptionComponent->ConfigureSense(*HearingConfig);
	AIPerceptionComponent->ConfigureSense(*DamageConfig);
	
	/* 
	 * 여러 감각이 같은 액터를 감지했을 때 
	 * 시야에서 얻은 위치를 우선 사용합니다.
	 */
	AIPerceptionComponent->SetDominantSense(UAISense_Sight::StaticClass());
}

// Called when the game starts or when spawned
void AEnemyController::BeginPlay()
{
	Super::BeginPlay();

	if (!ensureAlwaysMsgf(IsValid(AIPerceptionComponent),
	                      TEXT("%s::%s: AIPerceptionComponent is invalid or missing."),
	                      *GetClass()->GetName(),
	                      TEXT(__FUNCTION__)))
	{
		return;
	}
	
	AIPerceptionComponent->OnTargetPerceptionUpdated.AddUniqueDynamic(
		this,
		&AEnemyController::HandleTargetPerceptionUpdated);
	
	AIPerceptionComponent->OnTargetPerceptionForgotten.AddUniqueDynamic(
		this,
		&AEnemyController::HandleTargetPerceptionForgotten);
	
}

void AEnemyController::OnPossess(APawn* inPawn)
{
	Super::OnPossess(inPawn);

	if (!ensureAlwaysMsgf(IsValid(StateTreeComponent),
	                      TEXT("%s::%s: StateTree Component is invalid."),
	                      *GetClass()->GetName(),
	                      TEXT(__FUNCTION__)))
	{
		return;
	}
	
	StateTreeComponent->StartLogic();
}

// Called every frame
void AEnemyController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}



void AEnemyController::HandleTargetPerceptionUpdated(AActor* UpdatedActor, FAIStimulus Stimulus)
{
	if (!IsValid(UpdatedActor))
	{
		return;
	}
	
	/*
	 * 이 자극이 Sight, Hearing, Damage 중
	 * 어느 Sense에서 발생했는지 구분합니다.
	 */
	const TSubclassOf<UAISense> SenseClass = 
		UAIPerceptionSystem::GetSenseClassForStimulus(
			this,
			Stimulus);
	
	if (SenseClass == UAISense_Sight::StaticClass())
	{
		HandleSightStimulus(UpdatedActor, Stimulus);
		return;
	}
	
	if (SenseClass == UAISense_Hearing::StaticClass())
	{
		HandleHearingStimulus(UpdatedActor, Stimulus);
		return;
	}
	
	if (SenseClass == UAISense_Damage::StaticClass())
	{
		HandleDamageStimulus(UpdatedActor, Stimulus);
		return;
	}
}

void AEnemyController::HandleTargetPerceptionForgotten(AActor* UpdatedActor)
{
	if (IsValid(PerceptionTarget))
	{
		if (PerceptionTarget == UpdatedActor)
		{
			PerceptionTarget = nullptr;
			bHasPerceptionTarget = false;
		}
	}
	
}

void AEnemyController::HandleSightStimulus(AActor* UpdatedActor, const FAIStimulus& Stimulus)
{
	if (Stimulus.WasSuccessfullySensed())
	{
		PerceptionTarget = UpdatedActor;
		bHasPerceptionTarget= true;
		LastKnownTargetLocation = Stimulus.StimulusLocation;
		AlertState = EEnemyAlertState::Combat;
	}
	else
	{
		bCanSeeTarget = false;
		LastKnownTargetLocation = Stimulus.StimulusLocation;
		
		if (IsValid(PerceptionTarget))
		{
			StateTreeComponent->SendStateTreeEvent(
			FStateTreeEvent(
				OBGameplayTags::TAG_StateTree_Event_TargetSighted));
			AlertState = EEnemyAlertState::Chase;
		}
	}
}

void AEnemyController::HandleHearingStimulus(AActor* UpdatedActor, const FAIStimulus& Stimulus)
{
	if (!Stimulus.WasSuccessfullySensed())
	{
		return;
	}
	
	LastKnownTargetLocation = Stimulus.StimulusLocation;
	
	if (!bCanSeeTarget)
	{
		AlertState = EEnemyAlertState::Investigating;
	}
}

void AEnemyController::HandleDamageStimulus(AActor* UpdatedActor, const FAIStimulus& Stimulus)
{
	if (!Stimulus.WasSuccessfullySensed())
	{
		return;
	}
	
	PerceptionTarget = UpdatedActor;
	LastKnownTargetLocation = Stimulus.StimulusLocation;
	AlertState = EEnemyAlertState::Combat;
}



