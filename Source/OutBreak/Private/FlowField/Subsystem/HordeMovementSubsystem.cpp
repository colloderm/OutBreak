// Fill out your copyright notice in the Description page of Project Settings.


#include "FlowField/Subsystem/HordeMovementSubsystem.h"

#include "FlowField/Subsystem/FlowFieldSubsystem.h"
#include "FlowField/Settings/FlowFieldSettings.h"


void UHordeMovementSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	FlowFieldSubsystem = Collection.InitializeDependency<UFlowFieldSubsystem>();
	
}

void UHordeMovementSubsystem::InitializeStorage(int32 Capacity)
{
	MovementStorage.Initialize(Capacity);
}

void UHordeMovementSubsystem::Register(FTransform& Transform, float MoveSpeed)
{
	MovementStorage.Add(Transform, MoveSpeed);
}

void UHordeMovementSubsystem::ProcessSystem(const float DeltaSeconds)
{
	Super::ProcessSystem(DeltaSeconds);
	
	Parallel(DeltaSeconds);
}

void UHordeMovementSubsystem::Parallel(const float DeltaSeconds)
{
	check(IsInGameThread());
	check(MovementStorage.IsValid());
	
	if (!FlowFieldSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s: FlowFieldSubsystem is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	
	
	for (int32 i = 0 ; i < MovementStorage.Size(); i++)
	{
		FlowFieldSubsystem->QueryDirection(
			MovementStorage.Transforms[i].GetLocation(),
			MovementStorage.CachedFlowDirections[i]);
	}
	
	
	const UFlowFieldSettings* FlowFieldSettings = GetDefault<UFlowFieldSettings>();
	
	
	const float MaxSpeed = FlowFieldSettings->GetMaxVelocity();
	const int32 AgentCount = MovementStorage.Size();
	
	FTransform* Transforms = MovementStorage.Transforms.GetData();
	FVector* Velocities = MovementStorage.Velocities.GetData();
	const FVector* CachedFlowDirections = MovementStorage.CachedFlowDirections.GetData();
	const float* MoveSpeeds = MovementStorage.MoveSpeeds.GetData();
	
	ParallelFor(
		TEXT("UHordeMovementSubsystem::Parallel"),
			AgentCount,
			64,
			[this,
				Transforms,
				Velocities,
				CachedFlowDirections,
				MoveSpeeds,
				MaxSpeed,
				DeltaSeconds
				](const int32 AgentIndex)
			{
				const FVector CurrentPosition = 
					Transforms[AgentIndex].GetLocation();
				
				const FVector CurrentDirection = 
					CachedFlowDirections[AgentIndex].GetSafeNormal();
				
				const FVector CurrentVelocity =
					Velocities[AgentIndex];
				
				const float CurrentAcceleration = 
					MoveSpeeds[AgentIndex] * DeltaSeconds;
				
				const FVector NewVelocity =
				(
					// CurrentVelocity + 
					CurrentDirection * CurrentAcceleration
				).GetClampedToMaxSize(MaxSpeed);
				
				const FVector NewPosition = 
					CurrentPosition + (NewVelocity /* DeltaSeconds*/);
				
				Transforms[AgentIndex].SetLocation(NewPosition);
				if (!NewVelocity.IsNearlyZero())
				{
					const FVector FacingDirection =
						NewVelocity.GetSafeNormal2D();

					Transforms[AgentIndex].SetRotation(
						FacingDirection.Rotation().Quaternion());
				}
				Velocities[AgentIndex] = NewVelocity;
			});
}

