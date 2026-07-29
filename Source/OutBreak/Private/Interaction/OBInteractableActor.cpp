// Fill out your copyright notice in the Description page of Project Settings.


#include "Interaction/OBInteractableActor.h"

#include "Blueprint/UserWidget.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Pawn.h"
#include "Player/Controller/OBPlayerController.h"


AOBInteractableActor::AOBInteractableActor()
{
	PrimaryActorTick.bCanEverTick = false;
	
	Range = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	SetRootComponent(Range);
	Range->InitSphereRadius(InteractRadius);
	Range->SetCollisionProfileName(TEXT("Trigger"));
}

void AOBInteractableActor::BeginPlay()
{
	Super::BeginPlay();
	
	Range->SetSphereRadius(InteractRadius);
	Range->OnComponentBeginOverlap.AddDynamic(this, &AOBInteractableActor::OnRangeBeginOverlap);
	Range->OnComponentEndOverlap.AddDynamic(this, &AOBInteractableActor::OnRangeEndOverlap);
}

void AOBInteractableActor::OnRangeBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn) return;
	
	// Home은 로컬 레벨 -> 로컬 컨트롤러에만 등록
	if (AOBPlayerController* PC = Cast<AOBPlayerController>(Pawn->GetController()))
	{
		if (PC->IsLocalController())
		{
			PC->SetCurrentInteractable(this);
		}
	}
}

void AOBInteractableActor::OnRangeEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn) return;
	
	if (AOBPlayerController* PC = Cast<AOBPlayerController>(Pawn->GetController()))
	{
		if (PC->IsLocalController() && PC->GetCurrentInteractable() == this)
		{
			PC->SetCurrentInteractable(nullptr);
		}
	}
}

void AOBInteractableActor::Interact_Implementation(AOBPlayerController* PC)
{
	// 기본: 지정 위젯을 컨트롤러가 오픈(커서/이동잠금 포함)
	if (PC && InteractWidgetClass)
	{
		PC->OpenInteractionWidget(InteractWidgetClass);
	}
}
