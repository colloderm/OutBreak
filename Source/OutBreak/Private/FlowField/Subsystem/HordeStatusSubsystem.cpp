// Fill out your copyright notice in the Description page of Project Settings.


#include "FlowField/Subsystem/HordeStatusSubsystem.h"

#include "FlowField/Subsystem/BudgetOverlordSubsystem.h"

void UHordeStatusSubsystem::AddDamageEvent(
	AActor* DamagedActor,
	const double Damage)
{
	check(IsInGameThread());
	check(BudgetOverlord)

	if (!IsValid(DamagedActor) || Damage <= 0.0)
	{
		return;
	}

	const TWeakObjectPtr<AActor> ActorKey(DamagedActor);

	if (const int32* ExistingIndex =
		DamageEventIndexMap.Find(ActorKey))
	{
		HordeDamageEvents[*ExistingIndex].Damage += Damage;
		return;
	}

	
	int32 AgentIndex = BudgetOverlord->GetIndexByActor(DamagedActor);
	const int32 NewIndex = HordeDamageEvents.Add(
		HordeDamageEvent
		{
			AgentIndex,
			ActorKey,
			Damage
		});
	
	DamageEventIndexMap.Add(ActorKey, NewIndex);
}

void UHordeStatusSubsystem::InitializeStorage(int32 Capacity)
{
	StatusStorage.Initialize(Capacity);
}

void UHordeStatusSubsystem::Register(float MaxHealth, float Percent)
{
	StatusStorage.Add(MaxHealth, Percent);
}

void UHordeStatusSubsystem::Unregister(int32 Index)
{
	StatusStorage.RemoveAtSwap(Index);
}

void UHordeStatusSubsystem::ProcessSystem(const float DeltaSeconds)
{
	Super::ProcessSystem(DeltaSeconds);
	
	Parallel();
	DeadCheck();
}

void UHordeStatusSubsystem::DeadCheck()
{
	for (int32 i = 0; i < StatusStorage.Size(); i++)
	{
		if(StatusStorage.CurrentHealths[i] <= 0.f)
		{
			BudgetOverlord->UnregisterAgent(i);
		}
	}
}

void UHordeStatusSubsystem::Parallel()
{
	check(IsInGameThread());
	
	int32 EventCount = HordeDamageEvents.Num();
	
	if (EventCount <= 0) return;
	
	
	float* CurrentHealths = StatusStorage.CurrentHealths.GetData();
	auto* Events = HordeDamageEvents.GetData();
	
	ParallelFor(
		TEXT("UHordeStatusSubsystem::Parallel"),
			EventCount,
			64,
			[Events,CurrentHealths](const int32 EventIndex)
			{
				const HordeDamageEvent& Event = Events[EventIndex];
				
				int32 StatusIndex = Event.StatusIndex;
				CurrentHealths[StatusIndex] = FMath::Max(0.0f, CurrentHealths[StatusIndex] - Event.Damage);
				
			});
}

void UHordeStatusSubsystem::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                              UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	
}
