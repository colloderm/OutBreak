#include "Game/Expedition/OBExtractionSite.h"

#include "Components/SceneComponent.h"

AOBExtractionSite::AOBExtractionSite()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	// GameMode collects sites at session start, independent of World Partition distance.
	bIsSpatiallyLoaded = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	LandingAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("LandingAnchor"));
	LandingAnchor->SetupAttachment(SceneRoot);

	FlareAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("FlareAnchor"));
	FlareAnchor->SetupAttachment(SceneRoot);
	FlareAnchor->SetRelativeLocation(FVector(0.f, 0.f, 100.f));

	Tags.Add(TEXT("PersonalExtract"));
}

FTransform AOBExtractionSite::GetLandingTransform() const
{
	return LandingAnchor ? LandingAnchor->GetComponentTransform() : GetActorTransform();
}

FTransform AOBExtractionSite::GetFlareTransform() const
{
	return FlareAnchor ? FlareAnchor->GetComponentTransform() : GetActorTransform();
}
