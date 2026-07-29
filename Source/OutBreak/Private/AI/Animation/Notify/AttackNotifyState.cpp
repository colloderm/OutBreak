#include "AI/Animation/Notify/AttackNotifyState.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/HitResult.h"
#include "GameFramework/Actor.h"
#include "Kismet/KismetSystemLibrary.h"

UAttackNotifyState::UAttackNotifyState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
}

void UAttackNotifyState::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!IsValid(MeshComp))
	{
		return;
	}

	RemoveStaleRuntimeStates();

	const TWeakObjectPtr<USkeletalMeshComponent> MeshKey(MeshComp);
	RuntimeStates.Remove(MeshKey);

	if (!CanRunTrace(MeshComp))
	{
		return;
	}

	RuntimeStates.Add(MeshKey);
	TraceAndCollect(MeshComp);
}

void UAttackNotifyState::NotifyTick(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (!IsValid(MeshComp))
	{
		return;
	}

	const TWeakObjectPtr<USkeletalMeshComponent> MeshKey(MeshComp);
	if (RuntimeStates.Contains(MeshKey))
	{
		TraceAndCollect(MeshComp);
	}
}

void UAttackNotifyState::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (IsValid(MeshComp))
	{
		const TWeakObjectPtr<USkeletalMeshComponent> MeshKey(MeshComp);
		if (FAttackTraceRuntimeState* RuntimeState = RuntimeStates.Find(MeshKey))
		{
			TArray<AActor*> CollectedActors;
			CollectedActors.Reserve(RuntimeState->CollectedActors.Num());

			for (const TWeakObjectPtr<AActor>& CollectedActor : RuntimeState->CollectedActors)
			{
				if (AActor* Actor = CollectedActor.Get())
				{
					CollectedActors.Add(Actor);
				}
			}

			RuntimeStates.Remove(MeshKey);
			ProcessCollectedActors(MeshComp, MeshComp->GetOwner(), CollectedActors);
		}
	}

	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UAttackNotifyState::GetNotifyName_Implementation() const
{
	if (StartSocketName.IsNone() || EndSocketName.IsNone())
	{
		return TEXT("Attack Trace");
	}

	return FString::Printf(
		TEXT("Attack Trace (%s -> %s)"),
		*StartSocketName.ToString(),
		*EndSocketName.ToString());
}

void UAttackNotifyState::ProcessCollectedActors_Implementation(
	USkeletalMeshComponent*,
	AActor*,
	const TArray<AActor*>&) const
{
}

void UAttackNotifyState::TraceAndCollect(USkeletalMeshComponent* MeshComp)
{
	if (!IsValid(MeshComp))
	{
		return;
	}

	const TWeakObjectPtr<USkeletalMeshComponent> MeshKey(MeshComp);
	FAttackTraceRuntimeState* RuntimeState = RuntimeStates.Find(MeshKey);
	if (!RuntimeState)
	{
		return;
	}

	const FVector StartLocation = MeshComp->GetSocketLocation(StartSocketName);
	const FVector EndLocation = MeshComp->GetSocketLocation(EndSocketName);
	AActor* MeshOwner = MeshComp->GetOwner();

	TArray<AActor*> ActorsToIgnore;
	if (bIgnoreMeshOwner && IsValid(MeshOwner))
	{
		ActorsToIgnore.Add(MeshOwner);
	}

	TArray<FHitResult> Hits;
	const float EffectiveTraceRadius = FMath::Max(TraceRadius, 0.1f);
	const EDrawDebugTrace::Type DrawDebugType = !bDrawDebugTrace
		? EDrawDebugTrace::None
		: DebugDrawDuration > 0.0f
			? EDrawDebugTrace::ForDuration
			: EDrawDebugTrace::ForOneFrame;

	UKismetSystemLibrary::SphereTraceMultiForObjects(
		MeshComp,
		StartLocation,
		EndLocation,
		EffectiveTraceRadius,
		TraceObjectTypes,
		bTraceComplex,
		ActorsToIgnore,
		DrawDebugType,
		Hits,
		false,
		FLinearColor::Red,
		FLinearColor::Green,
		DebugDrawDuration);

	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if (!ShouldIgnoreActor(HitActor, MeshOwner))
		{
			RuntimeState->CollectedActors.Add(TWeakObjectPtr<AActor>(HitActor));
		}
	}
}

bool UAttackNotifyState::ShouldIgnoreActor(
	const AActor* Candidate,
	const AActor* MeshOwner) const
{
	if (!IsValid(Candidate) || (bIgnoreMeshOwner && Candidate == MeshOwner))
	{
		return true;
	}

	for (const TSubclassOf<AActor>& IgnoredClass : IgnoredActorClasses)
	{
		if (IgnoredClass && Candidate->IsA(IgnoredClass))
		{
			return true;
		}
	}

	return false;
}

bool UAttackNotifyState::CanRunTrace(const USkeletalMeshComponent* MeshComp) const
{
	if (!IsValid(MeshComp) ||
		StartSocketName.IsNone() ||
		EndSocketName.IsNone() ||
		!MeshComp->DoesSocketExist(StartSocketName) ||
		!MeshComp->DoesSocketExist(EndSocketName) ||
		TraceObjectTypes.IsEmpty())
	{
		return false;
	}

	const AActor* MeshOwner = MeshComp->GetOwner();
	return !bAuthorityOnly || !IsValid(MeshOwner) || MeshOwner->HasAuthority();
}

void UAttackNotifyState::RemoveStaleRuntimeStates()
{
	for (auto It = RuntimeStates.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}
