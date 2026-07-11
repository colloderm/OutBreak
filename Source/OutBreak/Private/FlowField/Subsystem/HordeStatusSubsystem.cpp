// Fill out your copyright notice in the Description page of Project Settings.


#include "FlowField/Subsystem/HordeStatusSubsystem.h"

#include "FlowField/Subsystem/BudgetOverlordSubsystem.h"
#include "FlowField/Subsystem/HordeProxySubsystem.h"

namespace
{
struct FResolvedHordeDamageEvent
{
	int32 PackedIndex = INDEX_NONE;
	double Damage = 0.0;
};
}

void UHordeStatusSubsystem::AddDamageEvent(
	AActor* DamagedActor,
	const double Damage)
{
	check(IsInGameThread());
	check(BudgetOverlord)

	if (!IsValid(DamagedActor)
		|| Damage <= 0.0
		|| !FMath::IsFinite(Damage))
	{
		return;
	}

	FHordeAgentHandle Handle;
	if (!BudgetOverlord->TryGetHandleByActor(
		DamagedActor,
		Handle))
	{
		return;
	}

	int32 PackedIndex = INDEX_NONE;
	if (!BudgetOverlord->TryResolvePackedIndex(
		Handle,
		PackedIndex))
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

	const int32 NewIndex = HordeDamageEvents.Add(
		HordeDamageEvent
		{
			Handle,
			ActorKey,
			Damage
		});
	
	DamageEventIndexMap.Add(ActorKey, NewIndex);
}

void UHordeStatusSubsystem::InitializeStorage(int32 Capacity)
{
	StatusStorage.Initialize(Capacity);
}

int32 UHordeStatusSubsystem::Register(float MaxHealth, float Percent)
{
	check(IsInGameThread());

	const int32 PackedIndex =
		StatusStorage.Add(MaxHealth, Percent);

	check(StatusStorage.IsValid());

	return PackedIndex;
}

void UHordeStatusSubsystem::Unregister(int32 Index)
{
	check(IsInGameThread());
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
	check(IsInGameThread());

	TArray<FHordeAgentHandle> PendingDeadAgents;

	for (int32 PackedIndex = 0;
		 PackedIndex < StatusStorage.Size();
		 ++PackedIndex)
	{
		if (StatusStorage.CurrentHealths[PackedIndex] <= 0.f)
		{
			const FHordeAgentHandle Handle =
				BudgetOverlord->GetHandleByPackedIndex(
					PackedIndex);

			if (Handle.IsValid()
				&& !PendingDeadAgents.Contains(Handle))
			{
				PendingDeadAgents.Add(Handle);
			}
		}
	}

	for (const FHordeAgentHandle& Handle : PendingDeadAgents)
	{
		BudgetOverlord->UnregisterAgent(Handle);
	}
}

void UHordeStatusSubsystem::Parallel()
{
	check(IsInGameThread());
	check(StatusStorage.IsValid());
	
	const int32 EventCount = HordeDamageEvents.Num();
	
	if (EventCount <= 0)
	{
		DamageEventIndexMap.Reset();
		return;
	}

	TArray<FResolvedHordeDamageEvent> ResolvedEvents;
	ResolvedEvents.Reserve(EventCount);

	for (const HordeDamageEvent& Event : HordeDamageEvents)
	{
		if (!Event.Handle.IsValid()
			|| !Event.DamagedActor.IsValid())
		{
			continue;
		}

		int32 PackedIndex = INDEX_NONE;
		if (!BudgetOverlord->TryResolvePackedIndex(
			Event.Handle,
			PackedIndex))
		{
			continue;
		}

		if (!StatusStorage.CurrentHealths.IsValidIndex(PackedIndex))
		{
			continue;
		}

		BudgetOverlord->GetProxySubsystem()->PlayHordeAgentMontage(PackedIndex, EHordeAnimationDataIndex::Hit_RightShoulder, 0.5f, 1.2f);
		ResolvedEvents.Add(
			FResolvedHordeDamageEvent
			{
				PackedIndex,
				Event.Damage
			});
	}

	HordeDamageEvents.Reset();
	DamageEventIndexMap.Reset();

	if (ResolvedEvents.IsEmpty())
	{
		return;
	}
	
	float* CurrentHealths = StatusStorage.CurrentHealths.GetData();
	const FResolvedHordeDamageEvent* Events =
		ResolvedEvents.GetData();
	
	ParallelFor(
		TEXT("UHordeStatusSubsystem::Parallel"),
			ResolvedEvents.Num(),
			64,
			[Events,CurrentHealths](const int32 EventIndex)
			{
				const FResolvedHordeDamageEvent& Event =
					Events[EventIndex];
				
				CurrentHealths[Event.PackedIndex] =
					FMath::Max(
						0.0f,
						CurrentHealths[Event.PackedIndex]
						- Event.Damage);
				
			});
}

void UHordeStatusSubsystem::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                              UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	
}
