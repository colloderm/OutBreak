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
	
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	

	APawn* SpawnActor = World->SpawnActor<APawn>
	(
		APawn::StaticClass(),
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
	
	HordeProxy->UpdateInstance(Transforms);
}


void UHordeProxySubsystem::CreateProxyHost()
{
	if (HordeProxy) return;
	
	const TSubclassOf<AHordeProxyActor> HordeProxyClass =
		Settings->GetHordeProxyClass();

	if (!ensureAlwaysMsgf(
		HordeProxyClass,
		TEXT("Flow Field 설정에 HordeProxyActorClass가 지정되지 않았습니다.")))
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


