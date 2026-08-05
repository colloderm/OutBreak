// Copyright OutBreak. All Rights Reserved.

#include "InteriorPCGGeneratorActor.h"

#include "InteriorPCGDataAssets.h"
#include "InteriorPCGGenerationLibrary.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InteriorPCGGeneratorActor)

AInteriorPCGGeneratorActor::AInteriorPCGGeneratorActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = SceneRoot;
}

void AInteriorPCGGeneratorActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (bGenerateOnConstruction && !bIsGenerating && Profile)
	{
		Generate();
	}
}

void AInteriorPCGGeneratorActor::BeginDestroy()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		ClearGenerated();
	}
	Super::BeginDestroy();
}

bool AInteriorPCGGeneratorActor::Validate(TArray<FString>& OutMessages) const
{
	return UInteriorPCGGenerationLibrary::ValidateProfile(Profile, OutMessages);
}

bool AInteriorPCGGeneratorActor::Generate()
{
	if (bIsGenerating) return false;
	TGuardValue<bool> GeneratingGuard(bIsGenerating, true);
	ClearGenerated();

	FInteriorPCGGenerationOptions ResolvedOptions = GenerationOptions;
	ResolvedOptions.WorldTransform = GetActorTransform();
	if (!UInteriorPCGGenerationLibrary::Generate(Profile, ResolvedOptions, LastResult))
	{
		return false;
	}

	BuildOutputComponents();
	OnGenerated.Broadcast(LastResult);
	return true;
}

void AInteriorPCGGeneratorActor::ClearGenerated()
{
	for (AActor* Actor : GeneratedActors)
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	GeneratedActors.Reset();

	for (UActorComponent* Component : GeneratedComponents)
	{
		if (IsValid(Component))
		{
			Component->DestroyComponent();
		}
	}
	GeneratedComponents.Reset();
	LastResult.Reset();
}

void AInteriorPCGGeneratorActor::BuildOutputComponents()
{
	TMap<TObjectPtr<UStaticMesh>, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> HISMByMesh;
	for (const FInteriorPCGPlacement& Placement : LastResult.Placements)
	{
		if (Placement.ActorClass && bSpawnInteractiveActors)
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.Owner = this;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
#if WITH_EDITOR
			SpawnParameters.ObjectFlags |= RF_Transactional;
#endif
			if (AActor* Spawned = GetWorld()->SpawnActor<AActor>(Placement.ActorClass, Placement.Transform, SpawnParameters))
			{
				Spawned->Tags.AddUnique(TEXT("InteriorPCG.Generated"));
				Spawned->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
				GeneratedActors.Add(Spawned);
			}
		}

		TArray<UStaticMesh*> MeshLayers;
		if (Placement.StaticMesh) MeshLayers.Add(Placement.StaticMesh);
		for (UStaticMesh* AdditionalMesh : Placement.AdditionalStaticMeshes)
		{
			if (AdditionalMesh) MeshLayers.Add(AdditionalMesh);
		}

		for (UStaticMesh* Mesh : MeshLayers)
		{
			if (Placement.bAllowInstancing)
			{
				TObjectPtr<UHierarchicalInstancedStaticMeshComponent>& ComponentPtr = HISMByMesh.FindOrAdd(Mesh);
				UHierarchicalInstancedStaticMeshComponent* Component = ComponentPtr.Get();
				if (!Component)
				{
					const FName ComponentName = MakeUniqueObjectName(this, UHierarchicalInstancedStaticMeshComponent::StaticClass(),
						FName(*FString::Printf(TEXT("HISM_%s"), *Mesh->GetName())));
					Component = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, ComponentName, RF_Transactional);
					ComponentPtr = Component;
					Component->SetupAttachment(SceneRoot);
					Component->SetStaticMesh(Mesh);
					Component->SetCollisionEnabled(bEnableCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
					Component->SetMobility(EComponentMobility::Static);
					Component->RegisterComponent();
					AddInstanceComponent(Component);
					GeneratedComponents.Add(Component);
				}
				Component->AddInstance(Placement.Transform, true);
			}
			else
			{
				const FName ComponentName = MakeUniqueObjectName(this, UStaticMeshComponent::StaticClass(), TEXT("GeneratedMesh"));
				UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this, ComponentName, RF_Transactional);
				Component->SetupAttachment(SceneRoot);
				Component->SetStaticMesh(Mesh);
				Component->SetWorldTransform(Placement.Transform);
				Component->SetCollisionEnabled(bEnableCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
				Component->SetMobility(EComponentMobility::Static);
				Component->RegisterComponent();
				AddInstanceComponent(Component);
				GeneratedComponents.Add(Component);
			}
		}
	}
}
