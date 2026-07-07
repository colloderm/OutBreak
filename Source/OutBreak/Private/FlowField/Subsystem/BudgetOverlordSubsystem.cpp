// Fill out your copyright notice in the Description page of Project Settings.


#include "FlowField/Subsystem/BudgetOverlordSubsystem.h"

#include "FlowField/Settings/FlowFieldSettings.h"

// Subsystem
#include "FlowField/Subsystem/HordeMovementSubsystem.h"
#include "FlowField/Subsystem/HordeNetworkSubsystem.h"
#include "FlowField/Subsystem/HordeProxySubsystem.h"
#include "FlowField/Subsystem/HordeStatusSubsystem.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"


void UBudgetOverlordSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	MovementSubsystem = Collection.InitializeDependency<UHordeMovementSubsystem>();
	ProxySubsystem = Collection.InitializeDependency<UHordeProxySubsystem>();
	StatusSubsystem = Collection.InitializeDependency<UHordeStatusSubsystem>();
	NetworkSubsystem = Collection.InitializeDependency<UHordeNetworkSubsystem>();
	
	const UFlowFieldSettings* Settings = GetDefault<UFlowFieldSettings>();
	InitializeViceroy(Settings->GetMaxAgentCount());
}

void UBudgetOverlordSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	
	ProxySubsystem->CreateProxyHost();
	
	TArray<AActor*> PlayerControllers;
	
	UGameplayStatics::GetAllActorsOfClass(this, APlayerController::StaticClass(), PlayerControllers);
	
	for (int i = 0; i < PlayerControllers.Num(); i++)
	{
		APlayerController* PlayerController = Cast<APlayerController>(PlayerControllers[i]);
		if (PlayerController)
		{
			NetworkSubsystem->RegisterConnection(PlayerController);
		}
	}
}

void UBudgetOverlordSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	
	/* Proxy는 Movement Subsystem의 값을 가져가 병렬로 처리하기에 우선 순위에 배치해야함.*/
	MovementSubsystem->ProcessSystem(DeltaTime);
	StatusSubsystem->ProcessSystem(DeltaTime);
	
	/* 유사 Rendering 단계. */
	ProxySubsystem->ProcessSystem(DeltaTime);
	
	/* Network Packet 전송 */
	BuildPacket();
	NetworkSubsystem->ProcessSystem(DeltaTime);
}

TStatId UBudgetOverlordSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBudgetOverlordSubsystem, STATGROUP_Tickables);
}

int32 UBudgetOverlordSubsystem::GetIndexByActor(
	const AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		return INDEX_NONE;
	}

	const int32* FoundIndex = IndexByActor.Find(Actor);

	return FoundIndex
		? *FoundIndex
		: INDEX_NONE;
}

FHordeAgentHandle UBudgetOverlordSubsystem::RegisterAgent(
	const FTransform& InTransform,
	const float InMoveSpeed,
	const float MaxHealth,
	const float HealthPercent)
{
	check(MovementSubsystem);
	check(ProxySubsystem);
	check(StatusSubsystem);
	check(NetworkSubsystem);

	const int32 PackedIndex =
		MovementSubsystem->MovementStorage.Size();

	const FHordeAgentHandle Handle =
		AllocateAgentHandle();

	MovementSubsystem->Register(
		InTransform,
		InMoveSpeed);

	const ProxyRegisterResult ProxyResult =
		ProxySubsystem->Register(InTransform);

	StatusSubsystem->Register(
		MaxHealth,
		HealthPercent);

	// 모든 Storage가 같은 PackedIndex에 추가됐는지 검증
	check(PackedIndexToHandle.Num() == PackedIndex);

	PackedIndexToHandle.Add(Handle);

	check(AgentIDToPackedIndex.IsValidIndex(
		static_cast<int32>(Handle.AgentID)));

	AgentIDToPackedIndex[Handle.AgentID] =
		PackedIndex;

	if (IsValid(ProxyResult.Actor))
	{
		IndexByActor.Add(
			ProxyResult.Actor,
			PackedIndex);
	}

	/*
	 * 등록 직후 클라이언트에 초기 상태 전달.
	 * 단, 클라이언트에서는 이 Handle로 로컬 Agent를 먼저 등록해야 한다.
	 */
	FHordeNetworkFormat Payload;
	Payload.Handle = Handle;
	Payload.Transforms = InTransform;
	Payload.MoveSpeed = InMoveSpeed;
	Payload.Velocities = FVector::ZeroVector;
	Payload.CachedFlowDirections =
		FVector::ZeroVector;
	Payload.MovementStates = 0;
	Payload.TraversalStates = 0;
	Payload.PriorityTiers = 0;
	Payload.PoseIndex = FIntVector2(0,0);
	Payload.InstanceId = ProxyResult.Index;

	NetworkSubsystem->AddPayload(Payload);

	return Handle;
}

void UBudgetOverlordSubsystem::UnregisterAgent(int32 Index)
{
	MovementSubsystem->Unregister(Index);
	ProxySubsystem->Unregister(Index);
	StatusSubsystem->Unregister(Index);
	
	/*
	 * NetworkSubsystem->AgentUnegistered()
	 * Agent가 등록 해제됨을 모든 Client에 전파
	 */
}

void UBudgetOverlordSubsystem::DispatchPayload(const FHordeNetworkFormat& Payload)
{
	const FHordeAgentHandle& Handle = Payload.Handle;
	if (!Handle.IsValid())
	{
		ensureAlwaysMsgf(false, TEXT("Invalid horde network payload handle."));
		return;
	}
	
	const int32 ID = Handle.AgentID;
	HordeMovementStorage& MovementStorage = MovementSubsystem->MovementStorage;

	if (!MovementStorage.Transforms.IsValidIndex(ID)
		|| !MovementStorage.CachedFlowDirections.IsValidIndex(ID)
		|| !MovementStorage.MoveSpeeds.IsValidIndex(ID)
		|| !MovementStorage.Velocities.IsValidIndex(ID)
		|| !MovementStorage.MovementStates.IsValidIndex(ID)
		|| !MovementStorage.TraversalStates.IsValidIndex(ID)
		|| !MovementStorage.PriorityTiers.IsValidIndex(ID))
	{
		ensureAlwaysMsgf(
			false,
			TEXT("Invalid horde network payload index. AgentID=%d Generation=%d TransformNum=%d VelocityNum=%d MoveSpeedNum=%d"),
			ID,
			Handle.Generation,
			MovementStorage.Transforms.Num(),
			MovementStorage.Velocities.Num(),
			MovementStorage.MoveSpeeds.Num());
		return;
	}
	
	MovementStorage.Transforms[ID] = Payload.Transforms;
	MovementStorage.MoveSpeeds[ID] = Payload.MoveSpeed;
	MovementStorage.Velocities[ID] = Payload.Velocities;
	MovementStorage.CachedFlowDirections[ID] = Payload.CachedFlowDirections;
	MovementStorage.MovementStates[ID] = Payload.MovementStates;
	MovementStorage.TraversalStates[ID] = Payload.TraversalStates;
	MovementStorage.PriorityTiers[ID] = Payload.PriorityTiers;
	
}
void UBudgetOverlordSubsystem::BuildPacket()
{
	UWorld* World =
		GetWorld();

	if (!World || World->GetNetMode() == NM_Client)
	{
		CacheTestIndex = 0;
		return;
	}

	const HordeMovementStorage& Storage =
		MovementSubsystem->MovementStorage;

	const int32 AgentCount = Storage.Size();
	constexpr int32 MaxPayloadCount = 8;

	if (AgentCount <= 0)
	{
		CacheTestIndex = 0;
		return;
	}

	// Agent 제거 등으로 기존 인덱스가 범위를 벗어난 경우 보정
	if (!Storage.Transforms.IsValidIndex(CacheTestIndex))
	{
		CacheTestIndex = 0;
	}

	const int32 PayloadCount =
		FMath::Min(MaxPayloadCount, AgentCount);

	for (int32 ProcessedCount = 0;
		 ProcessedCount < PayloadCount;
		 ++ProcessedCount)
	{
		const int32 PackedIndex = CacheTestIndex;

		if (!PackedIndexToHandle.IsValidIndex(PackedIndex)
			|| !Storage.Transforms.IsValidIndex(PackedIndex)
			|| !Storage.CachedFlowDirections.IsValidIndex(PackedIndex)
			|| !Storage.MoveSpeeds.IsValidIndex(PackedIndex)
			|| !Storage.Velocities.IsValidIndex(PackedIndex)
			|| !Storage.MovementStates.IsValidIndex(PackedIndex)
			|| !Storage.TraversalStates.IsValidIndex(PackedIndex)
			|| !Storage.PriorityTiers.IsValidIndex(PackedIndex))
		{
			CacheTestIndex =
				(CacheTestIndex + 1) % AgentCount;

			continue;
		}

		FHordeNetworkFormat Payload;
		Payload.Handle =
			PackedIndexToHandle[PackedIndex];

		Payload.Transforms =
			Storage.Transforms[PackedIndex];

		Payload.CachedFlowDirections =
			Storage.CachedFlowDirections[PackedIndex];

		Payload.MoveSpeed =
			Storage.MoveSpeeds[PackedIndex];

		Payload.Velocities =
			Storage.Velocities[PackedIndex];

		Payload.MovementStates =
			Storage.MovementStates[PackedIndex];

		Payload.TraversalStates =
			Storage.TraversalStates[PackedIndex];

		Payload.PriorityTiers =
			Storage.PriorityTiers[PackedIndex];

		NetworkSubsystem->AddPayload(Payload);

		CacheTestIndex =
			(CacheTestIndex + 1) % AgentCount;
	}
}

FHordeAgentHandle UBudgetOverlordSubsystem::AllocateAgentHandle()
{
	uint32 AgentID = MAX_uint32;
	
	if (!FreeAgentIDs.IsEmpty())
	{
		AgentID = FreeAgentIDs.Pop(EAllowShrinking::No);
	}
	else
	{
		AgentID = static_cast<uint32>(
			AgentGenerations.Add(0));
		
		AgentIDToPackedIndex.Add(INDEX_NONE);
	}
	
	check(AgentGenerations.IsValidIndex(
		static_cast<int32>(AgentID)));
	
	FHordeAgentHandle Handle;
	Handle.AgentID = AgentID;
	Handle.Generation = AgentGenerations[AgentID];
	
	return Handle;
}

void UBudgetOverlordSubsystem::InitializeViceroy(int32 Capacity)
{
	check(MovementSubsystem);
	check(Capacity > 0);
	
	MovementSubsystem->InitializeStorage(Capacity);
	ProxySubsystem->InitializeStorage(Capacity);
	StatusSubsystem->InitializeStorage(Capacity);
}
