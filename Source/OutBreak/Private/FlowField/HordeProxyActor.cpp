// Fill out your copyright notice in the Description page of Project Settings.


#include "FlowField/HordeProxyActor.h"

#pragma push_macro("UE_EXPERIMENTAL")
#undef UE_EXPERIMENTAL
#define UE_EXPERIMENTAL(Version, Message)

#include "Components/InstancedStaticMeshComponent.h"

#pragma pop_macro("UE_EXPERIMENTAL")

// Sets default values
AHordeProxyActor::AHordeProxyActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	InstancedStaticMesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(FName("StaticMeshComponent"));
}

void AHordeProxyActor::RegisterInstances(
		TArray<int32>& InstanceIds,
		const TArray<FTransform>& Transforms) const
{
	InstanceIds = InstancedStaticMesh->AddInstances(Transforms, true, true);
}

int32 AHordeProxyActor::AddInstance(const FTransform& Transform)
{
	return InstancedStaticMesh->AddInstance(Transform, true);
}


void AHordeProxyActor::RemoveInstance(const int32 InstanceId) const
{
	InstancedStaticMesh->RemoveInstance(InstanceId);
}

void AHordeProxyActor::UpdateInstances(const TArray<FTransform>& Transforms) const
{
	check(Transforms.Num() == InstancedStaticMesh->GetInstanceCount());
	
	InstancedStaticMesh->BatchUpdateInstancesTransforms(
		0, Transforms, true);
}


// Called when the game starts or when spawned
void AHordeProxyActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AHordeProxyActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

