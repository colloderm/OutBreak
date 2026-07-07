// Fill out your copyright notice in the Description page of Project Settings.


#include "FlowField/Subsystem/HordeMovementSubsystem.h"

#include "Algo/Sort.h"
#include "Async/ParallelFor.h"
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#include "DrawDebugHelpers.h"
#endif
#include "Engine/World.h"
#include "FlowField/Subsystem/FlowFieldSubsystem.h"
#include "FlowField/Settings/FlowFieldSettings.h"
#include "HAL/IConsoleManager.h"

namespace
{
	constexpr float FlowDirectionSmoothingAlpha = 0.35f;
	constexpr float FailedQueryFallbackScale = 0.25f;
	constexpr uint8 MaxConsecutiveFallbackFrames = 6;

	constexpr float SeparationDebugHeight = 40.0f;
	constexpr float SeparationDebugArrowLength = 120.0f;
	constexpr float SeparationDebugLifeTime = 0.0f;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	TAutoConsoleVariable<int32> CVarHordeSeparationDebugDraw(
		TEXT("OutBreak.HordeMovement.SeparationDebug"),
		0,
		TEXT("Draws Horde separation steering debug for one agent."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarHordeSeparationDebugIndex(
		TEXT("OutBreak.HordeMovement.SeparationDebugIndex"),
		0,
		TEXT("Packed agent index used by OutBreak.HordeMovement.SeparationDebug."),
		ECVF_Default);
#endif

	bool IsCellLess(
		const int32 CellX,
		const int32 CellY,
		const int32 OtherCellX,
		const int32 OtherCellY)
	{
		return CellX < OtherCellX
			|| (CellX == OtherCellX && CellY < OtherCellY);
	}

	FVector GetDeterministicOverlapSeparationDirection(
		const int32 AgentIndex,
		const int32 OtherAgentIndex)
	{
		const int32 MinIndex =
			FMath::Min(AgentIndex, OtherAgentIndex);

		const int32 MaxIndex =
			FMath::Max(AgentIndex, OtherAgentIndex);

		const uint32 PairHash =
			HashCombine(
				GetTypeHash(MinIndex),
				GetTypeHash(MaxIndex));

		FVector Axis = FVector::ForwardVector;

		switch (PairHash & 3u)
		{
		case 0:
			Axis = FVector::ForwardVector;
			break;
		case 1:
			Axis = FVector::RightVector;
			break;
		case 2:
			Axis = -FVector::ForwardVector;
			break;
		default:
			Axis = -FVector::RightVector;
			break;
		}

		return AgentIndex < OtherAgentIndex
			? Axis
			: -Axis;
	}

	int32 FindSeparationCellRangeIndex(
		const TArray<FHordeSeparationCellRange>& CellRanges,
		const int32 CellX,
		const int32 CellY)
	{
		int32 LowerIndex = 0;
		int32 UpperIndex = CellRanges.Num() - 1;

		while (LowerIndex <= UpperIndex)
		{
			const int32 MiddleIndex =
				LowerIndex + (UpperIndex - LowerIndex) / 2;

			const FHordeSeparationCellRange& Range =
				CellRanges[MiddleIndex];

			if (Range.CellX == CellX && Range.CellY == CellY)
			{
				return MiddleIndex;
			}

			if (IsCellLess(Range.CellX, Range.CellY, CellX, CellY))
			{
				LowerIndex = MiddleIndex + 1;
			}
			else
			{
				UpperIndex = MiddleIndex - 1;
			}
		}

		return INDEX_NONE;
	}

	void BuildSeparationSpatialGrid(
		const TArray<FVector>& PositionSnapshot,
		const float CellSize,
		TArray<FHordeSeparationGridEntry>& OutGridEntries,
		TArray<FHordeSeparationCellRange>& OutCellRanges)
	{
		const int32 AgentCount =
			PositionSnapshot.Num();

		OutGridEntries.SetNumUninitialized(AgentCount);
		OutCellRanges.Reset(AgentCount);

		if (AgentCount <= 0 || CellSize <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		for (int32 AgentIndex = 0;
			 AgentIndex < AgentCount;
			 ++AgentIndex)
		{
			const FVector& Position =
				PositionSnapshot[AgentIndex];

			FHordeSeparationGridEntry& Entry =
				OutGridEntries[AgentIndex];

			Entry.CellX =
				FMath::FloorToInt(Position.X / CellSize);

			Entry.CellY =
				FMath::FloorToInt(Position.Y / CellSize);

			Entry.AgentIndex =
				AgentIndex;
		}

		Algo::Sort(
			OutGridEntries,
			[](const FHordeSeparationGridEntry& Left,
			   const FHordeSeparationGridEntry& Right)
			{
				if (Left.CellX != Right.CellX)
				{
					return Left.CellX < Right.CellX;
				}

				if (Left.CellY != Right.CellY)
				{
					return Left.CellY < Right.CellY;
				}

				return Left.AgentIndex < Right.AgentIndex;
			});

		int32 CellStartIndex = 0;

		for (int32 EntryIndex = 1;
			 EntryIndex <= OutGridEntries.Num();
			 ++EntryIndex)
		{
			const bool bReachedEnd =
				EntryIndex == OutGridEntries.Num();

			const bool bStartedNewCell =
				!bReachedEnd
				&& (OutGridEntries[EntryIndex].CellX
						!= OutGridEntries[CellStartIndex].CellX
					|| OutGridEntries[EntryIndex].CellY
						!= OutGridEntries[CellStartIndex].CellY);

			if (!bReachedEnd && !bStartedNewCell)
			{
				continue;
			}

			FHordeSeparationCellRange& CellRange =
				OutCellRanges.AddDefaulted_GetRef();

			CellRange.CellX =
				OutGridEntries[CellStartIndex].CellX;

			CellRange.CellY =
				OutGridEntries[CellStartIndex].CellY;

			CellRange.StartIndex =
				CellStartIndex;

			CellRange.Count =
				EntryIndex - CellStartIndex;

			CellStartIndex =
				EntryIndex;
		}
	}

	void CalculateSeparationDirections(
		const TArray<FVector>& PositionSnapshot,
		const TArray<FHordeSeparationGridEntry>& GridEntries,
		const TArray<FHordeSeparationCellRange>& CellRanges,
		const float SeparationRadius,
		TArray<FVector>& OutSeparationDirections)
	{
		const int32 AgentCount =
			PositionSnapshot.Num();

		OutSeparationDirections.SetNumUninitialized(AgentCount);

		if (AgentCount <= 0
			|| SeparationRadius <= KINDA_SMALL_NUMBER
			|| GridEntries.Num() != AgentCount)
		{
			for (FVector& SeparationDirection : OutSeparationDirections)
			{
				SeparationDirection =
					FVector::ZeroVector;
			}

			return;
		}

		const float SeparationRadiusSquared =
			FMath::Square(SeparationRadius);

		const FVector* PositionData =
			PositionSnapshot.GetData();

		FVector* SeparationDirectionsData =
			OutSeparationDirections.GetData();

		ParallelFor(
			TEXT("UHordeMovementSubsystem::CalculateSeparationDirections"),
			AgentCount,
			64,
			[
				PositionData,
				SeparationDirectionsData,
				&GridEntries,
				&CellRanges,
				SeparationRadius,
				SeparationRadiusSquared
			](const int32 AgentIndex)
			{
				const FVector AgentPosition =
					PositionData[AgentIndex];

				const int32 CellX =
					FMath::FloorToInt(AgentPosition.X / SeparationRadius);

				const int32 CellY =
					FMath::FloorToInt(AgentPosition.Y / SeparationRadius);

				FVector SeparationSum =
					FVector::ZeroVector;

				for (int32 OffsetY = -1; OffsetY <= 1; ++OffsetY)
				{
					for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
					{
						const int32 CellRangeIndex =
							FindSeparationCellRangeIndex(
								CellRanges,
								CellX + OffsetX,
								CellY + OffsetY);

						if (CellRangeIndex == INDEX_NONE)
						{
							continue;
						}

						const FHordeSeparationCellRange& CellRange =
							CellRanges[CellRangeIndex];

						for (int32 EntryOffset = 0;
							 EntryOffset < CellRange.Count;
							 ++EntryOffset)
						{
							const FHordeSeparationGridEntry& Entry =
								GridEntries[
									CellRange.StartIndex + EntryOffset];

							const int32 OtherAgentIndex =
								Entry.AgentIndex;

							if (OtherAgentIndex == AgentIndex)
							{
								continue;
							}

							FVector Delta =
								AgentPosition
								- PositionData[OtherAgentIndex];

							Delta.Z = 0.0f;

							const float DistanceSquared =
								Delta.SizeSquared();

							if (DistanceSquared > SeparationRadiusSquared)
							{
								continue;
							}

							float Distance = 0.0f;
							FVector SeparationDirection =
								FVector::ZeroVector;

							if (DistanceSquared <= KINDA_SMALL_NUMBER)
							{
								SeparationDirection =
									GetDeterministicOverlapSeparationDirection(
										AgentIndex,
										OtherAgentIndex);
							}
							else
							{
								Distance =
									FMath::Sqrt(DistanceSquared);

								SeparationDirection =
									Delta / Distance;
							}

							const float NormalizedDistance =
								FMath::Clamp(
									Distance / SeparationRadius,
									0.0f,
									1.0f);

							const float Strength =
								FMath::Square(
									1.0f - NormalizedDistance);

							SeparationSum +=
								SeparationDirection * Strength;
						}
					}
				}

				SeparationDirectionsData[AgentIndex] =
					SeparationSum.GetClampedToMaxSize(1.0f);
			});
	}

	void UpdateSeparationDirections(
		HordeMovementStorage& MovementStorage,
		const float SeparationRadius)
	{
		const int32 AgentCount =
			MovementStorage.Size();

		MovementStorage.PositionSnapshot.SetNumUninitialized(
			AgentCount);

		for (int32 AgentIndex = 0;
			 AgentIndex < AgentCount;
			 ++AgentIndex)
		{
			MovementStorage.PositionSnapshot[AgentIndex] =
				MovementStorage.Transforms[AgentIndex].GetLocation();
		}

		BuildSeparationSpatialGrid(
			MovementStorage.PositionSnapshot,
			SeparationRadius,
			MovementStorage.SeparationGridEntries,
			MovementStorage.SeparationCellRanges);

		CalculateSeparationDirections(
			MovementStorage.PositionSnapshot,
			MovementStorage.SeparationGridEntries,
			MovementStorage.SeparationCellRanges,
			SeparationRadius,
			MovementStorage.SeparationDirections);
	}

	FVector ApplySeparationToMoveOffset(
		const FVector& BaseMoveOffset,
		const FVector& SeparationDirection,
		const float SeparationSteeringWeight)
	{
		const float BaseMoveDistance =
			BaseMoveOffset.Size2D();

		FVector FinalMoveOffset =
			BaseMoveOffset;

		if (BaseMoveDistance > KINDA_SMALL_NUMBER
			&& SeparationSteeringWeight > KINDA_SMALL_NUMBER)
		{
			FinalMoveOffset +=
				SeparationDirection
				* BaseMoveDistance
				* SeparationSteeringWeight;

			FinalMoveOffset.Z = 0.0f;

			FinalMoveOffset =
				FinalMoveOffset.GetClampedToMaxSize(
					BaseMoveDistance);
		}

		return FinalMoveOffset;
	}

	FVector ApplySeparationToDirection(
		const FVector& CurrentDirection,
		const FVector& SeparationDirection,
		const float SeparationSteeringWeight)
	{
		FVector SteeringDirection =
			CurrentDirection;

		if (!CurrentDirection.IsNearlyZero()
			&& SeparationSteeringWeight > KINDA_SMALL_NUMBER)
		{
			SteeringDirection =
				(
					CurrentDirection
					+ SeparationDirection
					* SeparationSteeringWeight
				).GetSafeNormal2D();

			if (SteeringDirection.IsNearlyZero())
			{
				SteeringDirection =
					CurrentDirection;
			}
		}

		return SteeringDirection;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	int32 GetSeparationDebugIndex(const int32 AgentCount)
	{
		if (CVarHordeSeparationDebugDraw.GetValueOnGameThread() == 0
			|| AgentCount <= 0)
		{
			return INDEX_NONE;
		}

		return FMath::Clamp(
			CVarHordeSeparationDebugIndex.GetValueOnGameThread(),
			0,
			AgentCount - 1);
	}

	void DrawHordeSeparationDebug(
		UWorld* World,
		const TArray<FVector>& PositionSnapshot,
		const TArray<FVector>& FlowVectors,
		const TArray<FVector>& SeparationDirections,
		const TArray<FVector>& FinalMoveVectors,
		const float SeparationRadius)
	{
		if (!World || World->GetNetMode() == NM_DedicatedServer)
		{
			return;
		}

		const int32 AgentCount =
			PositionSnapshot.Num();

		const int32 DebugIndex =
			GetSeparationDebugIndex(AgentCount);

		if (DebugIndex == INDEX_NONE
			|| !FlowVectors.IsValidIndex(DebugIndex)
			|| !SeparationDirections.IsValidIndex(DebugIndex)
			|| !FinalMoveVectors.IsValidIndex(DebugIndex))
		{
			return;
		}

		const FVector Origin =
			PositionSnapshot[DebugIndex]
			+ FVector(0.0f, 0.0f, SeparationDebugHeight);

		DrawDebugSphere(
			World,
			Origin,
			8.0f,
			8,
			FColor::White,
			false,
			SeparationDebugLifeTime,
			0,
			1.5f);

		if (SeparationRadius > KINDA_SMALL_NUMBER)
		{
			DrawDebugCylinder(
				World,
				Origin - FVector(0.0f, 0.0f, 1.0f),
				Origin + FVector(0.0f, 0.0f, 1.0f),
				SeparationRadius,
				32,
				FColor::Cyan,
				false,
				SeparationDebugLifeTime,
				0,
				0.5f);
		}

		const auto DrawDirection =
			[
				World,
				Origin
			](const FVector& Direction,
			  const FColor& Color)
			{
				if (Direction.IsNearlyZero())
				{
					return;
				}

				const FVector End =
					Origin
					+ Direction.GetSafeNormal2D()
					* SeparationDebugArrowLength;

				DrawDebugDirectionalArrow(
					World,
					Origin,
					End,
					18.0f,
					Color,
					false,
					SeparationDebugLifeTime,
					0,
					2.0f);
			};

		DrawDirection(FlowVectors[DebugIndex], FColor::Green);
		DrawDirection(SeparationDirections[DebugIndex], FColor(255, 128, 0));
		DrawDirection(FinalMoveVectors[DebugIndex], FColor::Blue);
	}
#endif
}


void UHordeMovementSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	FlowFieldSubsystem = Collection.InitializeDependency<UFlowFieldSubsystem>();
}

void UHordeMovementSubsystem::InitializeStorage(int32 Capacity)
{
	MovementStorage.Initialize(Capacity);
}

int32 UHordeMovementSubsystem::Register(const FTransform& Transform, float MoveSpeed)
{
	check(IsInGameThread());

	const int32 PackedIndex =
		MovementStorage.Add(Transform, MoveSpeed);

	check(MovementStorage.IsValid());

	return PackedIndex;
}

void UHordeMovementSubsystem::Unregister(int32 Index)
{
	check(IsInGameThread());
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

	FVector DebugSeparationDirection =
		FVector::ZeroVector;

	FVector DebugFinalMoveOffset =
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
	
	
	const UFlowFieldSettings* FlowFieldSettings =
		GetDefault<UFlowFieldSettings>();

	check(FlowFieldSettings);

	const float MaxSpeed =
		FlowFieldSettings->GetMaxVelocity();

	const float SeparationRadius =
		FMath::Max(0.0f, FlowFieldSettings->GetSeparationRadius());

	const float SeparationSteeringWeight =
		FMath::Clamp(
			FlowFieldSettings->GetSeparationSteeringWeight(),
			0.0f,
			1.0f);

	UpdateSeparationDirections(
		MovementStorage,
		SeparationRadius);

	TArray<FVector>& MoveOffsets =
		MovementStorage.MoveOffsetScratch;

	MoveOffsets.SetNumUninitialized(AgentCount);

	for (int32 i = 0 ; i < AgentCount; i++)
	{
		const float MaxTravelDistance =
			FMath::Min(
				MovementStorage.MoveSpeeds[i] * DeltaSeconds,
				MaxSpeed);

		FVector MoveOffset =
			FVector::ZeroVector;

		const bool bQuerySucceeded =
			FlowFieldSubsystem->QueryConstrainedMove(
				MovementStorage.Transforms[i].GetLocation(),
				MaxTravelDistance,
				MoveOffset);

		MoveOffset.Z = 0.0f;

		const bool bHasValidMoveOffset =
			bQuerySucceeded
			&& !MoveOffset.IsNearlyZero();

		const FVector PreviousDirection =
			MovementStorage.CachedFlowDirections[i]
			.GetSafeNormal2D();

		if (bHasValidMoveOffset)
		{
			const FVector QueryDirection =
				MoveOffset.GetSafeNormal2D();

			const FVector SmoothedDirection =
				PreviousDirection.IsNearlyZero()
					? QueryDirection
					: FMath::Lerp(
						PreviousDirection,
						QueryDirection,
						FlowDirectionSmoothingAlpha)
						.GetSafeNormal2D();

			MovementStorage.CachedFlowDirections[i] =
				SmoothedDirection.IsNearlyZero()
					? QueryDirection
					: SmoothedDirection;

			MovementStorage.FlowQueryFailureCounts[i] =
				0;

			MoveOffsets[i] =
				MoveOffset;
		}
		else if (!PreviousDirection.IsNearlyZero()
			&& MovementStorage.FlowQueryFailureCounts[i]
				< MaxConsecutiveFallbackFrames)
		{
			++MovementStorage.FlowQueryFailureCounts[i];

			MoveOffsets[i] =
				PreviousDirection
				* MaxTravelDistance
				* FailedQueryFallbackScale;
		}
		else
		{
			const uint8 FailureCount =
				MovementStorage.FlowQueryFailureCounts[i];

			MovementStorage.FlowQueryFailureCounts[i] =
				FailureCount < MaxConsecutiveFallbackFrames
					? FailureCount + 1
					: MaxConsecutiveFallbackFrames;

			MoveOffsets[i] =
				FVector::ZeroVector;
		}

		if (bDiagnosticsEnabled && i == 0)
		{
			bDebugQuerySucceeded =
				bQuerySucceeded;

			DebugDirection =
				MovementStorage.CachedFlowDirections[i];
		}
	}

	TArray<FVector>& FinalMoveOffsets =
		MovementStorage.FinalMoveOffsetScratch;

	FinalMoveOffsets.SetNumUninitialized(AgentCount);

	for (int32 AgentIndex = 0;
		 AgentIndex < AgentCount;
		 ++AgentIndex)
	{
		FinalMoveOffsets[AgentIndex] =
			ApplySeparationToMoveOffset(
				MoveOffsets[AgentIndex],
				MovementStorage.SeparationDirections[AgentIndex],
				SeparationSteeringWeight);

		if (bDiagnosticsEnabled && AgentIndex == 0)
		{
			DebugSeparationDirection =
				MovementStorage.SeparationDirections[AgentIndex];

			DebugFinalMoveOffset =
				FinalMoveOffsets[AgentIndex];
		}
	}
	
	FTransform* Transforms = MovementStorage.Transforms.GetData();
	FVector* Velocities = MovementStorage.Velocities.GetData();
	const FVector* FinalMoveOffsetsData = FinalMoveOffsets.GetData();
	
	ParallelFor(
		TEXT("UHordeMovementSubsystem::Parallel"),
			AgentCount,
			64,
			[
				Transforms,
				Velocities,
				FinalMoveOffsetsData
				](const int32 AgentIndex)
			{
				const FVector CurrentPosition = 
					Transforms[AgentIndex].GetLocation();
				
				const FVector MoveOffset =
					FinalMoveOffsetsData[AgentIndex];
				
				const FVector NewPosition = 
					CurrentPosition + MoveOffset;
				
				Transforms[AgentIndex].SetLocation(NewPosition);
				if (!MoveOffset.IsNearlyZero())
				{
					const FVector FacingDirection =
						MoveOffset.GetSafeNormal2D();

					Transforms[AgentIndex].SetRotation(
						FRotator(FacingDirection.Rotation()+FRotator(0,-90,0)).Quaternion());
				}
				Velocities[AgentIndex] = MoveOffset;
			});

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	DrawHordeSeparationDebug(
		GetWorld(),
		MovementStorage.PositionSnapshot,
		MoveOffsets,
		MovementStorage.SeparationDirections,
		FinalMoveOffsets,
		SeparationRadius);
#endif

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
					"AgentCount=%d Function=%s QueryConstrainedMove=%d "
					"Before=%s Direction=%s Separation=%s FinalMove=%s "
					"MoveSpeed=%.3f After=%s Velocity=%s"),
				*World->GetName(),
				static_cast<int32>(World->WorldType),
				static_cast<int32>(World->GetNetMode()),
				AgentCount,
				TEXT(__FUNCTION__),
				bDebugQuerySucceeded,
				*DebugBeforeLocation.ToCompactString(),
				*DebugDirection.ToCompactString(),
				*DebugSeparationDirection.ToCompactString(),
				*DebugFinalMoveOffset.ToCompactString(),
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

	const float SeparationRadius =
		FMath::Max(0.0f, FlowFieldSettings->GetSeparationRadius());

	const float SeparationSteeringWeight =
		FMath::Clamp(
			FlowFieldSettings->GetSeparationSteeringWeight(),
			0.0f,
			1.0f);

	UpdateSeparationDirections(
		MovementStorage,
		SeparationRadius);

	const bool bDiagnosticsEnabled =
		IsFlowFieldNetworkDiagnosticsEnabled();

	FVector DebugBeforeLocation =
		FVector::ZeroVector;

	FVector DebugDirection =
		FVector::ZeroVector;

	FVector DebugSeparationDirection =
		FVector::ZeroVector;

	FVector DebugSteeringDirection =
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

		DebugSeparationDirection =
			MovementStorage.SeparationDirections[0];

		DebugSteeringDirection =
			ApplySeparationToDirection(
				DebugDirection.GetSafeNormal2D(),
				DebugSeparationDirection,
				SeparationSteeringWeight);
	}

	FTransform* Transforms =
		MovementStorage.Transforms.GetData();

	FVector* Velocities =
		MovementStorage.Velocities.GetData();

	const FVector* CachedFlowDirections =
		MovementStorage.CachedFlowDirections.GetData();

	const FVector* SeparationDirections =
		MovementStorage.SeparationDirections.GetData();

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
			SeparationDirections,
			MoveSpeeds,
			MaxSpeed,
			SeparationSteeringWeight,
			DeltaSeconds
		](const int32 AgentIndex)
		{
			const FVector CurrentPosition =
				Transforms[AgentIndex].GetLocation();

			const FVector CurrentDirection =
				CachedFlowDirections[AgentIndex].GetSafeNormal2D();

			const FVector SteeringDirection =
				ApplySeparationToDirection(
					CurrentDirection,
					SeparationDirections[AgentIndex],
					SeparationSteeringWeight);

			const float CurrentAcceleration =
				MoveSpeeds[AgentIndex] * DeltaSeconds;

			const FVector NewVelocity =
				(SteeringDirection * CurrentAcceleration)
				.GetClampedToMaxSize(MaxSpeed);

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

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	DrawHordeSeparationDebug(
		GetWorld(),
		MovementStorage.PositionSnapshot,
		MovementStorage.CachedFlowDirections,
		MovementStorage.SeparationDirections,
		MovementStorage.Velocities,
		SeparationRadius);
#endif

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
					"Separation=%s Steering=%s MoveSpeed=%.3f "
					"After=%s Velocity=%s"),
				*World->GetName(),
				static_cast<int32>(World->WorldType),
				static_cast<int32>(World->GetNetMode()),
				AgentCount,
				TEXT(__FUNCTION__),
				*DebugBeforeLocation.ToCompactString(),
				*DebugDirection.ToCompactString(),
				*DebugSeparationDirection.ToCompactString(),
				*DebugSteeringDirection.ToCompactString(),
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
