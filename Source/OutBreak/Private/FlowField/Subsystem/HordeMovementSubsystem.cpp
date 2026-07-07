// Fill out your copyright notice in the Description page of Project Settings.


#include "FlowField/Subsystem/HordeMovementSubsystem.h"

#include "Engine/World.h"
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

void UHordeMovementSubsystem::Register(const FTransform& Transform, float MoveSpeed)
{
	MovementStorage.Add(Transform, MoveSpeed);
}

void UHordeMovementSubsystem::Unregister(int32 Index)
{
	MovementStorage.RemoveAtSwap(Index);
}

void UHordeMovementSubsystem::ProcessSystem(const float DeltaSeconds)
{
	Super::ProcessSystem(DeltaSeconds);
	
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const ENetMode NetMode =
		World->GetNetMode();

	if (IsFlowFieldNetworkDiagnosticsEnabled())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"[HordeMovement] World=%s WorldType=%d NetMode=%d "
				"AgentCount=%d FlowFieldValid=%d Function=%s"),
			*World->GetName(),
			static_cast<int32>(World->WorldType),
			static_cast<int32>(NetMode),
			MovementStorage.Size(),
			FlowFieldSubsystem != nullptr,
			TEXT(__FUNCTION__));
	}
	
	if (NetMode == NM_Client)
	{
		SimulateClient(DeltaSeconds);
		return;
	}

	SimulateAuthority(DeltaSeconds);
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

	const int32 AgentCount =
		MovementStorage.Size();

	if (AgentCount <= 0)
	{
		return;
	}

	const bool bDiagnosticsEnabled =
		IsFlowFieldNetworkDiagnosticsEnabled();

	FVector DebugBeforeLocation =
		FVector::ZeroVector;

	FVector DebugDirection =
		FVector::ZeroVector;

	float DebugMoveSpeed =
		0.0f;

	bool bDebugQuerySucceeded =
		false;

	if (bDiagnosticsEnabled)
	{
		DebugBeforeLocation =
			MovementStorage.Transforms[0].GetLocation();

		DebugMoveSpeed =
			MovementStorage.MoveSpeeds[0];
	}
	
	
	for (int32 i = 0 ; i < AgentCount; i++)
	{
		const bool bQuerySucceeded =
			FlowFieldSubsystem->QueryDirection(
			MovementStorage.Transforms[i].GetLocation(),
			MovementStorage.CachedFlowDirections[i]);

		if (bDiagnosticsEnabled && i == 0)
		{
			bDebugQuerySucceeded =
				bQuerySucceeded;

			DebugDirection =
				MovementStorage.CachedFlowDirections[i];
		}
	}
	
	
	const UFlowFieldSettings* FlowFieldSettings = GetDefault<UFlowFieldSettings>();
	
	
	const float MaxSpeed = FlowFieldSettings->GetMaxVelocity();
	
	FTransform* Transforms = MovementStorage.Transforms.GetData();
	FVector* Velocities = MovementStorage.Velocities.GetData();
	const FVector* CachedFlowDirections = MovementStorage.CachedFlowDirections.GetData();
	const float* MoveSpeeds = MovementStorage.MoveSpeeds.GetData();
	
	ParallelFor(
		TEXT("UHordeMovementSubsystem::Parallel"),
			AgentCount,
			64,
			[
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
						FRotator(FacingDirection.Rotation()+FRotator(0,-90,0)).Quaternion());
				}
				Velocities[AgentIndex] = NewVelocity;
			});

	if (bDiagnosticsEnabled)
	{
		UWorld* World =
			GetWorld();

		if (World)
		{
			const FVector DebugAfterLocation =
				MovementStorage.Transforms[0].GetLocation();

			const FVector DebugVelocity =
				MovementStorage.Velocities[0];

			UE_LOG(
				LogTemp,
				Warning,
				TEXT(
					"[HordeMovement] World=%s WorldType=%d NetMode=%d "
					"AgentCount=%d Function=%s QueryDirection=%d "
					"Before=%s Direction=%s MoveSpeed=%.3f After=%s Velocity=%s"),
				*World->GetName(),
				static_cast<int32>(World->WorldType),
				static_cast<int32>(World->GetNetMode()),
				AgentCount,
				TEXT(__FUNCTION__),
				bDebugQuerySucceeded,
				*DebugBeforeLocation.ToCompactString(),
				*DebugDirection.ToCompactString(),
				DebugMoveSpeed,
				*DebugAfterLocation.ToCompactString(),
				*DebugVelocity.ToCompactString());
		}
	}
}

void UHordeMovementSubsystem::SimulateClient(const float DeltaSeconds)
{
	check(IsInGameThread());
	check(MovementStorage.IsValid());

	const UFlowFieldSettings* FlowFieldSettings =
		GetDefault<UFlowFieldSettings>();

	check(FlowFieldSettings);

	const float MaxSpeed =
		FlowFieldSettings->GetMaxVelocity();

	const int32 AgentCount =
		MovementStorage.Size();

	if (AgentCount <= 0)
	{
		return;
	}

	const bool bDiagnosticsEnabled =
		IsFlowFieldNetworkDiagnosticsEnabled();

	FVector DebugBeforeLocation =
		FVector::ZeroVector;

	FVector DebugDirection =
		FVector::ZeroVector;

	float DebugMoveSpeed =
		0.0f;

	if (bDiagnosticsEnabled)
	{
		DebugBeforeLocation =
			MovementStorage.Transforms[0].GetLocation();

		DebugDirection =
			MovementStorage.CachedFlowDirections[0];

		DebugMoveSpeed =
			MovementStorage.MoveSpeeds[0];
	}

	FTransform* Transforms =
		MovementStorage.Transforms.GetData();

	FVector* Velocities =
		MovementStorage.Velocities.GetData();

	const FVector* CachedFlowDirections =
		MovementStorage.CachedFlowDirections.GetData();

	const float* MoveSpeeds =
		MovementStorage.MoveSpeeds.GetData();

	ParallelFor(
		TEXT("UHordeMovementSubsystem::SimulateClient"),
		AgentCount,
		64,
		[
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
				CurrentPosition + NewVelocity;

			Transforms[AgentIndex].SetLocation(
				NewPosition);

			if (!NewVelocity.IsNearlyZero())
			{
				const FVector FacingDirection =
					NewVelocity.GetSafeNormal2D();

				Transforms[AgentIndex].SetRotation(
					FRotator(
						FacingDirection.Rotation()
						+ FRotator(0.0f, -90.0f, 0.0f)
					).Quaternion());
			}

			Velocities[AgentIndex] =
				NewVelocity;
		});

	if (bDiagnosticsEnabled)
	{
		UWorld* World =
			GetWorld();

		if (World)
		{
			const FVector DebugAfterLocation =
				MovementStorage.Transforms[0].GetLocation();

			const FVector DebugVelocity =
				MovementStorage.Velocities[0];

			UE_LOG(
				LogTemp,
				Warning,
				TEXT(
					"[HordeMovement] World=%s WorldType=%d NetMode=%d "
					"AgentCount=%d Function=%s Before=%s Direction=%s "
					"MoveSpeed=%.3f After=%s Velocity=%s"),
				*World->GetName(),
				static_cast<int32>(World->WorldType),
				static_cast<int32>(World->GetNetMode()),
				AgentCount,
				TEXT(__FUNCTION__),
				*DebugBeforeLocation.ToCompactString(),
				*DebugDirection.ToCompactString(),
				DebugMoveSpeed,
				*DebugAfterLocation.ToCompactString(),
				*DebugVelocity.ToCompactString());
		}
	}
}

void UHordeMovementSubsystem::SimulateAuthority(const float DeltaSeconds)
{
	check(IsInGameThread());
	check(MovementStorage.IsValid());

	if (IsFlowFieldNetworkDiagnosticsEnabled())
	{
		UWorld* World =
			GetWorld();

		if (World)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT(
					"[HordeMovement] World=%s WorldType=%d NetMode=%d "
					"AgentCount=%d FlowFieldValid=%d Function=%s"),
				*World->GetName(),
				static_cast<int32>(World->WorldType),
				static_cast<int32>(World->GetNetMode()),
				MovementStorage.Size(),
				FlowFieldSubsystem != nullptr,
				TEXT(__FUNCTION__));
		}
	}

	Parallel(DeltaSeconds);
}
