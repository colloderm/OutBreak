#include "InteriorPCGWallVolume.h"

#include "InteriorPCGItemComponent.h"

#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogInteriorPCGWalls, Log, All);

namespace InteriorPCGWallVolumePrivate
{
	struct FPlannedWallModule
	{
		const FInteriorPCGWallClassEntry* Entry = nullptr;
		FTransform Transform;
		FGuid StableId;
		EInteriorPCGItemRole Role = EInteriorPCGItemRole::InteriorWall;
		float FloorHeightOffset = 0.0f;
		int32 FloorIndex = INDEX_NONE;
	};

	bool DestroyActorForGeneration(AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return false;
		}

		Actor->Modify();
#if WITH_EDITOR
		if (UWorld* World = Actor->GetWorld())
		{
			if (World->WorldType == EWorldType::Editor || World->WorldType == EWorldType::EditorPreview)
			{
				return World->EditorDestroyActor(Actor, true);
			}
		}
#endif
		return Actor->Destroy();
	}
}

bool FInteriorPCGWallClassEntry::HasValidAssetReference() const
{
	return AssetKind == EInteriorPCGAssetKind::StaticMesh ? !StaticMesh.IsNull() : !ActorClass.IsNull();
}

bool FInteriorPCGWallClassEntry::HasValidClass() const
{
	return HasValidAssetReference();
}

AInteriorPCGWallVolume::AInteriorPCGWallVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void AInteriorPCGWallVolume::PostLoad()
{
	Super::PostLoad();
	EnsureWallEntryIds();
}

#if WITH_EDITOR
void AInteriorPCGWallVolume::PostActorCreated()
{
	Super::PostActorCreated();
	EnsureWallEntryIds();
}

void AInteriorPCGWallVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	EnsureWallEntryIds();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void AInteriorPCGWallVolume::EnsureWallEntryIds()
{
	TSet<FGuid> UsedIds;
	auto EnsureIds = [&UsedIds](TArray<FInteriorPCGWallClassEntry>& Entries)
	{
		for (FInteriorPCGWallClassEntry& Entry : Entries)
		{
			if (!Entry.EntryId.IsValid() || UsedIds.Contains(Entry.EntryId))
			{
				Entry.EntryId = FGuid::NewGuid();
			}
			UsedIds.Add(Entry.EntryId);
		}
	};

	EnsureIds(WallClasses);
	EnsureIds(DoorWallClasses);
	EnsureIds(StairClasses);
}

const FInteriorPCGWallClassEntry* AInteriorPCGWallVolume::SelectWallClass(const TArray<FInteriorPCGWallClassEntry>& Entries, FRandomStream& Stream) const
{
	float TotalWeight = 0.0f;
	const FInteriorPCGWallClassEntry* LastValid = nullptr;
	for (const FInteriorPCGWallClassEntry& Entry : Entries)
	{
		if (Entry.bEnabled && Entry.HasValidAssetReference() && Entry.SelectionWeight > 0.0f)
		{
			TotalWeight += Entry.SelectionWeight;
			LastValid = &Entry;
		}
	}

	if (!LastValid || TotalWeight <= 0.0f)
	{
		return nullptr;
	}

	const float Selection = Stream.FRandRange(0.0f, TotalWeight);
	float Accumulated = 0.0f;
	for (const FInteriorPCGWallClassEntry& Entry : Entries)
	{
		if (!Entry.bEnabled || !Entry.HasValidAssetReference() || Entry.SelectionWeight <= 0.0f)
		{
			continue;
		}

		Accumulated += Entry.SelectionWeight;
		if (Selection <= Accumulated)
		{
			return &Entry;
		}
	}

	return LastValid;
}

bool AInteriorPCGWallVolume::FindBoundarySpan(const FVector& ScanOrigin, const FVector& Direction, FVector& OutNegativeHit, FVector& OutPositiveHit) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FBox WorldBounds = GetComponentsBoundingBox(true);
	if (!WorldBounds.IsValid)
	{
		return false;
	}

	const double TraceDistance = WorldBounds.GetSize().Length() + FloorTracePadding + WallEndClearance;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(InteriorPCGBoundaryWallTrace), true, this);
	QueryParams.AddIgnoredActor(this);
	TArray<AActor*> ExistingGeneratedActors;
	GetRegisteredActors(ExistingGeneratedActors);
	QueryParams.AddIgnoredActors(ExistingGeneratedActors);

	FHitResult NegativeHit;
	FHitResult PositiveHit;
	const bool bNegativeHit = World->LineTraceSingleByChannel(NegativeHit, ScanOrigin, ScanOrigin - Direction * TraceDistance, WallTraceChannel, QueryParams);
	const bool bPositiveHit = World->LineTraceSingleByChannel(PositiveHit, ScanOrigin, ScanOrigin + Direction * TraceDistance, WallTraceChannel, QueryParams);
	if (!bNegativeHit || !bPositiveHit || !NegativeHit.bBlockingHit || !PositiveHit.bBlockingHit)
	{
		return false;
	}

	if (FMath::Abs(NegativeHit.ImpactNormal.Z) > MaximumBoundaryWallNormalZ || FMath::Abs(PositiveHit.ImpactNormal.Z) > MaximumBoundaryWallNormalZ)
	{
		return false;
	}

	OutNegativeHit = NegativeHit.ImpactPoint;
	OutPositiveHit = PositiveHit.ImpactPoint;
	return FVector::DotProduct(OutPositiveHit - OutNegativeHit, Direction) > WallModuleLength;
}

bool AInteriorPCGWallVolume::IsConnectivitySampleClear(const FVector2D& WorldXY, const float TargetFloorWorldZ, const TArray<FBox>& NavigationObstacleBounds, FBox& OutCorridorBound, FVector& OutPathPoint) const
{
	FHitResult FloorHit;
	if (!TraceFloorAtWorldXYForLayer(WorldXY, TargetFloorWorldZ, FloorHit))
	{
		return false;
	}

	const FVector SafeExtent(
		FMath::Max(10.0f, WalkwayHalfWidth),
		FMath::Max(10.0f, WalkwayHalfWidth),
		FMath::Max(10.0f, WalkwayHalfHeight));
	const FVector CollisionCenter(WorldXY.X, WorldXY.Y, FloorHit.ImpactPoint.Z + SafeExtent.Z + CollisionFloorClearance);
	if (!EncompassesPoint(CollisionCenter, SafeExtent.X))
	{
		return false;
	}

	OutCorridorBound = FBox(-SafeExtent, SafeExtent).TransformBy(FTransform(GetActorQuat(), CollisionCenter));
	for (const FBox& ObstacleBound : NavigationObstacleBounds)
	{
		if (OutCorridorBound.Intersect(ObstacleBound))
		{
			return false;
		}
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(InteriorPCGConnectivityCorridor), false, this);
	QueryParams.AddIgnoredActor(this);
	TArray<AActor*> GeneratedActors;
	GetRegisteredActors(GeneratedActors);
	QueryParams.AddIgnoredActors(GeneratedActors);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(
		Overlaps,
		CollisionCenter,
		GetActorQuat(),
		PlacementCollisionChannel,
		FCollisionShape::MakeBox(SafeExtent),
		QueryParams);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (!Overlap.bBlockingHit)
		{
			continue;
		}

		const AActor* OverlapActor = Overlap.GetActor();
		if (!OverlapActor || OverlapActor == this)
		{
			continue;
		}
		if (OverlapActor == FloorHit.GetActor())
		{
			const FBox FloorBounds = OverlapActor->GetComponentsBoundingBox(true);
			if (FloorBounds.IsValid && FloorBounds.Max.Z <= FloorHit.ImpactPoint.Z + CollisionFloorClearance + 0.5f)
			{
				continue;
			}
		}
		return false;
	}

	OutPathPoint = FVector(WorldXY.X, WorldXY.Y, FloorHit.ImpactPoint.Z);
	return true;
}

bool AInteriorPCGWallVolume::TryBuildConnectivityCorridor(const FVector& StartWorld, const FVector& EndWorld, const float TargetFloorWorldZ, const TArray<FBox>& NavigationObstacleBounds, const bool bTryLocalXFirst, TArray<FBox>& OutCorridorBounds, TArray<FVector>& OutPathPoints) const
{
	const FTransform VolumeTransform = GetActorTransform();
	const FVector StartLocal = VolumeTransform.InverseTransformPosition(StartWorld);
	const FVector EndLocal = VolumeTransform.InverseTransformPosition(EndWorld);
	const float SampleSpacing = FMath::Max(10.0f, WalkwaySampleSpacing);

	auto TestRoute = [&](const bool bLocalXFirst)
	{
		OutCorridorBounds.Reset();
		OutPathPoints.Reset();
		const FVector CornerLocal = bLocalXFirst
			? FVector(EndLocal.X, StartLocal.Y, StartLocal.Z)
			: FVector(StartLocal.X, EndLocal.Y, StartLocal.Z);
		const FVector LocalRoute[] = { StartLocal, CornerLocal, EndLocal };

		for (int32 SegmentIndex = 0; SegmentIndex < 2; ++SegmentIndex)
		{
			const FVector SegmentStart = LocalRoute[SegmentIndex];
			const FVector SegmentEnd = LocalRoute[SegmentIndex + 1];
			const double SegmentLength = FVector2D(SegmentEnd - SegmentStart).Size();
			const int32 StepCount = FMath::Max(1, FMath::CeilToInt(SegmentLength / SampleSpacing));
			for (int32 StepIndex = SegmentIndex == 0 ? 0 : 1; StepIndex <= StepCount; ++StepIndex)
			{
				const double Alpha = static_cast<double>(StepIndex) / StepCount;
				const FVector LocalSample = FMath::Lerp(SegmentStart, SegmentEnd, Alpha);
				const FVector WorldSample = VolumeTransform.TransformPosition(LocalSample);
				FBox CorridorBound;
				FVector PathPoint;
				if (!IsConnectivitySampleClear(FVector2D(WorldSample.X, WorldSample.Y), TargetFloorWorldZ, NavigationObstacleBounds, CorridorBound, PathPoint))
				{
					return false;
				}
				OutCorridorBounds.Add(CorridorBound);
				OutPathPoints.Add(PathPoint);
			}
		}
		return true;
	};

	return TestRoute(bTryLocalXFirst) || TestRoute(!bTryLocalXFirst);
}

int32 AInteriorPCGWallVolume::ClearGeneratedWalls()
{
	TArray<AActor*> Registered;
	GetRegisteredActors(Registered);
	int32 RemovedCount = 0;
	for (AActor* Actor : Registered)
	{
		const UInteriorPCGItemComponent* ItemComponent = FindItemComponent(Actor);
		if (!ItemComponent || (ItemComponent->Role != EInteriorPCGItemRole::InteriorWall && ItemComponent->Role != EInteriorPCGItemRole::DoorWall && ItemComponent->Role != EInteriorPCGItemRole::Stair))
		{
			continue;
		}

		RemovedCount += InteriorPCGWallVolumePrivate::DestroyActorForGeneration(Actor) ? 1 : 0;
	}

	LastConnectivityPathPoints.Reset();
	Modify();
	return RemovedCount;
}

int32 AInteriorPCGWallVolume::GenerateInteriorWalls()
{
	EnsureWallEntryIds();
	ClearGeneratedWalls();
	LastConnectivityPathPoints.Reset();

	FBox LocalBounds;
	if (!GetVolumeLocalBounds(LocalBounds))
	{
		return 0;
	}

	const double SafeModuleLength = FMath::Max(1.0f, WallModuleLength);
	const FTransform VolumeTransform = GetActorTransform();
	TArray<FBox> AcceptedBounds;
	TArray<AActor*> ExistingActors;
	GetRegisteredActors(ExistingActors);
	for (AActor* ExistingActor : ExistingActors)
	{
		if (const UInteriorPCGItemComponent* ItemComponent = FindItemComponent(ExistingActor))
		{
			AcceptedBounds.Add(BuildPlacementBounds(ExistingActor->GetActorLocation(), ExistingActor->GetActorQuat(), ItemComponent->CollisionHalfExtent));
		}
	}
	TArray<float> TargetFloorHeights;
	if (bGenerateOnAllDetectedFloors)
	{
		ScanFloorLayers();
		TargetFloorHeights = LastDetectedFloorWorldHeights;
		if (TargetFloorHeights.IsEmpty())
		{
			UE_LOG(LogInteriorPCGWalls, Warning, TEXT("%s detected no floor layers for wall generation."), *GetName());
			return 0;
		}
	}
	else
	{
		TargetFloorHeights.Add(0.0f);
	}

	TArray<FBox> NavigationObstacleBounds = AcceptedBounds;
	TArray<FBox> ReservedCorridorBounds;
	TMap<int32, TArray<FVector>> StairAccessPointsByFloor;
	int32 SpawnedCount = 0;
	const bool bConnectivityRequired = bRequireConnectedDoorAndStairPaths && bGenerateOnAllDetectedFloors && TargetFloorHeights.Num() > 1;
	if (bConnectivityRequired)
	{
		FRandomStream ValidationStream(0);
		if (PartitionWallCount < 1 || !SelectWallClass(DoorWallClasses, ValidationStream) || !SelectWallClass(StairClasses, ValidationStream))
		{
			UE_LOG(LogInteriorPCGWalls, Warning, TEXT("%s requires at least one partition, one door asset, and one stair asset for connected generation."), *GetName());
			return 0;
		}

		for (int32 TransitionIndex = 0; TransitionIndex < TargetFloorHeights.Num() - 1; ++TransitionIndex)
		{
			FRandomStream StairStream(MakeFloorSeed(WallSeed ^ 0x51A17A1, TransitionIndex));
			bool bStairPlaced = false;
			for (int32 Attempt = 0; Attempt < FMath::Max(1, MaxStairPlacementAttempts) && !bStairPlaced; ++Attempt)
			{
				const FInteriorPCGWallClassEntry* StairEntry = SelectWallClass(StairClasses, StairStream);
				if (!StairEntry)
				{
					break;
				}

				const double MinX = LocalBounds.Min.X + FMath::Min<double>(PartitionMargin, LocalBounds.GetSize().X * 0.4);
				const double MaxX = LocalBounds.Max.X - FMath::Min<double>(PartitionMargin, LocalBounds.GetSize().X * 0.4);
				const double MinY = LocalBounds.Min.Y + FMath::Min<double>(PartitionMargin, LocalBounds.GetSize().Y * 0.4);
				const double MaxY = LocalBounds.Max.Y - FMath::Min<double>(PartitionMargin, LocalBounds.GetSize().Y * 0.4);
				const FVector LocalSample(StairStream.FRandRange(MinX, MaxX), StairStream.FRandRange(MinY, MaxY), LocalBounds.GetCenter().Z);
				const FVector WorldSample = VolumeTransform.TransformPosition(LocalSample);

				FHitResult LowerFloorHit;
				if (!TraceFloorAtWorldXYForLayer(FVector2D(WorldSample.X, WorldSample.Y), TargetFloorHeights[TransitionIndex], LowerFloorHit))
				{
					continue;
				}

				const float RandomYaw = 90.0f * StairStream.RandRange(0, 3);
				const FQuat ActorRotation = (GetActorQuat() * FRotator(0.0f, RandomYaw, 0.0f).Quaternion() * StairEntry->RotationOffset.Quaternion()).GetNormalized();
				const FVector WorldOffset = ActorRotation.RotateVector(StairEntry->PositionOffset);
				const FVector ActorLocation(WorldSample.X + WorldOffset.X, WorldSample.Y + WorldOffset.Y, LowerFloorHit.ImpactPoint.Z + WorldOffset.Z);
				TArray<FBox> StairPlacementExclusions = AcceptedBounds;
				StairPlacementExclusions.Append(ReservedCorridorBounds);
				if (!IsPlacementClear(ActorLocation, ActorRotation, StairEntry->CollisionHalfExtent, StairPlacementExclusions, LowerFloorHit.GetActor()))
				{
					continue;
				}

				const FBox StairBound = BuildPlacementBounds(ActorLocation, ActorRotation, StairEntry->CollisionHalfExtent);
				const FVector RawLowerAccess = ActorLocation + ActorRotation.RotateVector(StairEntry->LowerAccessPointOffset);
				const FVector RawUpperAccess = ActorLocation + ActorRotation.RotateVector(StairEntry->UpperAccessPointOffset);
				FHitResult LowerAccessFloorHit;
				FHitResult UpperAccessFloorHit;
				if (!TraceFloorAtWorldXYForLayer(FVector2D(RawLowerAccess.X, RawLowerAccess.Y), TargetFloorHeights[TransitionIndex], LowerAccessFloorHit)
					|| !TraceFloorAtWorldXYForLayer(FVector2D(RawUpperAccess.X, RawUpperAccess.Y), TargetFloorHeights[TransitionIndex + 1], UpperAccessFloorHit))
				{
					continue;
				}

				const FVector LowerAccess(RawLowerAccess.X, RawLowerAccess.Y, LowerAccessFloorHit.ImpactPoint.Z);
				const FVector UpperAccess(RawUpperAccess.X, RawUpperAccess.Y, UpperAccessFloorHit.ImpactPoint.Z);
				TArray<FBox> NewCorridorBounds;
				TArray<FVector> NewPathPoints;
				if (const TArray<FVector>* IncomingAccessPoints = StairAccessPointsByFloor.Find(TransitionIndex))
				{
					TArray<FBox> CorridorObstacles = NavigationObstacleBounds;
					CorridorObstacles.Add(StairBound);
					if (IncomingAccessPoints->IsEmpty()
						|| !TryBuildConnectivityCorridor((*IncomingAccessPoints)[0], LowerAccess, TargetFloorHeights[TransitionIndex], CorridorObstacles, StairStream.RandRange(0, 1) == 0, NewCorridorBounds, NewPathPoints))
					{
						continue;
					}
				}

				const FGuid StableId = FInteriorPCGPlacementMath::MakeStableGuid(StairStream);
				const FTransform StairTransform(ActorRotation, ActorLocation, StairEntry->Scale);
				if (!SpawnConfiguredActor(StairEntry->AssetKind, StairEntry->StaticMesh, StairEntry->ActorClass, StairTransform, StableId, StairEntry->EntryId, StairEntry->CollisionHalfExtent, WorldOffset.Z, false, EInteriorPCGItemRole::Stair, TransitionIndex))
				{
					continue;
				}

				++SpawnedCount;
				AcceptedBounds.Add(StairBound);
				NavigationObstacleBounds.Add(StairBound);
				ReservedCorridorBounds.Append(NewCorridorBounds);
				LastConnectivityPathPoints.Append(NewPathPoints);
				StairAccessPointsByFloor.FindOrAdd(TransitionIndex).Add(LowerAccess);
				StairAccessPointsByFloor.FindOrAdd(TransitionIndex + 1).Add(UpperAccess);
				bStairPlaced = true;
			}

			if (!bStairPlaced)
			{
				UE_LOG(LogInteriorPCGWalls, Warning, TEXT("%s could not place a connected stair for floor transition %d."), *GetName(), TransitionIndex);
				ClearGeneratedWalls();
				LastConnectivityPathPoints.Reset();
				return 0;
			}
		}
	}

	for (int32 FloorIndex = 0; FloorIndex < TargetFloorHeights.Num(); ++FloorIndex)
	{
		const bool bUseTargetFloor = bGenerateOnAllDetectedFloors;
		const float TargetFloorWorldZ = TargetFloorHeights[FloorIndex];
		FRandomStream Stream(bUseTargetFloor ? MakeFloorSeed(WallSeed, FloorIndex) : WallSeed);
		if (!SelectWallClass(WallClasses, Stream))
		{
			UE_LOG(LogInteriorPCGWalls, Warning, TEXT("%s has no enabled wall class with a positive weight."), *GetName());
			if (bConnectivityRequired)
			{
				ClearGeneratedWalls();
				LastConnectivityPathPoints.Reset();
				return 0;
			}
			return SpawnedCount;
		}
		const bool bFloorRequiresConnectivity = bConnectivityRequired && StairAccessPointsByFloor.Contains(FloorIndex);

		for (int32 PartitionIndex = 0; PartitionIndex < FMath::Max(0, PartitionWallCount); ++PartitionIndex)
		{
			bool bPartitionPlaced = false;
			for (int32 Attempt = 0; Attempt < FMath::Max(1, MaxWallPlacementAttempts) && !bPartitionPlaced; ++Attempt)
			{
				const bool bAlongLocalX = WallDirectionMode == EInteriorPCGWallDirectionMode::VolumeLocalX
					|| (WallDirectionMode == EInteriorPCGWallDirectionMode::RandomPerPartition && Stream.RandRange(0, 1) == 0);
				const FVector LocalDirection = bAlongLocalX ? FVector::ForwardVector : FVector::RightVector;
				const FVector WorldDirection = VolumeTransform.TransformVectorNoScale(LocalDirection).GetSafeNormal();

				const double MinX = LocalBounds.Min.X + FMath::Min<double>(PartitionMargin, LocalBounds.GetSize().X * 0.45);
				const double MaxX = LocalBounds.Max.X - FMath::Min<double>(PartitionMargin, LocalBounds.GetSize().X * 0.45);
				const double MinY = LocalBounds.Min.Y + FMath::Min<double>(PartitionMargin, LocalBounds.GetSize().Y * 0.45);
				const double MaxY = LocalBounds.Max.Y - FMath::Min<double>(PartitionMargin, LocalBounds.GetSize().Y * 0.45);
				const FVector LocalSample(Stream.FRandRange(MinX, MaxX), Stream.FRandRange(MinY, MaxY), LocalBounds.GetCenter().Z);
				const FVector WorldSample = VolumeTransform.TransformPosition(LocalSample);

				FHitResult CenterFloorHit;
				const bool bFoundCenterFloor = bUseTargetFloor
					? TraceFloorAtWorldXYForLayer(FVector2D(WorldSample.X, WorldSample.Y), TargetFloorWorldZ, CenterFloorHit)
					: TraceFloorAtWorldXY(FVector2D(WorldSample.X, WorldSample.Y), CenterFloorHit);
				if (!bFoundCenterFloor)
				{
					continue;
				}

				const FVector ScanOrigin(WorldSample.X, WorldSample.Y, CenterFloorHit.ImpactPoint.Z + WallScanHeight);
				FVector NegativeHit;
				FVector PositiveHit;
				if (!FindBoundarySpan(ScanOrigin, WorldDirection, NegativeHit, PositiveHit))
				{
					continue;
				}

				const double RawSpan = FVector::DotProduct(PositiveHit - NegativeHit, WorldDirection) - 2.0 * WallEndClearance;
				const int32 ModuleCount = FMath::FloorToInt(RawSpan / SafeModuleLength);
				if (ModuleCount < 1)
				{
					continue;
				}

				const FInteriorPCGWallClassEntry* DoorEntryProbe = SelectWallClass(DoorWallClasses, Stream);
				const bool bCanPlaceDoor = DoorEntryProbe && ModuleCount > DoorEndPaddingModules * 2;
				if (bFloorRequiresConnectivity && !bCanPlaceDoor)
				{
					continue;
				}
				const int32 DoorModuleIndex = bCanPlaceDoor && (bFloorRequiresConnectivity || Stream.FRand() <= DoorChancePerPartition)
					? Stream.RandRange(DoorEndPaddingModules, ModuleCount - 1 - DoorEndPaddingModules)
					: INDEX_NONE;

				const FVector SpanCenter = (NegativeHit + PositiveHit) * 0.5;
				const double FirstModuleOffset = -0.5 * SafeModuleLength * (ModuleCount - 1);
				const FQuat DirectionRotation = FRotationMatrix::MakeFromX(WorldDirection).ToQuat();
				TArray<InteriorPCGWallVolumePrivate::FPlannedWallModule> PlannedModules;
				TArray<FBox> PlannedBounds = AcceptedBounds;
				TArray<FBox> PlannedNavigationObstacleBounds = NavigationObstacleBounds;
				TArray<FBox> PlannedReservedCorridorBounds = ReservedCorridorBounds;
				TArray<FVector> PlannedPathPoints;
				int32 PlannedDoorIndex = INDEX_NONE;
				bool bAllModulesValid = true;

				for (int32 ModuleIndex = 0; ModuleIndex < ModuleCount; ++ModuleIndex)
				{
					const bool bDoorModule = ModuleIndex == DoorModuleIndex;
					const FInteriorPCGWallClassEntry* Entry = bDoorModule
						? SelectWallClass(DoorWallClasses, Stream)
						: SelectWallClass(WallClasses, Stream);
					if (!Entry)
					{
						bAllModulesValid = false;
						break;
					}

					const FVector ModuleXY = SpanCenter + WorldDirection * (FirstModuleOffset + ModuleIndex * SafeModuleLength);
					FHitResult ModuleFloorHit;
					const bool bFoundModuleFloor = bUseTargetFloor
						? TraceFloorAtWorldXYForLayer(FVector2D(ModuleXY.X, ModuleXY.Y), TargetFloorWorldZ, ModuleFloorHit)
						: TraceFloorAtWorldXY(FVector2D(ModuleXY.X, ModuleXY.Y), ModuleFloorHit);
					if (!bFoundModuleFloor)
					{
						bAllModulesValid = false;
						break;
					}

					const FQuat ActorRotation = (DirectionRotation * Entry->RotationOffset.Quaternion()).GetNormalized();
					const FVector WorldOffset = DirectionRotation.RotateVector(Entry->PositionOffset);
					const FVector ActorLocation(ModuleXY.X + WorldOffset.X, ModuleXY.Y + WorldOffset.Y, ModuleFloorHit.ImpactPoint.Z + WorldOffset.Z);
					TArray<FBox> ModuleExclusionBounds = PlannedBounds;
					if (!bDoorModule)
					{
						ModuleExclusionBounds.Append(ReservedCorridorBounds);
					}
					if (!IsPlacementClear(ActorLocation, ActorRotation, Entry->CollisionHalfExtent, ModuleExclusionBounds, ModuleFloorHit.GetActor()))
					{
						bAllModulesValid = false;
						break;
					}

					InteriorPCGWallVolumePrivate::FPlannedWallModule& Planned = PlannedModules.AddDefaulted_GetRef();
					Planned.Entry = Entry;
					Planned.Transform = FTransform(ActorRotation, ActorLocation, Entry->Scale);
					Planned.StableId = FInteriorPCGPlacementMath::MakeStableGuid(Stream);
					Planned.Role = bDoorModule ? EInteriorPCGItemRole::DoorWall : EInteriorPCGItemRole::InteriorWall;
					Planned.FloorHeightOffset = WorldOffset.Z;
					Planned.FloorIndex = bUseTargetFloor ? FloorIndex : INDEX_NONE;
					const FBox ModuleBound = BuildPlacementBounds(ActorLocation, ActorRotation, Entry->CollisionHalfExtent);
					PlannedBounds.Add(ModuleBound);
					if (bDoorModule)
					{
						PlannedDoorIndex = PlannedModules.Num() - 1;
					}
					else
					{
						PlannedNavigationObstacleBounds.Add(ModuleBound);
					}
				}

				if (!bAllModulesValid)
				{
					continue;
				}
				if (bFloorRequiresConnectivity)
				{
					const TArray<FVector>* StairAccessPoints = StairAccessPointsByFloor.Find(FloorIndex);
					if (!StairAccessPoints || StairAccessPoints->IsEmpty() || !PlannedModules.IsValidIndex(PlannedDoorIndex))
					{
						continue;
					}

					const InteriorPCGWallVolumePrivate::FPlannedWallModule& DoorModule = PlannedModules[PlannedDoorIndex];
					const FVector DoorAccessPoint = DoorModule.Transform.TransformPosition(DoorModule.Entry->DoorAccessPointOffset);
					const FVector* ClosestStairAccess = &(*StairAccessPoints)[0];
					double ClosestDistanceSquared = FVector2D(DoorAccessPoint - *ClosestStairAccess).SizeSquared();
					for (const FVector& StairAccessPoint : *StairAccessPoints)
					{
						const double DistanceSquared = FVector2D(DoorAccessPoint - StairAccessPoint).SizeSquared();
						if (DistanceSquared < ClosestDistanceSquared)
						{
							ClosestDistanceSquared = DistanceSquared;
							ClosestStairAccess = &StairAccessPoint;
						}
					}

					TArray<FBox> DoorCorridorBounds;
					if (!TryBuildConnectivityCorridor(DoorAccessPoint, *ClosestStairAccess, TargetFloorWorldZ, PlannedNavigationObstacleBounds, Stream.RandRange(0, 1) == 0, DoorCorridorBounds, PlannedPathPoints))
					{
						continue;
					}
					PlannedReservedCorridorBounds.Append(DoorCorridorBounds);
				}

				bool bAllModulesSpawned = true;
				for (const InteriorPCGWallVolumePrivate::FPlannedWallModule& Planned : PlannedModules)
				{
					if (SpawnConfiguredActor(Planned.Entry->AssetKind, Planned.Entry->StaticMesh, Planned.Entry->ActorClass, Planned.Transform, Planned.StableId, Planned.Entry->EntryId, Planned.Entry->CollisionHalfExtent, Planned.FloorHeightOffset, false, Planned.Role, Planned.FloorIndex))
					{
						++SpawnedCount;
					}
					else
					{
						bAllModulesSpawned = false;
					}
				}
				if (bFloorRequiresConnectivity && !bAllModulesSpawned)
				{
					ClearGeneratedWalls();
					LastConnectivityPathPoints.Reset();
					return 0;
				}
				AcceptedBounds = MoveTemp(PlannedBounds);
				NavigationObstacleBounds = MoveTemp(PlannedNavigationObstacleBounds);
				ReservedCorridorBounds = MoveTemp(PlannedReservedCorridorBounds);
				LastConnectivityPathPoints.Append(PlannedPathPoints);
				bPartitionPlaced = true;
			}
			if (bFloorRequiresConnectivity && !bPartitionPlaced)
			{
				UE_LOG(LogInteriorPCGWalls, Warning, TEXT("%s could not connect a door to a stair on floor %d."), *GetName(), FloorIndex);
				ClearGeneratedWalls();
				LastConnectivityPathPoints.Reset();
				return 0;
			}
		}
	}

	Modify();
	return SpawnedCount;
}

int32 AInteriorPCGWallVolume::GenerateWallsAndInterior()
{
	const int32 InteriorCount = GenerateRandomInterior();
	return InteriorCount + GenerateInteriorWalls();
}

void AInteriorPCGWallVolume::ExecuteGraphGeneration()
{
	if (GraphGenerationMode == EInteriorPCGGraphGenerationMode::RandomEntries && bGenerateWallsWithRandomGraphGeneration)
	{
		GenerateWallsAndInterior();
		return;
	}

	Super::ExecuteGraphGeneration();
}
