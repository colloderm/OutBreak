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
	Payload.Operation = EHordeNetworkOperation::Register;
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

bool UBudgetOverlordSubsystem::UnregisterAgent(
	const int32 PackedIndex)
{
	check(MovementSubsystem);
	check(ProxySubsystem);
	check(StatusSubsystem);
	check(NetworkSubsystem);

	const int32 AgentCount =
		PackedIndexToHandle.Num();

	if (!PackedIndexToHandle.IsValidIndex(PackedIndex))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"%s::%s: Invalid PackedIndex. "
				"PackedIndex=%d AgentCount=%d"),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__),
			PackedIndex,
			AgentCount);

		return false;
	}

	check(MovementSubsystem->MovementStorage.Size()
		== AgentCount);

	/*
	 * Status 및 Proxy Storage도 같은 AgentCount를 가지는지
	 * 각 Storage의 Size()에 맞게 검증하는 것이 좋다.
	 */
	check(StatusSubsystem->StatusStorage.Size()
		== AgentCount);

	const int32 PreviousLastIndex =
		AgentCount - 1;

	const bool bMovesLastAgent =
		PackedIndex != PreviousLastIndex;

	/*
	 * RemoveAtSwap 전에 삭제될 Agent와 이동될 Agent 정보를
	 * 반드시 저장한다.
	 */
	const FHordeAgentHandle RemovedHandle =
		PackedIndexToHandle[PackedIndex];

	FHordeAgentHandle MovedHandle;

	if (bMovesLastAgent)
	{
		MovedHandle =
			PackedIndexToHandle[PreviousLastIndex];
	}

	AActor* RemovedActor =
		ProxySubsystem->GetRegisteredActor(
			PackedIndex);

	AActor* MovedActor = nullptr;

	if (bMovesLastAgent)
	{
		MovedActor =
			ProxySubsystem->GetRegisteredActor(
				PreviousLastIndex);
	}

	/*
	 * 클라이언트에는 삭제되는 시점의 기존 Generation을 가진
	 * Handle을 전송해야 한다.
	 */
	FHordeNetworkFormat Payload;
	Payload.Operation =
		EHordeNetworkOperation::Unregister;
	Payload.Handle =
		RemovedHandle;

	NetworkSubsystem->AddPayload(Payload);

	/*
	 * 모든 Storage는 반드시 같은 PackedIndex를
	 * RemoveAtSwap 해야 한다.
	 *
	 * 각 Unregister 내부에서도
	 * EAllowShrinking::No를 사용해야 한다.
	 */
	MovementSubsystem->Unregister(
		PackedIndex);

	ProxySubsystem->Unregister(
		PackedIndex);

	StatusSubsystem->Unregister(
		PackedIndex);

	/*
	 * PackedIndexToHandle 역시 동일한 Swap Remove를 수행한다.
	 *
	 * Num은 감소하지만 Max는 유지되므로 메모리는 축소되지 않는다.
	 */
	PackedIndexToHandle.RemoveAtSwap(
		PackedIndex,
		1,
		EAllowShrinking::No);

	/*
	 * 삭제된 Handle은 더 이상 PackedIndex를 가지지 않는다.
	 */
	check(AgentIDToPackedIndex.IsValidIndex(
		static_cast<int32>(RemovedHandle.AgentID)));

	AgentIDToPackedIndex[RemovedHandle.AgentID] =
		INDEX_NONE;

	/*
	 * 마지막 Agent가 삭제 자리로 이동했다면
	 * Handle → PackedIndex 역참조를 수정한다.
	 */
	if (bMovesLastAgent)
	{
		check(AgentIDToPackedIndex.IsValidIndex(
			static_cast<int32>(MovedHandle.AgentID)));

		AgentIDToPackedIndex[MovedHandle.AgentID] =
			PackedIndex;

		check(
			PackedIndexToHandle.IsValidIndex(
				PackedIndex));

		check(
			PackedIndexToHandle[PackedIndex]
			== MovedHandle);
	}

	/*
	 * 삭제된 Actor의 매핑을 제거한다.
	 */
	if (IsValid(RemovedActor))
	{
		IndexByActor.Remove(RemovedActor);
	}

	/*
	 * 마지막 Actor가 삭제 위치로 이동한 경우
	 * Actor → PackedIndex를 수정한다.
	 */
	if (bMovesLastAgent
		&& IsValid(MovedActor))
	{
		IndexByActor.FindOrAdd(MovedActor) =
			PackedIndex;
	}

	/*
	 * ID를 Free List에 반환하고 Generation을 증가시킨다.
	 *
	 * 오래된 네트워크 패킷이 이후 같은 AgentID를 재사용한
	 * 새 Agent에 적용되는 것을 Generation으로 방지한다.
	 */
	ReleaseAgentHandle(RemovedHandle);

	/*
	 * 모든 Storage와 매핑의 논리 크기가 같은지 검증한다.
	 */
	check(
		MovementSubsystem->MovementStorage.Size()
		== PackedIndexToHandle.Num());

	check(
		StatusSubsystem->StatusStorage.Size()
		== PackedIndexToHandle.Num());

	return true;
}

void UBudgetOverlordSubsystem::ReleaseAgentHandle(
	const FHordeAgentHandle Handle)
{
	const int32 AgentID =
		static_cast<int32>(Handle.AgentID);

	if (!AgentGenerations.IsValidIndex(AgentID)
		|| !AgentIDToPackedIndex.IsValidIndex(AgentID))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"%s::%s: Invalid AgentID. "
				"AgentID=%u Generation=%u"),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__),
			Handle.AgentID,
			Handle.Generation);

		return;
	}

	/*
	 * 현재 활성 Handle과 Generation이 같은 경우만 반환한다.
	 */
	if (AgentGenerations[AgentID]
		!= Handle.Generation)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"%s::%s: Generation mismatch. "
				"AgentID=%u HandleGeneration=%u "
				"CurrentGeneration=%u"),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__),
			Handle.AgentID,
			Handle.Generation,
			AgentGenerations[AgentID]);

		return;
	}

	AgentIDToPackedIndex[AgentID] =
		INDEX_NONE;

	/*
	 * 이전 Handle을 무효화한다.
	 */
	++AgentGenerations[AgentID];

	/*
	 * 동일한 AgentID를 다음 등록에서 재사용할 수 있도록
	 * Free List에 반환한다.
	 */
	FreeAgentIDs.Add(
		Handle.AgentID);
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
		Payload.Operation =
			EHordeNetworkOperation::Update;
		
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
