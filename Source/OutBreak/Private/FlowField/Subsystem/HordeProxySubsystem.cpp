// Fill out your copyright notice in the Description page of Project Settings.


#include "FlowField/Subsystem/HordeProxySubsystem.h"

#include "GeometryTypes.h"
#include "FlowField/HordeProxyActor.h"
#include "FlowField/Subsystem/HordeMovementSubsystem.h"
#include "FlowField/Settings/FlowFieldSettings.h"
#include "FlowField/HordeProxyActor.h"

void UHordeProxySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	
	MovementSubsystem = Collection.InitializeDependency<UHordeMovementSubsystem>();
	
}

void UHordeProxySubsystem::InitializeStorage(int32 Capacity)
{
	ProxyEntity.Initialize(Capacity);
}

void UHordeProxySubsystem::Register(FTransform&	Transform)
{
	check(HordeProxy);
	const int32 InstanceId = HordeProxy->AddInstance(Transform);
	
	UWorld* World = GetWorld();
	
	check(World);
	
	const TSubclassOf<AActor> HordeProxyActorClass =
		Settings->GetHordeProxyActorClass();
	
	if (!ensureAlwaysMsgf(
		HordeProxyActorClass,
		TEXT("Flow Field 설정에 HordeProxyActorClass가 지정되지 않았습니다.")))
	{
		return;
	}
	
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	

	AActor* SpawnActor = World->SpawnActor<AActor>
	(
		HordeProxyActorClass,
		FTransform::Identity,
		SpawnParameters
	);
	
	ProxyEntity.Add(SpawnActor, InstanceId);
}


void UHordeProxySubsystem::ProcessSystem(const float DeltaSeconds)
{
	Super::ProcessSystem(DeltaSeconds);
	
	check(MovementSubsystem);
	
	const TArray<FTransform>& Transforms = MovementSubsystem->MovementStorage.Transforms;
	
	HordeProxy->UpdateInstances(Transforms);
	ParallelProxy();
}


void UHordeProxySubsystem::CreateProxyHost()
{
	if (HordeProxy) return;
	
	const TSubclassOf<AHordeProxyActor> HordeProxyClass =
		Settings->GetHordeProxyHostClass();

	if (!ensureAlwaysMsgf(
		HordeProxyClass,
		TEXT("Flow Field 설정에 HordeProxyHostClass가 지정되지 않았습니다.")))
	{
		return;
	}
	
	UWorld* World = GetWorld();
	check(World);
	
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = TEXT("HordeProxyHost");
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	HordeProxy = World->SpawnActor<AHordeProxyActor>
		(
			HordeProxyClass,
			FTransform::Identity,
			SpawnParameters
		); 
	
	check(HordeProxy);
}

void UHordeProxySubsystem::ParallelProxy()
{
	check(MovementSubsystem);

	TArray<FTransform>& Transforms =
		MovementSubsystem->MovementStorage.Transforms;

	const int32 UpdateCount = FMath::Min(
		ProxyEntity.PawnProxies.Num(),
		Transforms.Num());

	for (int32 AgentIndex = 0;
		 AgentIndex < UpdateCount;
		 ++AgentIndex)
	{
		TObjectPtr<AActor> PawnProxy = ProxyEntity.PawnProxies[AgentIndex];

		if (!IsValid(PawnProxy))
		{
			continue;
		}
		
		PawnProxy->SetActorTransform(
			Transforms[AgentIndex],
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}
}


