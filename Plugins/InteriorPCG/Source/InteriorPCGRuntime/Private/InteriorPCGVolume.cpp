#include "InteriorPCGVolume.h"

#include "InteriorPCGItemComponent.h"
#include "InteriorPCGPreset.h"
#include "InteriorPCGStaticMeshActor.h"

#include "Components/BrushComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "PCGComponent.h"
#include "PCGGraph.h"

DEFINE_LOG_CATEGORY_STATIC(LogInteriorPCG, Log, All);

AInteriorPCGVolume::AInteriorPCGVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsEditorOnlyActor = true;

	if (PCGComponent)
	{
		PCGComponent->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
		PCGComponent->bIsComponentPartitioned = false;
#if WITH_EDITORONLY_DATA
		PCGComponent->bRegenerateInEditor = false;
		PCGComponent->bOnlyTrackItself = true;
#endif
	}
}

void AInteriorPCGVolume::PostLoad()
{
	Super::PostLoad();
	EnsureEntryIds();
	TryAssignDefaultGraph();
}

#if WITH_EDITOR
void AInteriorPCGVolume::PostActorCreated()
{
	Super::PostActorCreated();
	EnsureEntryIds();
	TryAssignDefaultGraph();
}

void AInteriorPCGVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	EnsureEntryIds();
	if (PCGComponent)
	{
		PCGComponent->Seed = Seed;
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void AInteriorPCGVolume::EnsureEntryIds()
{
	TSet<FGuid> UsedIds;
	for (FInteriorPCGAssetEntry& Entry : AssetEntries)
	{
		if (!Entry.EntryId.IsValid() || UsedIds.Contains(Entry.EntryId))
		{
			Entry.EntryId = FGuid::NewGuid();
		}
		UsedIds.Add(Entry.EntryId);
	}
}

bool AInteriorPCGVolume::GetVolumeLocalBounds(FBox& OutLocalBounds) const
{
	const UBrushComponent* VolumeBrushComponent = GetBrushComponent();
	if (!VolumeBrushComponent)
	{
		return false;
	}

	OutLocalBounds = VolumeBrushComponent->CalcBounds(FTransform::Identity).GetBox();
	return OutLocalBounds.IsValid && OutLocalBounds.GetSize().X > UE_DOUBLE_SMALL_NUMBER && OutLocalBounds.GetSize().Y > UE_DOUBLE_SMALL_NUMBER;
}

void AInteriorPCGVolume::GetRegisteredActors(TArray<AActor*>& OutActors) const
{
	OutActors.Reset();
	TSet<AActor*> UniqueActors;

	for (const TSoftObjectPtr<AActor>& ActorReference : RegisteredActors)
	{
		if (AActor* Actor = ActorReference.Get())
		{
			if (!Actor->IsActorBeingDestroyed() && FindItemComponent(Actor))
			{
				UniqueActors.Add(Actor);
			}
		}
	}

	if (const UWorld* World = GetWorld())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (const UInteriorPCGItemComponent* ItemComponent = Actor->FindComponentByClass<UInteriorPCGItemComponent>())
			{
				if (ItemComponent->Generator.Get() == this)
				{
					UniqueActors.Add(Actor);
				}
			}
		}
	}

	OutActors = UniqueActors.Array();
	OutActors.Sort([](const AActor& A, const AActor& B)
	{
		return A.GetPathName() < B.GetPathName();
	});
}

UInteriorPCGItemComponent* AInteriorPCGVolume::FindItemComponent(const AActor* Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}

	UInteriorPCGItemComponent* ItemComponent = Actor->FindComponentByClass<UInteriorPCGItemComponent>();
	return ItemComponent && ItemComponent->Generator.Get() == this ? ItemComponent : nullptr;
}

bool AInteriorPCGVolume::TraceFloorAtWorldXY(const FVector2D& WorldXY, FHitResult& OutHit, const AActor* AdditionalIgnoredActor) const
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

	const FVector Start(WorldXY.X, WorldXY.Y, WorldBounds.Max.Z + FloorTracePadding);
	const FVector End(WorldXY.X, WorldXY.Y, WorldBounds.Min.Z - FloorTracePadding);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(InteriorPCGFloorTrace), true, this);
	QueryParams.AddIgnoredActor(this);
	if (AdditionalIgnoredActor)
	{
		QueryParams.AddIgnoredActor(AdditionalIgnoredActor);
	}

	TArray<AActor*> ExistingActors;
	GetRegisteredActors(ExistingActors);
	QueryParams.AddIgnoredActors(ExistingActors);

	TArray<FHitResult> Hits;
	if (!World->LineTraceMultiByChannel(Hits, Start, End, FloorTraceChannel, QueryParams))
	{
		return false;
	}

	for (int32 Index = Hits.Num() - 1; Index >= 0; --Index)
	{
		const FHitResult& Hit = Hits[Index];
		if (!Hit.bBlockingHit || Hit.ImpactNormal.Z < MinimumFloorNormalZ)
		{
			continue;
		}

		if (!EncompassesPoint(Hit.ImpactPoint + FVector::UpVector * 2.0f))
		{
			continue;
		}

		OutHit = Hit;
		return true;
	}

	return false;
}

bool AInteriorPCGVolume::TraceAllFloorSurfacesAtWorldXY(const FVector2D& WorldXY, TArray<FHitResult>& OutHits, const AActor* AdditionalIgnoredActor) const
{
	OutHits.Reset();
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

	const FVector Start(WorldXY.X, WorldXY.Y, WorldBounds.Max.Z + FloorTracePadding);
	const FVector End(WorldXY.X, WorldXY.Y, WorldBounds.Min.Z - FloorTracePadding);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(InteriorPCGAllFloorTrace), true, this);
	QueryParams.AddIgnoredActor(this);
	if (AdditionalIgnoredActor)
	{
		QueryParams.AddIgnoredActor(AdditionalIgnoredActor);
	}

	TArray<AActor*> ExistingActors;
	GetRegisteredActors(ExistingActors);
	QueryParams.AddIgnoredActors(ExistingActors);

	TArray<FHitResult> RawHits;
	const FCollisionResponseParams OverlapResponses(ECR_Overlap);
	World->LineTraceMultiByChannel(RawHits, Start, End, FloorTraceChannel, QueryParams, OverlapResponses);
	for (const FHitResult& Hit : RawHits)
	{
		const UPrimitiveComponent* HitComponent = Hit.GetComponent();
		if (!HitComponent || HitComponent->GetCollisionResponseToChannel(FloorTraceChannel) != ECR_Block)
		{
			continue;
		}

		if (Hit.ImpactNormal.Z < MinimumFloorNormalZ || !EncompassesPoint(Hit.ImpactPoint + FVector::UpVector * 2.0f))
		{
			continue;
		}

		const bool bDuplicateSurface = OutHits.ContainsByPredicate([&Hit](const FHitResult& ExistingHit)
		{
			return FMath::IsNearlyEqual(ExistingHit.ImpactPoint.Z, Hit.ImpactPoint.Z, 1.0f);
		});
		if (!bDuplicateSurface)
		{
			OutHits.Add(Hit);
		}
	}

	OutHits.Sort([](const FHitResult& A, const FHitResult& B)
	{
		return A.ImpactPoint.Z < B.ImpactPoint.Z;
	});
	return !OutHits.IsEmpty();
}

bool AInteriorPCGVolume::TraceFloorAtWorldXYForLayer(const FVector2D& WorldXY, const float TargetFloorWorldZ, FHitResult& OutHit, const AActor* AdditionalIgnoredActor) const
{
	TArray<FHitResult> FloorHits;
	if (!TraceAllFloorSurfacesAtWorldXY(WorldXY, FloorHits, AdditionalIgnoredActor))
	{
		return false;
	}

	const float AllowedDistance = FMath::Max(1.0f, FloorLayerHeightTolerance);
	const FHitResult* ClosestHit = nullptr;
	float ClosestDistance = TNumericLimits<float>::Max();
	for (const FHitResult& Hit : FloorHits)
	{
		const float Distance = FMath::Abs(Hit.ImpactPoint.Z - TargetFloorWorldZ);
		if (Distance <= AllowedDistance && Distance < ClosestDistance)
		{
			ClosestHit = &Hit;
			ClosestDistance = Distance;
		}
	}

	if (!ClosestHit)
	{
		return false;
	}

	OutHit = *ClosestHit;
	return true;
}

bool AInteriorPCGVolume::DetectFloorLayers(TArray<float>& OutFloorWorldHeights) const
{
	OutFloorWorldHeights.Reset();
	FBox LocalBounds;
	if (!GetVolumeLocalBounds(LocalBounds))
	{
		return false;
	}

	struct FFloorCluster
	{
		double HeightSum = 0.0;
		int32 HitCount = 0;
		TSet<int32> SampleIndices;

		double GetAverageHeight() const
		{
			return HitCount > 0 ? HeightSum / HitCount : 0.0;
		}
	};

	const int32 SamplesPerAxis = FMath::Clamp(FloorDetectionSamplesPerAxis, 1, 16);
	const int32 TotalSampleCount = SamplesPerAxis * SamplesPerAxis;
	const FTransform VolumeTransform = GetActorTransform();
	const float MergeTolerance = FMath::Max(1.0f, FloorLayerHeightTolerance);
	TArray<FFloorCluster> Clusters;

	for (int32 YIndex = 0; YIndex < SamplesPerAxis; ++YIndex)
	{
		for (int32 XIndex = 0; XIndex < SamplesPerAxis; ++XIndex)
		{
			const double XAlpha = (XIndex + 0.5) / SamplesPerAxis;
			const double YAlpha = (YIndex + 0.5) / SamplesPerAxis;
			const FVector LocalSample(
				FMath::Lerp(LocalBounds.Min.X, LocalBounds.Max.X, XAlpha),
				FMath::Lerp(LocalBounds.Min.Y, LocalBounds.Max.Y, YAlpha),
				LocalBounds.GetCenter().Z);
			const FVector WorldSample = VolumeTransform.TransformPosition(LocalSample);
			const int32 SampleIndex = YIndex * SamplesPerAxis + XIndex;
			TArray<FHitResult> FloorHits;
			TraceAllFloorSurfacesAtWorldXY(FVector2D(WorldSample.X, WorldSample.Y), FloorHits);

			for (const FHitResult& Hit : FloorHits)
			{
				FFloorCluster* ClosestCluster = nullptr;
				double ClosestDistance = TNumericLimits<double>::Max();
				for (FFloorCluster& Cluster : Clusters)
				{
					const double Distance = FMath::Abs(Cluster.GetAverageHeight() - Hit.ImpactPoint.Z);
					if (Distance <= MergeTolerance && Distance < ClosestDistance)
					{
						ClosestCluster = &Cluster;
						ClosestDistance = Distance;
					}
				}

				if (!ClosestCluster)
				{
					ClosestCluster = &Clusters.AddDefaulted_GetRef();
				}
				ClosestCluster->HeightSum += Hit.ImpactPoint.Z;
				++ClosestCluster->HitCount;
				ClosestCluster->SampleIndices.Add(SampleIndex);
			}
		}
	}

	const int32 RequiredSamples = FMath::Max(1, FMath::CeilToInt(TotalSampleCount * FMath::Clamp(MinimumFloorSampleCoverage, 0.0f, 1.0f)));
	for (const FFloorCluster& Cluster : Clusters)
	{
		if (Cluster.SampleIndices.Num() >= RequiredSamples)
		{
			OutFloorWorldHeights.Add(static_cast<float>(Cluster.GetAverageHeight()));
		}
	}

	OutFloorWorldHeights.Sort();
	if (OutFloorWorldHeights.Num() > FMath::Max(1, MaximumDetectedFloorCount))
	{
		OutFloorWorldHeights.SetNum(FMath::Max(1, MaximumDetectedFloorCount));
	}
	return !OutFloorWorldHeights.IsEmpty();
}

int32 AInteriorPCGVolume::ScanFloorLayers()
{
	DetectFloorLayers(LastDetectedFloorWorldHeights);
	return LastDetectedFloorWorldHeights.Num();
}

bool AInteriorPCGVolume::IsPlacementClear(const FVector& ActorLocation, const FQuat& ActorRotation, const FVector& CollisionHalfExtent, const TArray<FBox>& AcceptedBounds, const AActor* FloorActor) const
{
	const FVector SafeExtent(
		FMath::Max(1.0, CollisionHalfExtent.X),
		FMath::Max(1.0, CollisionHalfExtent.Y),
		FMath::Max(1.0, CollisionHalfExtent.Z));

	const FVector CollisionCenter = ActorLocation + FVector::UpVector * (SafeExtent.Z + CollisionFloorClearance);
	if (!EncompassesPoint(CollisionCenter, FMath::Max(SafeExtent.X, SafeExtent.Y)))
	{
		return false;
	}

	const FBox CandidateBounds = BuildPlacementBounds(ActorLocation, ActorRotation, SafeExtent);
	for (const FBox& AcceptedBound : AcceptedBounds)
	{
		if (CandidateBounds.Intersect(AcceptedBound))
		{
			return false;
		}
	}

	if (!bCheckWorldCollision)
	{
		return true;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(InteriorPCGPlacementOverlap), false, this);
	QueryParams.AddIgnoredActor(this);

	TArray<FOverlapResult> Overlaps;
	const bool bHasBlockingOverlap = World->OverlapMultiByChannel(
		Overlaps,
		CollisionCenter,
		ActorRotation,
		PlacementCollisionChannel,
		FCollisionShape::MakeBox(SafeExtent),
		QueryParams);

	if (!bHasBlockingOverlap)
	{
		return true;
	}

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

		if (OverlapActor == FloorActor)
		{
			const FBox FloorBounds = OverlapActor->GetComponentsBoundingBox(true);
			if (FloorBounds.IsValid && FloorBounds.Max.Z <= ActorLocation.Z + CollisionFloorClearance + 0.5f)
			{
				continue;
			}
		}

		return false;
	}

	return true;
}

bool AInteriorPCGVolume::TryFindRandomPlacement(const FInteriorPCGAssetEntry& Entry, FRandomStream& Stream, const FBox& LocalBounds, const TArray<FBox>& AcceptedBounds, FTransform& OutTransform, AActor*& OutFloorActor, const bool bUseTargetFloor, const float TargetFloorWorldZ) const
{
	const FTransform VolumeTransform = GetActorTransform();
	for (int32 Attempt = 0; Attempt < MaxPlacementAttemptsPerItem; ++Attempt)
	{
		const FVector LocalSample(
			Stream.FRandRange(LocalBounds.Min.X, LocalBounds.Max.X),
			Stream.FRandRange(LocalBounds.Min.Y, LocalBounds.Max.Y),
			LocalBounds.GetCenter().Z);
		const FVector WorldSample = VolumeTransform.TransformPosition(LocalSample);
		const FVector WorldOffset = VolumeTransform.TransformVectorNoScale(Entry.PositionOffset);
		const FVector2D CorrectedWorldXY(WorldSample.X + WorldOffset.X, WorldSample.Y + WorldOffset.Y);

		FHitResult FloorHit;
		const bool bFoundFloor = bUseTargetFloor
			? TraceFloorAtWorldXYForLayer(CorrectedWorldXY, TargetFloorWorldZ, FloorHit)
			: TraceFloorAtWorldXY(CorrectedWorldXY, FloorHit);
		if (!bFoundFloor)
		{
			continue;
		}

		const float RandomYaw = FInteriorPCGPlacementMath::ResolveYaw(Entry.RotationMode, Entry.YawStepDegrees, Stream);
		const FQuat ActorRotation = (GetActorQuat() * FRotator(0.0f, RandomYaw, 0.0f).Quaternion() * Entry.RotationOffset.Quaternion()).GetNormalized();
		const FVector ActorLocation(CorrectedWorldXY.X, CorrectedWorldXY.Y, FloorHit.ImpactPoint.Z + WorldOffset.Z);

		if (!IsPlacementClear(ActorLocation, ActorRotation, Entry.CollisionHalfExtent, AcceptedBounds, FloorHit.GetActor()))
		{
			continue;
		}

		OutTransform = FTransform(ActorRotation, ActorLocation, FVector::OneVector);
		OutFloorActor = FloorHit.GetActor();
		return true;
	}

	return false;
}

int32 AInteriorPCGVolume::MakeFloorSeed(const int32 BaseSeed, const int32 FloorIndex)
{
	return static_cast<int32>(HashCombineFast(GetTypeHash(BaseSeed), GetTypeHash(FloorIndex + 1)));
}

FBox AInteriorPCGVolume::BuildPlacementBounds(const FVector& ActorLocation, const FQuat& ActorRotation, const FVector& CollisionHalfExtent)
{
	const FVector SafeExtent(
		FMath::Max(1.0, CollisionHalfExtent.X),
		FMath::Max(1.0, CollisionHalfExtent.Y),
		FMath::Max(1.0, CollisionHalfExtent.Z));
	const FVector Center = ActorLocation + FVector::UpVector * SafeExtent.Z;
	return FBox(FVector(-SafeExtent.X, -SafeExtent.Y, -SafeExtent.Z), SafeExtent)
		.TransformBy(FTransform(ActorRotation, Center));
}

AActor* AInteriorPCGVolume::SpawnConfiguredActor(const EInteriorPCGAssetKind AssetKind, const TSoftObjectPtr<UStaticMesh>& StaticMesh, const TSoftClassPtr<AActor>& ActorClass, const FTransform& Transform, const FGuid& StableId, const FGuid& SourceEntryId, const FVector& CollisionHalfExtent, const float FloorHeightOffset, const bool bUserAdded, const EInteriorPCGItemRole ItemRole, const int32 FloorIndex)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UClass* ClassToSpawn = nullptr;
	UStaticMesh* LoadedMesh = nullptr;
	if (AssetKind == EInteriorPCGAssetKind::StaticMesh)
	{
		LoadedMesh = StaticMesh.LoadSynchronous();
		ClassToSpawn = LoadedMesh ? AInteriorPCGStaticMeshActor::StaticClass() : nullptr;
	}
	else
	{
		ClassToSpawn = ActorClass.LoadSynchronous();
	}

	if (!ClassToSpawn || ClassToSpawn->HasAnyClassFlags(CLASS_Abstract) || !ClassToSpawn->IsChildOf(AActor::StaticClass()))
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.OverrideLevel = GetLevel();
	SpawnParameters.ObjectFlags |= RF_Transactional;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* SpawnedActor = World->SpawnActor<AActor>(ClassToSpawn, Transform, SpawnParameters);
	if (!SpawnedActor)
	{
		return nullptr;
	}

	SpawnedActor->SetFlags(RF_Transactional);
	SpawnedActor->Modify();
	if (AInteriorPCGStaticMeshActor* StaticMeshActor = Cast<AInteriorPCGStaticMeshActor>(SpawnedActor))
	{
		StaticMeshActor->GetStaticMeshComponent()->SetStaticMesh(LoadedMesh);
	}

	UInteriorPCGItemComponent* ItemComponent = NewObject<UInteriorPCGItemComponent>(SpawnedActor, NAME_None, RF_Transactional);
	ItemComponent->StableId = StableId.IsValid() ? StableId : FGuid::NewGuid();
	ItemComponent->SourceEntryId = SourceEntryId;
	ItemComponent->Generator = this;
	ItemComponent->CollisionHalfExtent = CollisionHalfExtent;
	ItemComponent->FloorHeightOffset = FloorHeightOffset;
	ItemComponent->AssetKind = AssetKind;
	ItemComponent->Role = ItemRole;
	ItemComponent->FloorIndex = FloorIndex;
	ItemComponent->bUserAdded = bUserAdded;
	SpawnedActor->AddInstanceComponent(ItemComponent);
	ItemComponent->OnComponentCreated();
	ItemComponent->RegisterComponent();

#if WITH_EDITOR
	SpawnedActor->SetActorLabel(FString::Printf(TEXT("PCG_%s"), *ItemComponent->StableId.ToString(EGuidFormats::Short)));
	SpawnedActor->SetFolderPath(FName(*FString::Printf(TEXT("InteriorPCG/%s"), *GetActorLabel())));
#endif

	RegisteredActors.AddUnique(SpawnedActor);
	if (ULevel* Level = SpawnedActor->GetLevel())
	{
		Level->MarkPackageDirty();
	}
	return SpawnedActor;
}

int32 AInteriorPCGVolume::ClearGeneratedActors()
{
	TArray<AActor*> ActorsToDestroy;
	GetRegisteredActors(ActorsToDestroy);
	int32 RemovedCount = 0;

	for (AActor* Actor : ActorsToDestroy)
	{
		if (!IsValid(Actor) || Actor == this)
		{
			continue;
		}

		Actor->Modify();
#if WITH_EDITOR
		if (UWorld* World = Actor->GetWorld())
		{
			if (World->WorldType == EWorldType::Editor || World->WorldType == EWorldType::EditorPreview)
			{
				RemovedCount += World->EditorDestroyActor(Actor, true) ? 1 : 0;
				continue;
			}
		}
#endif
		RemovedCount += Actor->Destroy() ? 1 : 0;
	}

	RegisteredActors.Reset();
	Modify();
	return RemovedCount;
}

int32 AInteriorPCGVolume::GenerateRandomInterior()
{
	EnsureEntryIds();
	ClearGeneratedActors();

	FBox LocalBounds;
	if (!GetVolumeLocalBounds(LocalBounds))
	{
		UE_LOG(LogInteriorPCG, Warning, TEXT("%s has no valid brush bounds."), *GetName());
		return 0;
	}

	TArray<float> TargetFloorHeights;
	if (bGenerateOnAllDetectedFloors)
	{
		ScanFloorLayers();
		TargetFloorHeights = LastDetectedFloorWorldHeights;
		if (TargetFloorHeights.IsEmpty())
		{
			UE_LOG(LogInteriorPCG, Warning, TEXT("%s detected no floor layers."), *GetName());
			return 0;
		}
	}
	else
	{
		TargetFloorHeights.Add(0.0f);
	}

	TArray<FBox> AcceptedBounds;
	int32 SpawnedCount = 0;
	for (int32 FloorIndex = 0; FloorIndex < TargetFloorHeights.Num(); ++FloorIndex)
	{
		const bool bUseTargetFloor = bGenerateOnAllDetectedFloors;
		const float TargetFloorWorldZ = TargetFloorHeights[FloorIndex];
		FRandomStream Stream(bUseTargetFloor ? MakeFloorSeed(Seed, FloorIndex) : Seed);
		TArray<const FInteriorPCGAssetEntry*> PlacementRequests;
		TArray<const FInteriorPCGAssetEntry*> WeightedEntries;
		float TotalWeight = 0.0f;

		for (const FInteriorPCGAssetEntry& Entry : AssetEntries)
		{
			if (!Entry.bEnabled || !Entry.HasValidAssetReference())
			{
				continue;
			}

			if (Entry.QuantityMode == EInteriorPCGQuantityMode::FixedCount)
			{
				for (int32 Index = 0; Index < FMath::Max(0, Entry.Count); ++Index)
				{
					PlacementRequests.Add(&Entry);
				}
			}
			else if (Entry.SelectionWeight > 0.0f)
			{
				WeightedEntries.Add(&Entry);
				TotalWeight += Entry.SelectionWeight;
			}
		}

		for (int32 Index = 0; Index < WeightedSelectionCount && TotalWeight > 0.0f; ++Index)
		{
			const float Selection = Stream.FRandRange(0.0f, TotalWeight);
			float AccumulatedWeight = 0.0f;
			const FInteriorPCGAssetEntry* SelectedEntry = WeightedEntries.Last();
			for (const FInteriorPCGAssetEntry* Candidate : WeightedEntries)
			{
				AccumulatedWeight += Candidate->SelectionWeight;
				if (Selection <= AccumulatedWeight)
				{
					SelectedEntry = Candidate;
					break;
				}
			}
			PlacementRequests.Add(SelectedEntry);
		}

		for (const FInteriorPCGAssetEntry* Entry : PlacementRequests)
		{
			FTransform PlacementTransform;
			AActor* FloorActor = nullptr;
			if (!TryFindRandomPlacement(*Entry, Stream, LocalBounds, AcceptedBounds, PlacementTransform, FloorActor, bUseTargetFloor, TargetFloorWorldZ))
			{
				UE_LOG(LogInteriorPCG, Verbose, TEXT("No valid placement found for entry %s on floor %d."), *Entry->Label.ToString(), FloorIndex);
				continue;
			}

			const FGuid StableId = FInteriorPCGPlacementMath::MakeStableGuid(Stream);
			const float HeightOffset = PlacementTransform.GetLocation().Z;
			FHitResult FloorHit;
			const bool bResolvedFloor = bUseTargetFloor
				? TraceFloorAtWorldXYForLayer(FVector2D(PlacementTransform.GetLocation().X, PlacementTransform.GetLocation().Y), TargetFloorWorldZ, FloorHit)
				: TraceFloorAtWorldXY(FVector2D(PlacementTransform.GetLocation().X, PlacementTransform.GetLocation().Y), FloorHit);
			const float ResolvedFloorOffset = bResolvedFloor ? HeightOffset - FloorHit.ImpactPoint.Z : Entry->PositionOffset.Z;

			if (AActor* SpawnedActor = SpawnConfiguredActor(Entry->AssetKind, Entry->StaticMesh, Entry->ActorClass, PlacementTransform, StableId, Entry->EntryId, Entry->CollisionHalfExtent, ResolvedFloorOffset, false, EInteriorPCGItemRole::FurnitureOrProp, bUseTargetFloor ? FloorIndex : INDEX_NONE))
			{
				AcceptedBounds.Add(BuildPlacementBounds(SpawnedActor->GetActorLocation(), SpawnedActor->GetActorQuat(), Entry->CollisionHalfExtent));
				++SpawnedCount;
			}
		}
	}

	Modify();
	return SpawnedCount;
}

int32 AInteriorPCGVolume::GenerateFromSelectedPreset()
{
	return GenerateFromPreset(SelectedPreset);
}

int32 AInteriorPCGVolume::GenerateFromSelectedPresets()
{
	TArray<const UInteriorPCGPreset*> Presets;
	for (const UInteriorPCGPreset* Preset : SelectedPresets)
	{
		if (Preset)
		{
			Presets.AddUnique(Preset);
		}
	}

	return GenerateFromPresetList(Presets);
}

int32 AInteriorPCGVolume::GenerateFromPreset(const UInteriorPCGPreset* Preset)
{
	if (!Preset)
	{
		return 0;
	}

	TArray<const UInteriorPCGPreset*> Presets{ Preset };
	return GenerateFromPresetList(Presets);
}

int32 AInteriorPCGVolume::GenerateFromPresetList(const TArray<const UInteriorPCGPreset*>& Presets)
{
	if (Presets.IsEmpty())
	{
		return 0;
	}

	ClearGeneratedActors();

	FBox LocalBounds;
	if (!GetVolumeLocalBounds(LocalBounds))
	{
		return 0;
	}

	const FTransform VolumeTransform = GetActorTransform();
	TArray<FBox> AcceptedBounds;
	TSet<FGuid> UsedStableIds;
	int32 SpawnedCount = 0;
	TArray<float> TargetFloorHeights;
	if (bGenerateOnAllDetectedFloors)
	{
		ScanFloorLayers();
		TargetFloorHeights = LastDetectedFloorWorldHeights;
		if (TargetFloorHeights.IsEmpty())
		{
			return 0;
		}
	}
	else
	{
		TargetFloorHeights.Add(0.0f);
	}

	for (int32 FloorIndex = 0; FloorIndex < TargetFloorHeights.Num(); ++FloorIndex)
	{
		const bool bUseTargetFloor = bGenerateOnAllDetectedFloors;
		const float TargetFloorWorldZ = TargetFloorHeights[FloorIndex];
		for (int32 PresetIndex = 0; PresetIndex < Presets.Num(); ++PresetIndex)
		{
			const UInteriorPCGPreset* Preset = Presets[PresetIndex];
			if (!Preset)
			{
				continue;
			}

			for (int32 ItemIndex = 0; ItemIndex < Preset->Items.Num(); ++ItemIndex)
			{
				const FInteriorPCGPresetItem& Item = Preset->Items[ItemIndex];
				if (!Item.bEnabled || !Item.HasValidAssetReference())
				{
					continue;
				}
				if (bUseTargetFloor && Item.FloorIndex != INDEX_NONE && Item.FloorIndex != FloorIndex)
				{
					continue;
				}
				if (!bUseTargetFloor && Item.FloorIndex > 0)
				{
					continue;
				}

				const FVector LocalPosition = FInteriorPCGPlacementMath::DenormalizeLocalXY(Item.NormalizedPosition, LocalBounds, LocalBounds.GetCenter().Z);
				const FVector ProjectedWorldPosition = VolumeTransform.TransformPosition(LocalPosition);
				FHitResult FloorHit;
				const bool bFoundFloor = bUseTargetFloor
					? TraceFloorAtWorldXYForLayer(FVector2D(ProjectedWorldPosition.X, ProjectedWorldPosition.Y), TargetFloorWorldZ, FloorHit)
					: TraceFloorAtWorldXY(FVector2D(ProjectedWorldPosition.X, ProjectedWorldPosition.Y), FloorHit);
				if (!bFoundFloor)
				{
					continue;
				}

				const FVector ActorLocation(ProjectedWorldPosition.X, ProjectedWorldPosition.Y, FloorHit.ImpactPoint.Z + Item.FloorHeightOffset);
				const FQuat ActorRotation = (GetActorQuat() * Item.RelativeRotation.Quaternion()).GetNormalized();
				if (bCheckPresetCollision && !IsPlacementClear(ActorLocation, ActorRotation, Item.CollisionHalfExtent, AcceptedBounds, FloorHit.GetActor()))
				{
					continue;
				}

				FGuid StableId = Item.StableId;
				if (!StableId.IsValid() || UsedStableIds.Contains(StableId))
				{
					const uint32 RemapSeed = HashCombineFast(
						GetTypeHash(FloorIndex),
						HashCombineFast(GetTypeHash(PresetIndex), HashCombineFast(GetTypeHash(ItemIndex), GetTypeHash(Item.StableId))));
					FRandomStream StableIdStream(static_cast<int32>(RemapSeed));
					do
					{
						StableId = FInteriorPCGPlacementMath::MakeStableGuid(StableIdStream);
					}
					while (UsedStableIds.Contains(StableId));
				}

				const FTransform PlacementTransform(ActorRotation, ActorLocation, Item.Scale);
				const int32 SpawnFloorIndex = bUseTargetFloor ? FloorIndex : Item.FloorIndex;
				if (AActor* SpawnedActor = SpawnConfiguredActor(Item.AssetKind, Item.StaticMesh, Item.ActorClass, PlacementTransform, StableId, Item.SourceEntryId, Item.CollisionHalfExtent, Item.FloorHeightOffset, false, Item.Role, SpawnFloorIndex))
				{
					UsedStableIds.Add(StableId);
					AcceptedBounds.Add(BuildPlacementBounds(SpawnedActor->GetActorLocation(), SpawnedActor->GetActorQuat(), Item.CollisionHalfExtent));
					++SpawnedCount;
				}
			}
		}
	}

	Modify();
	return SpawnedCount;
}

bool AInteriorPCGVolume::RegisterActor(AActor* Actor)
{
	if (!Actor || Actor == this || Actor->GetWorld() != GetWorld())
	{
		return false;
	}

	UInteriorPCGItemComponent* ItemComponent = Actor->FindComponentByClass<UInteriorPCGItemComponent>();
	if (ItemComponent && !ItemComponent->Generator.IsNull() && ItemComponent->Generator.Get() != this)
	{
		return false;
	}

	Actor->Modify();
	if (!ItemComponent)
	{
		ItemComponent = NewObject<UInteriorPCGItemComponent>(Actor, NAME_None, RF_Transactional);
		Actor->AddInstanceComponent(ItemComponent);
		ItemComponent->OnComponentCreated();
		ItemComponent->RegisterComponent();
	}
	else
	{
		ItemComponent->Modify();
	}

	ItemComponent->Generator = this;
	ItemComponent->StableId = ItemComponent->StableId.IsValid() ? ItemComponent->StableId : FGuid::NewGuid();
	ItemComponent->bUserAdded = true;
	ItemComponent->bIncludedInPreset = true;

	const bool bPlainStaticMeshActor = Actor->GetClass() == AStaticMeshActor::StaticClass() || Actor->IsA<AInteriorPCGStaticMeshActor>();
	ItemComponent->AssetKind = bPlainStaticMeshActor ? EInteriorPCGAssetKind::StaticMesh : EInteriorPCGAssetKind::ActorClass;
	const FVector ActorExtent = Actor->GetComponentsBoundingBox(true).GetExtent();
	if (!ActorExtent.IsNearlyZero())
	{
		ItemComponent->CollisionHalfExtent = FVector(
			FMath::Max(1.0, ActorExtent.X),
			FMath::Max(1.0, ActorExtent.Y),
			FMath::Max(1.0, ActorExtent.Z));
	}

	FHitResult FloorHit;
	bool bFoundFloor = false;
	if (bGenerateOnAllDetectedFloors)
	{
		ScanFloorLayers();
		float ClosestDistance = TNumericLimits<float>::Max();
		for (int32 FloorIndex = 0; FloorIndex < LastDetectedFloorWorldHeights.Num(); ++FloorIndex)
		{
			FHitResult CandidateHit;
			if (TraceFloorAtWorldXYForLayer(FVector2D(Actor->GetActorLocation().X, Actor->GetActorLocation().Y), LastDetectedFloorWorldHeights[FloorIndex], CandidateHit, Actor))
			{
				const float Distance = FMath::Abs(Actor->GetActorLocation().Z - CandidateHit.ImpactPoint.Z);
				if (Distance < ClosestDistance)
				{
					ClosestDistance = Distance;
					FloorHit = CandidateHit;
					ItemComponent->FloorIndex = FloorIndex;
					bFoundFloor = true;
				}
			}
		}
	}
	else
	{
		ItemComponent->FloorIndex = INDEX_NONE;
		bFoundFloor = TraceFloorAtWorldXY(FVector2D(Actor->GetActorLocation().X, Actor->GetActorLocation().Y), FloorHit, Actor);
	}

	if (bFoundFloor)
	{
		ItemComponent->FloorHeightOffset = Actor->GetActorLocation().Z - FloorHit.ImpactPoint.Z;
	}

	RegisteredActors.AddUnique(Actor);
	Modify();
#if WITH_EDITOR
	Actor->SetFolderPath(FName(*FString::Printf(TEXT("InteriorPCG/%s"), *GetActorLabel())));
#endif
	Actor->MarkPackageDirty();
	return true;
}

int32 AInteriorPCGVolume::CaptureCurrentPlacement(UInteriorPCGPreset* Preset)
{
	if (!Preset)
	{
		return 0;
	}

	FBox LocalBounds;
	if (!GetVolumeLocalBounds(LocalBounds))
	{
		return 0;
	}

	TArray<FInteriorPCGPresetItem> PreservedInactiveItems;
	for (const FInteriorPCGPresetItem& ExistingItem : Preset->Items)
	{
		if (!ExistingItem.bEnabled)
		{
			PreservedInactiveItems.Add(ExistingItem);
		}
	}

	TArray<AActor*> Actors;
	GetRegisteredActors(Actors);
	if (bGenerateOnAllDetectedFloors)
	{
		ScanFloorLayers();
	}
	TSet<FGuid> UsedIds;
	TArray<FInteriorPCGPresetItem> CapturedItems;
	const FTransform VolumeTransform = GetActorTransform();

	for (AActor* Actor : Actors)
	{
		UInteriorPCGItemComponent* ItemComponent = FindItemComponent(Actor);
		if (!ItemComponent)
		{
			continue;
		}

		ItemComponent->Modify();
		if (!ItemComponent->StableId.IsValid() || UsedIds.Contains(ItemComponent->StableId))
		{
			ItemComponent->StableId = FGuid::NewGuid();
		}
		UsedIds.Add(ItemComponent->StableId);

		FInteriorPCGPresetItem Item;
		Item.StableId = ItemComponent->StableId;
		Item.SourceEntryId = ItemComponent->SourceEntryId;
		Item.bEnabled = ItemComponent->bIncludedInPreset;
		Item.Role = ItemComponent->Role;
		Item.FloorIndex = ItemComponent->FloorIndex;
		Item.AssetKind = ItemComponent->AssetKind;
		Item.CollisionHalfExtent = ItemComponent->CollisionHalfExtent;

		if (Item.AssetKind == EInteriorPCGAssetKind::StaticMesh)
		{
			const AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(Actor);
			if (!StaticMeshActor || !StaticMeshActor->GetStaticMeshComponent()->GetStaticMesh())
			{
				continue;
			}
			Item.StaticMesh = StaticMeshActor->GetStaticMeshComponent()->GetStaticMesh();
		}
		else
		{
			Item.ActorClass = Actor->GetClass();
		}

		const FVector LocalPosition = VolumeTransform.InverseTransformPosition(Actor->GetActorLocation());
		Item.NormalizedPosition = FInteriorPCGPlacementMath::NormalizeLocalXY(LocalPosition, LocalBounds);
		Item.RelativeRotation = (GetActorQuat().Inverse() * Actor->GetActorQuat()).Rotator();
		Item.Scale = Actor->GetActorScale3D();

		FHitResult FloorHit;
		bool bFoundFloor = false;
		if (bGenerateOnAllDetectedFloors)
		{
			float ClosestDistance = TNumericLimits<float>::Max();
			for (int32 FloorIndex = 0; FloorIndex < LastDetectedFloorWorldHeights.Num(); ++FloorIndex)
			{
				FHitResult CandidateHit;
				if (TraceFloorAtWorldXYForLayer(FVector2D(Actor->GetActorLocation().X, Actor->GetActorLocation().Y), LastDetectedFloorWorldHeights[FloorIndex], CandidateHit, Actor))
				{
					const float Distance = FMath::Abs(Actor->GetActorLocation().Z - CandidateHit.ImpactPoint.Z);
					if (Distance < ClosestDistance)
					{
						ClosestDistance = Distance;
						FloorHit = CandidateHit;
						Item.FloorIndex = FloorIndex;
						ItemComponent->FloorIndex = FloorIndex;
						bFoundFloor = true;
					}
				}
			}
		}
		else
		{
			bFoundFloor = TraceFloorAtWorldXY(FVector2D(Actor->GetActorLocation().X, Actor->GetActorLocation().Y), FloorHit, Actor);
		}

		if (bFoundFloor)
		{
			Item.FloorHeightOffset = Actor->GetActorLocation().Z - FloorHit.ImpactPoint.Z;
			ItemComponent->FloorHeightOffset = Item.FloorHeightOffset;
		}
		else
		{
			Item.FloorHeightOffset = ItemComponent->FloorHeightOffset;
		}

		CapturedItems.Add(Item);
	}

	for (const FInteriorPCGPresetItem& InactiveItem : PreservedInactiveItems)
	{
		if (!UsedIds.Contains(InactiveItem.StableId))
		{
			CapturedItems.Add(InactiveItem);
			UsedIds.Add(InactiveItem.StableId);
		}
	}

	CapturedItems.Sort([](const FInteriorPCGPresetItem& A, const FInteriorPCGPresetItem& B)
	{
		return A.StableId < B.StableId;
	});

	Preset->Modify();
	Preset->Items = MoveTemp(CapturedItems);
	Preset->SourceVolumeLocalSize = LocalBounds.GetSize();
	Preset->MarkPackageDirty();
	Modify();
	return Preset->Items.Num();
}

void AInteriorPCGVolume::ExecuteGraphGeneration()
{
	if (GraphGenerationMode == EInteriorPCGGraphGenerationMode::SelectedPreset)
	{
		if (SelectedPresets.IsEmpty())
		{
			GenerateFromSelectedPreset();
		}
		else
		{
			GenerateFromSelectedPresets();
		}
	}
	else
	{
		GenerateRandomInterior();
	}
}

void AInteriorPCGVolume::TryAssignDefaultGraph()
{
#if WITH_EDITOR
	if (PCGComponent && !PCGComponent->GetGraph())
	{
		if (UPCGGraph* DefaultGraph = LoadObject<UPCGGraph>(nullptr, TEXT("/Game/InteriorPCG/PCG_InteriorGenerator.PCG_InteriorGenerator")))
		{
			PCGComponent->SetGraphLocal(DefaultGraph);
			PCGComponent->Seed = Seed;
		}
	}
#endif
}
