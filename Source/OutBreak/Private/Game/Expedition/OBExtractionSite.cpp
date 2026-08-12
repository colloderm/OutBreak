#include "Game/Expedition/OBExtractionSite.h"

#include "Components/SceneComponent.h"

AOBExtractionSite::AOBExtractionSite()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	// GameMode collects sites at session start, independent of World Partition distance.
#if WITH_EDITORONLY_DATA
	// World Partition 저작 정보(에디터 전용). 쿠킹 후에는 값이 이미 굳어 있다.
	// 서버/Shipping 타겟에는 이 멤버가 존재하지 않는다.
	bIsSpatiallyLoaded = false;
#endif

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
