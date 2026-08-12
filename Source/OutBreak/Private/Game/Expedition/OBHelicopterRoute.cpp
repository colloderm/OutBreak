#include "Game/Expedition/OBHelicopterRoute.h"

#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"

AOBHelicopterRoute::AOBHelicopterRoute()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	// Route authoring data is required before any player-driven streaming source exists.
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
	
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	FlightPath = CreateDefaultSubobject<USplineComponent>(TEXT("FlightPath"));
	FlightPath->SetupAttachment(SceneRoot);
	FlightPath->SetClosedLoop(bLoop);
}

void AOBHelicopterRoute::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (FlightPath)
	{
		FlightPath->SetClosedLoop(bLoop);
	}
}

FTransform AOBHelicopterRoute::GetRouteTransform(float NormalizedDistance) const
{
	if (!FlightPath)
	{
		return GetActorTransform();
	}

	const float Distance = FMath::Clamp(NormalizedDistance, 0.f, 1.f) * FlightPath->GetSplineLength();
	return FlightPath->GetTransformAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World, true);
}

float AOBHelicopterRoute::GetRouteLength() const
{
	return FlightPath ? FlightPath->GetSplineLength() : 0.f;
}
