#include "FlowField/ModularAnimationBudgetWorldSubsystem.h"

#include "Engine/World.h"
#include "IAnimationBudgetAllocator.h"
#include "FlowField/ModularSkeletalMeshProxyActor.h"

bool UModularAnimationBudgetWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	const UWorld* World = Cast<UWorld>(Outer);
	return World != nullptr && World->IsGameWorld();
}

void UModularAnimationBudgetWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (InWorld.GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (IAnimationBudgetAllocator* Allocator = IAnimationBudgetAllocator::Get(&InWorld))
	{
		Allocator->SetEnabled(true);
		UE_LOG(LogModularAnimationProxy, Log, TEXT("Animation Budget Allocator enabled for world %s."), *InWorld.GetName());
	}
}
