// Copyright Epic Games, Inc. All Rights Reserved.

#include "FlowField/FlowFieldSubsystem.h"

#include "EngineUtils.h"
#include "FlowField/FlowFieldDensityComponent.h"
#include "FlowField/FlowFieldRecastNavMesh.h"
#include "GameFramework/Actor.h"
#include "FlowField/FlowFieldAgentPawn.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY(LogFlowField);

namespace
{
	TAutoConsoleVariable<int32> CVarFlowFieldNetDiagnostics(
		TEXT("OutBreak.FlowField.NetDiagnostics"),
		0,
		TEXT("Enables throttled diagnostics for Flow Field goal and movement queries."),
		ECVF_Default);

	const TCHAR* NetModeToString(const ENetMode NetMode)
	{
		switch (NetMode)
		{
		case NM_Standalone:
			return TEXT("Standalone");
		case NM_DedicatedServer:
			return TEXT("DedicatedServer");
		case NM_ListenServer:
			return TEXT("ListenServer");
		case NM_Client:
			return TEXT("Client");
		default:
			return TEXT("Unknown");
		}
	}

	float ComputeDensity2D(
		const FVector& PositionA,
		const FVector& PositionB,
		const float InfluenceRadius)
	{
		if (InfluenceRadius <= KINDA_SMALL_NUMBER)
		{
			return 0.0f;
		}

		FVector Delta = PositionA - PositionB;
		Delta.Z = 0.0f;
		const float Distance = Delta.Size();
		return FMath::Clamp(1.0f - Distance / InfluenceRadius, 0.0f, 1.0f);
	}
}

bool IsFlowFieldNetworkDiagnosticsEnabled()
{
	return CVarFlowFieldNetDiagnostics.GetValueOnGameThread() != 0;
}

void UFlowFieldSubsystem::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	ApplyDensitySeparation(DeltaTime);
}

TStatId UFlowFieldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFlowFieldSubsystem, STATGROUP_Tickables);
}

AFlowFieldAgentPawn* UFlowFieldSubsystem::SpawnFlowFieldPawn(TSubclassOf<AFlowFieldAgentPawn> FlowFieldPawnClass,
	const FVector SpawnLocation)
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	
	if (!IsValid(FlowFieldPawnClass))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::%s: Invalid FlowFieldPawnClass."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return nullptr;
	}
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	
	AFlowFieldAgentPawn* SpawnPawn = World->SpawnActor<AFlowFieldAgentPawn>(
		FlowFieldPawnClass,
		SpawnLocation,
		FRotator::ZeroRotator,
		SpawnParams);
	
	if (!IsValid(SpawnPawn))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::%s: Failed to spawn FlowFieldPawn."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return nullptr;
	}
	
	return SpawnPawn;
}

void UFlowFieldSubsystem::DestroyFlowFieldPawn(AFlowFieldAgentPawn* FlowFieldPawn)
{
	if (!IsValid(FlowFieldPawn))
	{
		return;
	}
	
	UWorld* World = GetWorld();
	if (!IsValid(World) || World->GetNetMode() == NM_Client)
	{
		return;
	}
	
	FlowFieldPawn->Destroy();
}

void UFlowFieldSubsystem::RegisterDensityComponent(UFlowFieldDensityComponent* DensityComponent)
{
	if (IsValid(DensityComponent) && IsValid(DensityComponent->GetOwner()))
	{
		RegisteredDensityComponents.AddUnique(DensityComponent);
		PreviousDensityComponentPositions.FindOrAdd(DensityComponent) = DensityComponent->GetOwner()->GetActorLocation();
	}
}

void UFlowFieldSubsystem::UnregisterDensityComponent(UFlowFieldDensityComponent* DensityComponent)
{
	RegisteredDensityComponents.Remove(DensityComponent);
	PreviousDensityComponentPositions.Remove(DensityComponent);
}

void UFlowFieldSubsystem::ApplyDensitySeparation(const float DeltaTime)
{
	return;
	if (DeltaTime <= 0.0f)
	{
		return;
	}

	RegisteredDensityComponents.RemoveAll([](const TWeakObjectPtr<UFlowFieldDensityComponent>& DensityComponent)
	{
		return !DensityComponent.IsValid() || !IsValid(DensityComponent->GetOwner());
	});
	for (auto It = PreviousDensityComponentPositions.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || !IsValid(It.Key()->GetOwner()))
		{
			It.RemoveCurrent();
		}
	}

	TArray<FVector> VelocityDeltas;
	VelocityDeltas.Init(FVector::ZeroVector, RegisteredDensityComponents.Num());
	TArray<FVector> ComponentPositions;
	TArray<FVector> MeasuredVelocities;
	ComponentPositions.Init(FVector::ZeroVector, RegisteredDensityComponents.Num());
	MeasuredVelocities.Init(FVector::ZeroVector, RegisteredDensityComponents.Num());

	for (int32 Index = 0; Index < RegisteredDensityComponents.Num(); ++Index)
	{
		const UFlowFieldDensityComponent* DensityComponent = RegisteredDensityComponents[Index].Get();
		const AActor* Owner = DensityComponent != nullptr ? DensityComponent->GetOwner() : nullptr;
		if (!IsValid(DensityComponent) || !IsValid(Owner))
		{
			continue;
		}

		ComponentPositions[Index] = Owner->GetActorLocation();
		if (const FVector* PreviousPosition = PreviousDensityComponentPositions.Find(RegisteredDensityComponents[Index]))
		{
			MeasuredVelocities[Index] = (ComponentPositions[Index] - *PreviousPosition) / DeltaTime;
			MeasuredVelocities[Index].Z = 0.0f;
		}
	}

	for (int32 IndexA = 0; IndexA < RegisteredDensityComponents.Num(); ++IndexA)
	{
		UFlowFieldDensityComponent* DensityComponentA = RegisteredDensityComponents[IndexA].Get();
		if (!IsValid(DensityComponentA) || !DensityComponentA->IsDensitySeparationEnabled())
		{
			continue;
		}

		for (int32 IndexB = IndexA + 1; IndexB < RegisteredDensityComponents.Num(); ++IndexB)
		{
			UFlowFieldDensityComponent* DensityComponentB = RegisteredDensityComponents[IndexB].Get();
			if (!IsValid(DensityComponentB) || !DensityComponentB->IsDensitySeparationEnabled())
			{
				continue;
			}

			const float InfluenceRadius = FMath::Min(DensityComponentA->SeparationRadius, DensityComponentB->SeparationRadius);
			const float RadiusSquared = InfluenceRadius * InfluenceRadius;

			FVector Delta = ComponentPositions[IndexA] - ComponentPositions[IndexB];
			Delta.Z = 0.0f;
			const float DistanceSquared = Delta.SizeSquared();
			if (DistanceSquared >= RadiusSquared)
			{
				continue;
			}

			FVector PushDirection;
			float Density = 1.0f;
			if (DistanceSquared <= KINDA_SMALL_NUMBER)
			{
				const float Sign = ((IndexA + IndexB) & 1) == 0 ? 1.0f : -1.0f;
				PushDirection = FVector(Sign, -Sign, 0.0f).GetSafeNormal();
			}
			else
			{
				const float Distance = FMath::Sqrt(DistanceSquared);
				PushDirection = Delta / Distance;
				Density = ComputeDensity2D(ComponentPositions[IndexA], ComponentPositions[IndexB], InfluenceRadius);
			}

			const FVector RelativeVelocity = MeasuredVelocities[IndexA] - MeasuredVelocities[IndexB];
			const float ClosingSpeed = FMath::Max(0.0f, -FVector::DotProduct(RelativeVelocity, PushDirection));
			const float PredictionTime = (DensityComponentA->SeparationPredictionTime + DensityComponentB->SeparationPredictionTime) * 0.5f;
			const float PredictedDistance = FMath::Max(0.0f, FMath::Sqrt(DistanceSquared) - ClosingSpeed * PredictionTime);
			const float PredictedDensity = FMath::Clamp(1.0f - PredictedDistance / InfluenceRadius, 0.0f, 1.0f);
			const float DensityExponent = (DensityComponentA->SeparationDensityExponent + DensityComponentB->SeparationDensityExponent) * 0.5f;
			Density = FMath::Pow(FMath::Max(Density, PredictedDensity), DensityExponent);

			const float SeparationStrength = (DensityComponentA->SeparationStrength + DensityComponentB->SeparationStrength) * 0.5f;
			const float ClosingBrakeAcceleration = PredictionTime > KINDA_SMALL_NUMBER
				? ClosingSpeed * PredictedDensity / (PredictionTime * 2.0f)
				: 0.0f;
			const FVector SeparationAcceleration =
				PushDirection * (Density * SeparationStrength + ClosingBrakeAcceleration);
			const FVector VelocityDelta = SeparationAcceleration * DeltaTime;
			VelocityDeltas[IndexA] += VelocityDelta;
			VelocityDeltas[IndexB] -= VelocityDelta;
		}
	}

	for (int32 Index = 0; Index < RegisteredDensityComponents.Num(); ++Index)
	{
		if (UFlowFieldDensityComponent* DensityComponent = RegisteredDensityComponents[Index].Get())
		{
			DensityComponent->ApplySeparationVelocityDelta(VelocityDeltas[Index]);
			PreviousDensityComponentPositions.FindOrAdd(RegisteredDensityComponents[Index]) = ComponentPositions[Index];
		}
	}
}

bool UFlowFieldSubsystem::SetGoal(const FVector& GoalWorldLocation)
{
	if (bHasFlowField
		&& GoalWorldLocation.Equals(CurrentGoal)
		&& CachedNavMesh.IsValid()
		&& CachedNavMesh->HasFlowField())
	{
		return true;
	}

	bHasFlowField = false;

	AFlowFieldRecastNavMesh* NavMesh = FindNavMesh();
	if (NavMesh == nullptr || !NavMesh->BuildFlowField(GoalWorldLocation))
	{
		if (IsFlowFieldNetworkDiagnosticsEnabled())
		{
			const UWorld* World = GetWorld();
			const double Now = World != nullptr ? World->GetTimeSeconds() : FPlatformTime::Seconds();
			if (LastSetGoalFailureLogTime < 0.0 || Now - LastSetGoalFailureLogTime >= 5.0)
			{
				LastSetGoalFailureLogTime = Now;
				UE_LOG(
					LogFlowField,
					Warning,
					TEXT("SetGoal failed. World=%s NetMode=%s Goal=%s NavMesh=%s NavHasFlowField=%s"),
					*GetNameSafe(World),
					NetModeToString(World != nullptr ? World->GetNetMode() : NM_MAX),
					*GoalWorldLocation.ToCompactString(),
					*GetNameSafe(NavMesh),
					NavMesh != nullptr && NavMesh->HasFlowField() ? TEXT("true") : TEXT("false"));
			}
		}
		return false;
	}

	CachedNavMesh = NavMesh;
	CurrentGoal = GoalWorldLocation;
	bHasFlowField = true;
	LastSetGoalFailureLogTime = -1.0;
	return true;
}

bool UFlowFieldSubsystem::QueryDirection(const FVector& WorldLocation, FVector& OutDirection) const
{
	OutDirection = FVector::ZeroVector;

	const AFlowFieldRecastNavMesh* NavMesh = FindNavMesh();
	return NavMesh != nullptr
		&& NavMesh->HasFlowField()
		&& NavMesh->QueryDirection(WorldLocation, OutDirection);
}

bool UFlowFieldSubsystem::QueryConstrainedMove(
	const FVector& WorldLocation,
	const float MaxTravelDistance,
	FVector& OutMoveOffset) const
{
	OutMoveOffset = FVector::ZeroVector;

	const AFlowFieldRecastNavMesh* NavMesh = FindNavMesh();
	return NavMesh != nullptr
		&& NavMesh->HasFlowField()
		&& NavMesh->QueryConstrainedMove(WorldLocation, MaxTravelDistance, OutMoveOffset);
}

bool UFlowFieldSubsystem::QueryNodeRef(const FVector& WorldLocation, NavNodeRef& OutNodeRef) const
{
	OutNodeRef = INVALID_NAVNODEREF;

	const AFlowFieldRecastNavMesh* NavMesh = FindNavMesh();
	return NavMesh != nullptr && NavMesh->QueryNodeRef(WorldLocation, OutNodeRef);
}

bool UFlowFieldSubsystem::QueryNavLink(const FVector& WorldLocation, FFlowFieldNavLinkInfo& OutNavLinksRef) const
{
	OutNavLinksRef = FFlowFieldNavLinkInfo();

	const AFlowFieldRecastNavMesh* NavMesh = FindNavMesh();
	return NavMesh != nullptr
		&& NavMesh->HasFlowField()
		&& NavMesh->QueryNavLink(WorldLocation, OutNavLinksRef);
}

bool UFlowFieldSubsystem::HasFlowField() const
{
	const AFlowFieldRecastNavMesh* NavMesh = FindNavMesh();
	return NavMesh != nullptr && NavMesh->HasFlowField();
}

AFlowFieldRecastNavMesh* UFlowFieldSubsystem::FindNavMesh() const
{
	if (AFlowFieldRecastNavMesh* NavMesh = CachedNavMesh.Get())
	{
		return NavMesh;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	for (TActorIterator<AFlowFieldRecastNavMesh> It(World); It; ++It)
	{
		return *It;
	}

	return nullptr;
}
