#include "Game/Expedition/OBLandingZoneScannerComponent.h"

#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "Game/Expedition/OBHelicopterExclusionVolume.h"
#include "Game/Expedition/OBHelicopterInsertionAreaVolume.h"
#include "NavigationSystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogOBLandingZone, Log, All);

UOBLandingZoneScannerComponent::UOBLandingZoneScannerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UOBLandingZoneScannerComponent::FindGroundAtXY(const FVector2D& WorldXY, FVector& OutGroundLocation) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FVector Start(WorldXY.X, WorldXY.Y, TraceHalfHeight);
	const FVector End(WorldXY.X, WorldXY.Y, -TraceHalfHeight);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(OBLandingZoneGround), false, GetOwner());
	if (!World->LineTraceSingleByChannel(Hit, Start, End, GroundTraceChannel, Params) || !Hit.bBlockingHit)
	{
		return false;
	}

	OutGroundLocation = Hit.ImpactPoint;
	return true;
}

bool UOBLandingZoneScannerComponent::FindSafeLandingZone(
	const FVector& RequestedLocation,
	FOBLandingZoneResult& OutResult) const
{
	OutResult = FOBLandingZoneResult();
	FOBLandingZoneResult Best;
	TArray<int32> FailureCounts;
	FailureCounts.SetNumZeroed(static_cast<int32>(EOBLandingZoneFailure::OutsideInsertionArea) + 1);
	int32 CandidatesEvaluated = 0;

	const int32 RingCount = FMath::Max(1, FMath::CeilToInt(SearchRadius / FMath::Max(100.f, RingStep)));
	for (int32 Ring = 0; Ring <= RingCount; ++Ring)
	{
		const int32 Count = Ring == 0 ? 1 : SamplesPerRing;
		const float Radius = Ring * RingStep;
		for (int32 Sample = 0; Sample < Count; ++Sample)
		{
			++CandidatesEvaluated;
			const float Angle = Count == 1 ? 0.f : (2.f * UE_PI * static_cast<float>(Sample) / static_cast<float>(Count));
			const FVector2D CandidateXY(
				RequestedLocation.X + FMath::Cos(Angle) * Radius,
				RequestedLocation.Y + FMath::Sin(Angle) * Radius);

			FOBLandingZoneResult Candidate;
			EOBLandingZoneFailure Failure = EOBLandingZoneFailure::None;
			if (EvaluateCandidate(RequestedLocation, CandidateXY, Candidate, Failure) && Candidate.Score < Best.Score)
			{
				Best = Candidate;
			}
			else if (Failure != EOBLandingZoneFailure::None)
			{
				const int32 FailureIndex = static_cast<int32>(Failure);
				if (FailureCounts.IsValidIndex(FailureIndex))
				{
					++FailureCounts[FailureIndex];
				}
			}
		}
	}

	OutResult = Best;
	OutResult.CandidatesEvaluated = CandidatesEvaluated;
	if (!OutResult.bValid)
	{
		int32 MostFrequentCount = 0;
		for (int32 Index = 1; Index < FailureCounts.Num(); ++Index)
		{
			if (FailureCounts[Index] > MostFrequentCount)
			{
				MostFrequentCount = FailureCounts[Index];
				OutResult.Failure = static_cast<EOBLandingZoneFailure>(Index);
			}
		}
	}

	if (OutResult.bValid)
	{
		UE_LOG(LogOBLandingZone, Log,
			TEXT("[InsertionTarget] Validation accepted Requested=%s Candidates=%d Reason=%s Ground=%s Hover=%s Score=%.1f"),
			*RequestedLocation.ToCompactString(), OutResult.CandidatesEvaluated,
			LexToString(OutResult.Failure), *FVector(OutResult.GroundLocation).ToCompactString(),
			*OutResult.HoverTransform.GetLocation().ToCompactString(), OutResult.Score);
	}
	else
	{
		UE_LOG(LogOBLandingZone, Warning,
			TEXT("[InsertionTarget] Validation failed Requested=%s Candidates=%d Reason=%s Ground=%s Hover=%s Score=%.1f"),
			*RequestedLocation.ToCompactString(), OutResult.CandidatesEvaluated,
			LexToString(OutResult.Failure), *FVector(OutResult.GroundLocation).ToCompactString(),
			*OutResult.HoverTransform.GetLocation().ToCompactString(), OutResult.Score);
	}
	return OutResult.bValid;
}

bool UOBLandingZoneScannerComponent::EvaluateCandidate(
	const FVector& RequestedLocation,
	const FVector2D& CandidateXY,
	FOBLandingZoneResult& OutResult,
	EOBLandingZoneFailure& OutFailure) const
{
	OutFailure = EOBLandingZoneFailure::None;
	UWorld* World = GetWorld();
	if (!World)
	{
		OutFailure = EOBLandingZoneFailure::WorldNotReady;
		return false;
	}

	const FVector Start(CandidateXY.X, CandidateXY.Y, TraceHalfHeight);
	const FVector End(CandidateXY.X, CandidateXY.Y, -TraceHalfHeight);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(OBLandingZoneCandidate), false, GetOwner());
	FHitResult CenterHit;
	if (!World->LineTraceSingleByChannel(CenterHit, Start, End, GroundTraceChannel, Params) || !CenterHit.bBlockingHit)
	{
		OutFailure = EOBLandingZoneFailure::NoGroundHit;
		return false;
	}

	const float MinUpDot = FMath::Cos(FMath::DegreesToRadians(MaxSlopeDegrees));
	if (FVector::DotProduct(CenterHit.ImpactNormal.GetSafeNormal(), FVector::UpVector) < MinUpDot)
	{
		OutFailure = EOBLandingZoneFailure::SlopeTooSteep;
		return false;
	}

	const FVector Ground = CenterHit.ImpactPoint;
	if (bRequireInsertionAreaVolume)
	{
		if (!HasInsertionAreaVolume())
		{
			OutFailure = EOBLandingZoneFailure::MissingInsertionAreaVolume;
			return false;
		}
		if (!IsInsideInsertionArea(Ground))
		{
			OutFailure = EOBLandingZoneFailure::OutsideInsertionArea;
			return false;
		}
	}
	if (IsInsideExclusionVolume(Ground))
	{
		OutFailure = EOBLandingZoneFailure::ExclusionVolume;
		return false;
	}

	float MinZ = Ground.Z;
	float MaxZ = Ground.Z;
	for (int32 Index = 0; Index < 8; ++Index)
	{
		const float Angle = 2.f * UE_PI * static_cast<float>(Index) / 8.f;
		const FVector2D SampleXY(
			CandidateXY.X + FMath::Cos(Angle) * FootprintRadius,
			CandidateXY.Y + FMath::Sin(Angle) * FootprintRadius);
		FVector SampleGround;
		if (!FindGroundAtXY(SampleXY, SampleGround))
		{
			OutFailure = EOBLandingZoneFailure::FootprintInvalid;
			return false;
		}
		MinZ = FMath::Min(MinZ, SampleGround.Z);
		MaxZ = FMath::Max(MaxZ, SampleGround.Z);
	}

	const float HeightVariance = MaxZ - MinZ;
	if (HeightVariance > MaxHeightVariance)
	{
		OutFailure = EOBLandingZoneFailure::HeightVariance;
		return false;
	}

	FVector NavigableGround = Ground;
	if (bRequireNavigation)
	{
		const UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		FNavLocation Projected;
		if (!Navigation)
		{
			OutFailure = EOBLandingZoneFailure::NavigationUnavailable;
			return false;
		}
		if (!Navigation->ProjectPointToNavigation(Ground, Projected, NavigationProjectionExtent))
		{
			OutFailure = EOBLandingZoneFailure::NavigationProjectionFailed;
			return false;
		}
		NavigableGround = Projected.Location;
	}

	const FVector HoverLocation = Ground + FVector(0.f, 0.f, HoverHeight);
	const FCollisionShape HelicopterShape = FCollisionShape::MakeCapsule(
		HelicopterClearanceRadius,
		HelicopterClearanceHalfHeight);
	if (World->OverlapBlockingTestByChannel(
		HoverLocation,
		FQuat::Identity,
		ECC_WorldStatic,
		HelicopterShape,
		Params))
	{
		OutFailure = EOBLandingZoneFailure::HelicopterClearanceBlocked;
		return false;
	}

	const FVector RopeStart = Ground + FVector(0.f, 0.f, 150.f);
	const FVector RopeEnd = HoverLocation - FVector(0.f, 0.f, HelicopterClearanceHalfHeight + 50.f);
	if (World->LineTraceTestByChannel(RopeStart, RopeEnd, ECC_WorldStatic, Params))
	{
		OutFailure = EOBLandingZoneFailure::RappelPathBlocked;
		return false;
	}

	const float DistanceScore = FVector::Dist2D(RequestedLocation, Ground);
	const float SlopeScore = (1.f - FVector::DotProduct(CenterHit.ImpactNormal.GetSafeNormal(), FVector::UpVector)) * 10000.f;
	const float NavOffsetScore = FVector::Dist2D(Ground, NavigableGround);

	OutResult.bValid = true;
	OutResult.GroundLocation = Ground;
	OutResult.HoverTransform = FTransform(FRotator::ZeroRotator, HoverLocation);
	OutResult.Score = DistanceScore + SlopeScore + HeightVariance * 10.f + NavOffsetScore;
	OutResult.Failure = EOBLandingZoneFailure::None;

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugSphere(World, Ground, 100.f, 12, FColor::Green, false, 15.f, 0, 8.f);
		DrawDebugCapsule(World, HoverLocation, HelicopterClearanceHalfHeight, HelicopterClearanceRadius,
			FQuat::Identity, FColor::Cyan, false, 15.f, 0, 5.f);
	}
#endif

	return true;
}

bool UOBLandingZoneScannerComponent::IsInsideExclusionVolume(const FVector& Location) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return true;
	}

	for (TActorIterator<AOBHelicopterExclusionVolume> It(World); It; ++It)
	{
		const AOBHelicopterExclusionVolume* Volume = *It;
		if (Volume && Volume->bBlockInsertion && Volume->EncompassesPoint(Location))
		{
			return true;
		}
	}
	return false;
}

bool UOBLandingZoneScannerComponent::HasInsertionAreaVolume() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	for (TActorIterator<AOBHelicopterInsertionAreaVolume> It(World); It; ++It)
	{
		if (It->bAllowInsertion)
		{
			return true;
		}
	}
	return false;
}

bool UOBLandingZoneScannerComponent::IsInsideInsertionArea(const FVector& WorldLocation) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	for (TActorIterator<AOBHelicopterInsertionAreaVolume> It(World); It; ++It)
	{
		const AOBHelicopterInsertionAreaVolume* Volume = *It;
		if (Volume && Volume->bAllowInsertion && Volume->EncompassesPoint(WorldLocation))
		{
			return true;
		}
	}
	return false;
}
