// Fill out your copyright notice in the Description page of Project Settings.


#include "FlowField/Subsystem/HordeProxySubsystem.h"

#include "GeometryTypes.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "FlowField/HordeProxyHost.h"
#include "FlowField/Subsystem/HordeMovementSubsystem.h"
#include "FlowField/Subsystem/FlowFieldSubsystem.h"
#include "FlowField/Settings/FlowFieldSettings.h"
#include "FlowField/HordeProxyHost.h"
#include "FlowField/HordeProxyActor.h"

void UHordeProxySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	
	MovementSubsystem = Collection.InitializeDependency<UHordeMovementSubsystem>();
	
}

void UHordeProxySubsystem::InitializeStorage(int32 Capacity)
{
	ProxyStorage.Initialize(Capacity);
}

ProxyRegisterResult UHordeProxySubsystem::Register(const FTransform& Transform)
{
	check(HordeProxy);
	const int32 InstanceId = HordeProxy->AddInstance(Transform);
	
	UWorld* World = GetWorld();
	
	check(World);
	
	const TSubclassOf<AHordeProxyActor> HordeProxyActorClass =
		Settings->GetHordeProxyActorClass();
	
	if (!ensureAlwaysMsgf(
		HordeProxyActorClass,
		TEXT("Flow Field 설정에 HordeProxyActorClass가 지정되지 않았습니다.")))
	{
		return ProxyRegisterResult();
	}
	
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	

	AHordeProxyActor* SpawnActor = World->SpawnActor<AHordeProxyActor>
	(
		HordeProxyActorClass,
		FTransform::Identity,
		SpawnParameters
	);
	
	// SpawnActor->Capsule->OnComponentBeginOverlap.AddDynamic(this, &UHordeProxySubsystem::HandleCapsuleBeginOverlap);
	// SpawnActor->Capsule->OnComponentEndOverlap.AddDynamic(this, &UHordeProxySubsystem::HandleCapsuleEndOverlap);
	// SpawnActor->Capsule->OnComponentHit.AddDynamic(this, &UHordeProxySubsystem::HandleCapsuleHit);
	int32 Index = ProxyStorage.Add(SpawnActor, InstanceId);
	return ProxyRegisterResult(SpawnActor, Index);
}

void UHordeProxySubsystem::Unregister(int32 Index)
{
	ProxyStorage.RemoveAtSwap(Index);
}


void UHordeProxySubsystem::ProcessSystem(const float DeltaSeconds)
{
	Super::ProcessSystem(DeltaSeconds);
	
	check(MovementSubsystem);
	
	if (!IsValid(HordeProxy))
	{
		CreateProxyHost();
	}
	
	if (!IsValid(HordeProxy))
	{
		return;
	}
	
	const TArray<FTransform>& Transforms = MovementSubsystem->MovementStorage.Transforms;

	const bool bDiagnosticsEnabled =
		IsFlowFieldNetworkDiagnosticsEnabled();

	FVector DebugMovementLocation =
		FVector::ZeroVector;

	FVector DebugInstanceBeforeLocation =
		FVector::ZeroVector;

	FVector DebugInstanceAfterLocation =
		FVector::ZeroVector;

	FVector DebugPawnBeforeLocation =
		FVector::ZeroVector;

	FVector DebugPawnAfterLocation =
		FVector::ZeroVector;

	bool bDebugHasMovementTransform =
		false;

	bool bDebugHasInstanceBefore =
		false;

	bool bDebugHasInstanceAfter =
		false;

	bool bDebugHasPawnBefore =
		false;

	bool bDebugHasPawnAfter =
		false;

	if (bDiagnosticsEnabled)
	{
		bDebugHasMovementTransform =
			Transforms.IsValidIndex(0);

		if (bDebugHasMovementTransform)
		{
			DebugMovementLocation =
				Transforms[0].GetLocation();
		}

		if (IsValid(HordeProxy->InstancedStaticMesh)
			&& HordeProxy->InstancedStaticMesh->GetInstanceCount() > 0)
		{
			FTransform DebugInstanceBefore =
				FTransform::Identity;

			bDebugHasInstanceBefore =
				HordeProxy->InstancedStaticMesh->GetInstanceTransform(
					0,
					DebugInstanceBefore,
					true);

			DebugInstanceBeforeLocation =
				DebugInstanceBefore.GetLocation();
		}

		if (ProxyStorage.PawnProxies.IsValidIndex(0)
			&& IsValid(ProxyStorage.PawnProxies[0]))
		{
			bDebugHasPawnBefore =
				true;

			DebugPawnBeforeLocation =
				ProxyStorage.PawnProxies[0]->GetActorLocation();
		}
	}
	
	HordeProxy->UpdateInstances(Transforms);
	ParallelProxy();

	if (bDiagnosticsEnabled)
	{
		if (IsValid(HordeProxy->InstancedStaticMesh)
			&& HordeProxy->InstancedStaticMesh->GetInstanceCount() > 0)
		{
			FTransform DebugInstanceAfter =
				FTransform::Identity;

			bDebugHasInstanceAfter =
				HordeProxy->InstancedStaticMesh->GetInstanceTransform(
					0,
					DebugInstanceAfter,
					true);

			DebugInstanceAfterLocation =
				DebugInstanceAfter.GetLocation();
		}

		if (ProxyStorage.PawnProxies.IsValidIndex(0)
			&& IsValid(ProxyStorage.PawnProxies[0]))
		{
			bDebugHasPawnAfter =
				true;

			DebugPawnAfterLocation =
				ProxyStorage.PawnProxies[0]->GetActorLocation();
		}

		UWorld* World =
			GetWorld();

		if (World)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT(
					"[HordeProxy] World=%s WorldType=%d NetMode=%d "
					"TransformCount=%d PawnProxyCount=%d Function=%s "
					"HasMove=%d Move=%s HasInstanceBefore=%d InstanceBefore=%s "
					"HasInstanceAfter=%d InstanceAfter=%s "
					"HasPawnBefore=%d PawnBefore=%s HasPawnAfter=%d PawnAfter=%s"),
				*World->GetName(),
				static_cast<int32>(World->WorldType),
				static_cast<int32>(World->GetNetMode()),
				Transforms.Num(),
				ProxyStorage.PawnProxies.Num(),
				TEXT(__FUNCTION__),
				bDebugHasMovementTransform,
				*DebugMovementLocation.ToCompactString(),
				bDebugHasInstanceBefore,
				*DebugInstanceBeforeLocation.ToCompactString(),
				bDebugHasInstanceAfter,
				*DebugInstanceAfterLocation.ToCompactString(),
				bDebugHasPawnBefore,
				*DebugPawnBeforeLocation.ToCompactString(),
				bDebugHasPawnAfter,
				*DebugPawnAfterLocation.ToCompactString());
		}
	}
}


void UHordeProxySubsystem::CreateProxyHost()
{
	if (IsValid(HordeProxy))
	{
		return;
	}
	
	const TSubclassOf<AHordeProxyHost> HordeProxyClass =
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
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	HordeProxy = World->SpawnActor<AHordeProxyHost>
		(
			HordeProxyClass,
			FTransform::Identity,
			SpawnParameters
		); 
	
	ensureAlwaysMsgf(
		IsValid(HordeProxy),
		TEXT("Failed to spawn HordeProxyHost."));
}

void UHordeProxySubsystem::ParallelProxy()
{
	check(MovementSubsystem);

	TArray<FTransform>& Transforms =
		MovementSubsystem->MovementStorage.Transforms;

	const int32 UpdateCount = FMath::Min(
		ProxyStorage.PawnProxies.Num(),
		Transforms.Num());

	for (int32 AgentIndex = 0;
		 AgentIndex < UpdateCount;
		 ++AgentIndex)
	{
		TObjectPtr<AActor> PawnProxy = ProxyStorage.PawnProxies[AgentIndex];

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

void UHordeProxySubsystem::HandleCapsuleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	
}

void UHordeProxySubsystem::HandleCapsuleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
	
}

void UHordeProxySubsystem::HandleCapsuleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
	
}


