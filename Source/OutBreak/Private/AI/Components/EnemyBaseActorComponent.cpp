// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Components/EnemyBaseActorComponent.h"

#include  "AI/EnemyCharacter.h"


// Sets default values for this component's properties
UEnemyBaseActorComponent::UEnemyBaseActorComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	
	EnemyCharacter = Cast<AEnemyCharacter>(GetOwner());

	// ...
}


// Called when the game starts
void UEnemyBaseActorComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UEnemyBaseActorComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                             FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

