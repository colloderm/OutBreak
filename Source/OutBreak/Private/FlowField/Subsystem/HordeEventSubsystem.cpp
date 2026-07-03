// Fill out your copyright notice in the Description page of Project Settings.


#include "FlowField/Subsystem/HordeEventSubsystem.h"

void UHordeEventSubsystem::AddDamageEvent(AActor* DamagedActor,const double Damage)
{
	if (!IsValid(DamagedActor) || Damage <= 0.0)
	{
		return;
	}
	
	HordeDamageEvent Event = { DamagedActor, Damage };
	HordeDamageEvents.Add(Event);
}

void UHordeEventSubsystem::ProcessSystem(const float DeltaSeconds)
{
	Super::ProcessSystem(DeltaSeconds);
	
	
}

void UHordeEventSubsystem::ProcessEvent()
{
	for (HordeDamageEvent& Event : HordeDamageEvents)
	{
		
	}
	
	HordeDamageEvents.Empty();
}

void UHordeEventSubsystem::Parallel()
{
}

void UHordeEventSubsystem::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                              UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	
}
