// Fill out your copyright notice in the Description page of Project Settings.


#include "FlowField/HordeProxyHost.h"

#pragma push_macro("UE_EXPERIMENTAL")
#undef UE_EXPERIMENTAL
#define UE_EXPERIMENTAL(Version, Message)

#include "Components/InstancedStaticMeshComponent.h"
#include "AnimToTextureInstancePlaybackHelpers.h"

#include "FlowField/Settings/FlowFieldSettings.h"

#pragma pop_macro("UE_EXPERIMENTAL")

// Sets default values
AHordeProxyHost::AHordeProxyHost()
{
	PrimaryActorTick.bCanEverTick = false;
	
	
	InstancedStaticMesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(FName("StaticMeshComponent"));
	check(InstancedStaticMesh);
	
	const UFlowFieldSettings* Settings = GetDefault<UFlowFieldSettings>();
	const int DataCount = Settings->GetAnimToTextureAutoPlayDataCount();
	
	InstancedStaticMesh->SetNumCustomDataFloats(DataCount);

	RootComponent = InstancedStaticMesh;
}

void AHordeProxyHost::RegisterInstances(
		TArray<int32>& InstanceIds,
		const TArray<FTransform>& Transforms) const
{
	InstanceIds = InstancedStaticMesh->AddInstances(Transforms, true, true);
}

int32 AHordeProxyHost::AddInstance(const FTransform& Transform)
{
	if (!ensureAlwaysMsgf(
		IsValid(InstancedStaticMesh),
		TEXT("HordeProxyHost has no valid InstancedStaticMesh component.")))
	{
		return INDEX_NONE;
	}

	return InstancedStaticMesh->AddInstance(Transform, true);
}


void AHordeProxyHost::RemoveInstance(const int32 InstanceId) const
{
	InstancedStaticMesh->RemoveInstance(InstanceId);
}

void AHordeProxyHost::UpdateInstances(
	TArray<int32>& InstanceIds,
	const TArray<FTransform>& Transforms) const
{
	if (!ensureAlwaysMsgf(
		IsValid(InstancedStaticMesh),
		TEXT("HordeProxyHost has no valid InstancedStaticMesh component.")))
	{
		return;
	}
	
	if (Transforms.Num() != InstancedStaticMesh->GetInstanceCount()
		|| InstanceIds.Num() != Transforms.Num())
	{
		InstancedStaticMesh->ClearInstances();
		InstanceIds =
			InstancedStaticMesh->AddInstances(
				Transforms,
				true,
				true);
		return;
	}
	
	InstancedStaticMesh->BatchUpdateInstancesTransforms(
		0, Transforms, true);
}

void AHordeProxyHost::PlayHordeAgentMontage(int32 InstanceID, FAnimToTextureAutoPlayData PlaybackData)
{
	UAnimToTextureInstancePlaybackLibrary::UpdateInstanceAutoPlayData
	(
		InstancedStaticMesh,
		InstanceID,
		PlaybackData,
		true
	);
}

void AHordeProxyHost::PlayHordeAgentsMontage(int32 InstanceID, FAnimToTextureAutoPlayData PlaybackData)
{
	// UAnimToTextureInstancePlaybackLibrary::BatchUpdateInstancesAutoPlayData
	// (
	// 	InstancedStaticMesh,
	// 	InstanceID,
	// 	PlaybackData,
	// 	true
	// );
	UE_LOG(LogTemp, Display, TEXT("%s::%s: Success."), *GetClass()->GetName(), TEXT(__FUNCTION__));
}


// Called when the game starts or when spawned
void AHordeProxyHost::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AHordeProxyHost::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

