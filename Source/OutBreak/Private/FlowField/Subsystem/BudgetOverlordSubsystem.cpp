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

DEFINE_LOG_CATEGORY(LogHordeLifecycle);


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

bool UBudgetOverlordSubsystem::TryGetHandleByActor(
	const AActor* Actor,
	FHordeAgentHandle& OutHandle) const
{
	OutHandle = FHordeAgentHandle();

	const int32 PackedIndex =
		GetIndexByActor(Actor);

	if (!PackedIndexToHandle.IsValidIndex(PackedIndex))
	{
		return false;
	}

	OutHandle = PackedIndexToHandle[PackedIndex];
	return OutHandle.IsValid();
}

FHordeAgentHandle UBudgetOverlordSubsystem::GetHandleByPackedIndex(
	const int32 PackedIndex) const
{
	return PackedIndexToHandle.IsValidIndex(PackedIndex)
		? PackedIndexToHandle[PackedIndex]
		: FHordeAgentHandle();
}

bool UBudgetOverlordSubsystem::TryResolvePackedIndex(
	const FHordeAgentHandle& Handle,
	int32& OutPackedIndex) const
{
	OutPackedIndex = INDEX_NONE;

	if (!Handle.IsValid()
		|| Handle.AgentID > static_cast<uint32>(MAX_int32))
	{
		return false;
	}

	const int32 AgentID =
		static_cast<int32>(Handle.AgentID);

	if (!AgentGenerations.IsValidIndex(AgentID)
		|| !AgentIDToPackedIndex.IsValidIndex(AgentID)
		|| AgentGenerations[AgentID] != Handle.Generation)
	{
		return false;
	}

	const int32 PackedIndex =
		AgentIDToPackedIndex[AgentID];

	if (PackedIndex == INDEX_NONE
		|| !PackedIndexToHandle.IsValidIndex(PackedIndex)
		|| !(PackedIndexToHandle[PackedIndex] == Handle))
	{
		return false;
	}

	OutPackedIndex = PackedIndex;
	return true;
}

FHordeAgentHandle UBudgetOverlordSubsystem::RegisterAgent(
	const FTransform& InTransform,
	const float InMoveSpeed,
	const float MaxHealth,
	const float HealthPercent)
{
	check(IsInGameThread());
	check(MovementSubsystem);
	check(ProxySubsystem);
	check(StatusSubsystem);
	check(NetworkSubsystem);

	if (MaxHealth <= 0.0f
		|| !FMath::IsFinite(MaxHealth)
		|| !FMath::IsFinite(HealthPercent))
	{
		UE_LOG(
			LogHordeLifecycle,
			Warning,
			TEXT("Invalid Horde register health. MaxHealth=%f HealthPercent=%f"),
			MaxHealth,
			HealthPercent);

		return FHordeAgentHandle();
	}

#if DO_CHECK
	ValidateAgentRegistry();
#endif

	const int32 PackedIndex =
		MovementSubsystem->MovementStorage.Size();

	const FHordeAgentHandle Handle =
		AllocateAgentHandle();

	const int32 MovementIndex =
		MovementSubsystem->Register(
		InTransform,
		InMoveSpeed);

	const ProxyRegisterResult ProxyResult =
		ProxySubsystem->Register(InTransform);

	if (MovementIndex != PackedIndex
		|| !ProxyResult.bSucceeded
		|| ProxyResult.ProxyStorageIndex != PackedIndex)
	{
		if (ProxyResult.bSucceeded)
		{
			ProxySubsystem->Unregister(
				ProxyResult.ProxyStorageIndex);
		}

		if (MovementSubsystem->MovementStorage.Transforms.IsValidIndex(
			MovementIndex))
		{
			MovementSubsystem->Unregister(
				MovementIndex);
		}

		ProxySubsystem->RefreshInstancesFromMovement();
		RollbackAgentHandleAllocation(Handle);

		UE_LOG(
			LogHordeLifecycle,
			Warning,
			TEXT("Failed to register Horde proxy. PackedIndex=%d MovementIndex=%d ProxyIndex=%d"),
			PackedIndex,
			MovementIndex,
			ProxyResult.ProxyStorageIndex);

		return FHordeAgentHandle();
	}

	const int32 StatusIndex =
		StatusSubsystem->Register(
		MaxHealth,
		HealthPercent);

	if (StatusIndex != PackedIndex)
	{
		if (StatusSubsystem->StatusStorage.MaxHealths.IsValidIndex(
			StatusIndex))
		{
			StatusSubsystem->Unregister(StatusIndex);
		}

		MovementSubsystem->Unregister(MovementIndex);
		ProxySubsystem->Unregister(ProxyResult.ProxyStorageIndex);
		ProxySubsystem->RefreshInstancesFromMovement();
		RollbackAgentHandleAllocation(Handle);

		UE_LOG(
			LogHordeLifecycle,
			Error,
			TEXT("Horde register storage index mismatch. Movement=%d Proxy=%d Status=%d Expected=%d"),
			MovementIndex,
			ProxyResult.ProxyStorageIndex,
			StatusIndex,
			PackedIndex);

		return FHordeAgentHandle();
	}

	// 모든 Storage가 같은 PackedIndex에 추가됐는지 검증
	check(PackedIndexToHandle.Num() == PackedIndex);

	PackedIndexToHandle.Add(Handle);

	check(AgentIDToPackedIndex.IsValidIndex(
		static_cast<int32>(Handle.AgentID)));

	AgentIDToPackedIndex[
		static_cast<int32>(Handle.AgentID)] =
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
	Payload.MaxHealth = MaxHealth;
	Payload.CurrentHealth =
		MaxHealth * FMath::Clamp(HealthPercent, 0.0f, 1.0f);
	Payload.Velocities = FVector::ZeroVector;
	Payload.CachedFlowDirections =
		FVector::ZeroVector;
	Payload.MovementStates = 0;
	Payload.TraversalStates = 0;
	Payload.PriorityTiers = 0;
	Payload.PoseIndex = FIntVector2(0,0);

	if (UWorld* World = GetWorld();
		World && World->GetNetMode() != NM_Client)
	{
		NetworkSubsystem->AddPayload(Payload);
	}

#if DO_CHECK
	ValidateAgentRegistry();
#endif

	return Handle;
}

bool UBudgetOverlordSubsystem::UnregisterAgent(
	const FHordeAgentHandle& Handle)
{
	int32 PackedIndex = INDEX_NONE;
	if (!TryResolvePackedIndex(Handle, PackedIndex))
	{
		return false;
	}

	return UnregisterAgentByPackedIndex(
		PackedIndex,
		true);
}

bool UBudgetOverlordSubsystem::UnregisterAgent(
	const AActor* Actor)
{
	FHordeAgentHandle Handle;
	if (!TryGetHandleByActor(Actor, Handle))
	{
		return false;
	}

	return UnregisterAgent(Handle);
}

bool UBudgetOverlordSubsystem::UnregisterAgentByPackedIndex(
	const int32 PackedIndex,
	const bool bQueueNetworkPayload)
{
	check(IsInGameThread());
	check(MovementSubsystem);
	check(ProxySubsystem);
	check(StatusSubsystem);
	check(NetworkSubsystem);

#if DO_CHECK
	ValidateAgentRegistry();
#endif

	const int32 AgentCount =
		PackedIndexToHandle.Num();

	if (!PackedIndexToHandle.IsValidIndex(PackedIndex))
	{
		UE_LOG(
			LogHordeLifecycle,
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

	HordeRemoveResult RemoveResult;
	RemoveResult.RemovedPackedIndex =
		PackedIndex;
	RemoveResult.PreviousLastIndex =
		AgentCount - 1;
	RemoveResult.bMovedLastAgent =
		PackedIndex != RemoveResult.PreviousLastIndex;
	RemoveResult.RemovedHandle =
		PackedIndexToHandle[PackedIndex];
	RemoveResult.RemovedActor =
		ProxySubsystem->GetRegisteredActor(PackedIndex);
	RemoveResult.RemovedInstanceIndex =
		ProxySubsystem->GetInstanceIndex(PackedIndex);

	if (RemoveResult.bMovedLastAgent)
	{
		RemoveResult.MovedHandle =
			PackedIndexToHandle[RemoveResult.PreviousLastIndex];
		RemoveResult.MovedActor =
			ProxySubsystem->GetRegisteredActor(
				RemoveResult.PreviousLastIndex);
		RemoveResult.MovedInstanceIndex =
			ProxySubsystem->GetInstanceIndex(
				RemoveResult.PreviousLastIndex);
	}

	if (bQueueNetworkPayload)
	{
		FHordeNetworkFormat Payload;
		Payload.Operation =
			EHordeNetworkOperation::Unregister;
		Payload.Handle =
			RemoveResult.RemovedHandle;

		NetworkSubsystem->AddPayload(Payload);
	}

	IndexByActor.Remove(RemoveResult.RemovedActor);

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

	ProxySubsystem->RefreshInstancesFromMovement();

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
		static_cast<int32>(RemoveResult.RemovedHandle.AgentID)));

	AgentIDToPackedIndex[
		static_cast<int32>(RemoveResult.RemovedHandle.AgentID)] =
		INDEX_NONE;

	/*
	 * 마지막 Agent가 삭제 자리로 이동했다면
	 * Handle → PackedIndex 역참조를 수정한다.
	 */
	if (RemoveResult.bMovedLastAgent)
	{
		check(AgentIDToPackedIndex.IsValidIndex(
			static_cast<int32>(RemoveResult.MovedHandle.AgentID)));

		AgentIDToPackedIndex[
			static_cast<int32>(RemoveResult.MovedHandle.AgentID)] =
			PackedIndex;

		check(
			PackedIndexToHandle.IsValidIndex(
			PackedIndex));

		check(
			PackedIndexToHandle[PackedIndex]
			== RemoveResult.MovedHandle);
	}

	/*
	 * 마지막 Actor가 삭제 위치로 이동한 경우
	 * Actor → PackedIndex를 수정한다.
	 */
	if (RemoveResult.bMovedLastAgent
		&& RemoveResult.MovedActor.IsValid())
	{
		IndexByActor.FindOrAdd(RemoveResult.MovedActor) =
			PackedIndex;
	}

	/*
	 * ID를 Free List에 반환하고 Generation을 증가시킨다.
	 *
	 * 오래된 네트워크 패킷이 이후 같은 AgentID를 재사용한
	 * 새 Agent에 적용되는 것을 Generation으로 방지한다.
	 */
	ReleaseAgentHandle(RemoveResult.RemovedHandle);

	/*
	 * 모든 Storage와 매핑의 논리 크기가 같은지 검증한다.
	 */
	check(
		MovementSubsystem->MovementStorage.Size()
		== PackedIndexToHandle.Num());

	check(
		StatusSubsystem->StatusStorage.Size()
		== PackedIndexToHandle.Num());

	check(
		ProxySubsystem->ProxyStorage.Size()
		== PackedIndexToHandle.Num());

#if DO_CHECK
	ValidateAgentRegistry();
#endif

	UE_LOG(
		LogHordeLifecycle,
		Verbose,
		TEXT("Removed Horde agent. RemovedPackedIndex=%d PreviousLastIndex=%d RemovedAgentID=%u RemovedGeneration=%u MovedAgentID=%u MovedGeneration=%u RemovedInstance=%d MovedInstance=%d AgentCountAfter=%d FreeList=%d"),
		RemoveResult.RemovedPackedIndex,
		RemoveResult.PreviousLastIndex,
		RemoveResult.RemovedHandle.AgentID,
		RemoveResult.RemovedHandle.Generation,
		RemoveResult.MovedHandle.AgentID,
		RemoveResult.MovedHandle.Generation,
		RemoveResult.RemovedInstanceIndex,
		RemoveResult.MovedInstanceIndex,
		PackedIndexToHandle.Num(),
		FreeAgentIDs.Num());

	return true;
}

void UBudgetOverlordSubsystem::ReleaseAgentHandle(
	const FHordeAgentHandle& Handle)
{
	const int32 AgentID =
		static_cast<int32>(Handle.AgentID);

	if (!AgentGenerations.IsValidIndex(AgentID)
		|| !AgentIDToPackedIndex.IsValidIndex(AgentID))
	{
		UE_LOG(
			LogHordeLifecycle,
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
			LogHordeLifecycle,
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

	if (AgentIDToPackedIndex[AgentID] != INDEX_NONE)
	{
		UE_LOG(
			LogHordeLifecycle,
			Error,
			TEXT(
				"%s::%s: Active AgentID cannot be released. "
				"AgentID=%u PackedIndex=%d"),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__),
			Handle.AgentID,
			AgentIDToPackedIndex[AgentID]);

		return;
	}

	if (FreeAgentIDs.Contains(Handle.AgentID))
	{
		UE_LOG(
			LogHordeLifecycle,
			Error,
			TEXT(
				"%s::%s: Duplicate free AgentID. "
				"AgentID=%u"),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__),
			Handle.AgentID);

		return;
	}

	ensureAlwaysMsgf(
		AgentGenerations[AgentID] != MAX_uint32,
		TEXT("Horde agent generation overflow. AgentID=%u"),
		Handle.AgentID);

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

void UBudgetOverlordSubsystem::RollbackAgentHandleAllocation(
	const FHordeAgentHandle& Handle)
{
	if (!Handle.IsValid()
		|| Handle.AgentID > static_cast<uint32>(MAX_int32))
	{
		return;
	}

	const int32 AgentID =
		static_cast<int32>(Handle.AgentID);

	if (!AgentGenerations.IsValidIndex(AgentID)
		|| !AgentIDToPackedIndex.IsValidIndex(AgentID)
		|| AgentGenerations[AgentID] != Handle.Generation
		|| AgentIDToPackedIndex[AgentID] != INDEX_NONE
		|| FreeAgentIDs.Contains(Handle.AgentID))
	{
		return;
	}

	FreeAgentIDs.Add(Handle.AgentID);
}

void UBudgetOverlordSubsystem::DispatchPayload(const FHordeNetworkFormat& Payload)
{
	switch (Payload.Operation)
	{
	case EHordeNetworkOperation::Register:
		ApplyRegisterPayload(Payload);
		return;

	case EHordeNetworkOperation::Update:
		ApplyUpdatePayload(Payload);
		return;

	case EHordeNetworkOperation::Unregister:
		ApplyUnregisterPayload(Payload);
		return;

	default:
		ensureAlwaysMsgf(
			false,
			TEXT("Unknown Horde network operation."));
		return;
	}
}

bool UBudgetOverlordSubsystem::RegisterAgentWithHandle(
	const FHordeNetworkFormat& Payload)
{
	check(IsInGameThread());
	check(MovementSubsystem);
	check(ProxySubsystem);
	check(StatusSubsystem);

	const FHordeAgentHandle& Handle =
		Payload.Handle;

	if (!Handle.IsValid()
		|| Handle.AgentID > static_cast<uint32>(MAX_int32))
	{
		return false;
	}

	int32 ExistingPackedIndex = INDEX_NONE;
	if (TryResolvePackedIndex(Handle, ExistingPackedIndex))
	{
		ApplyUpdatePayload(Payload);
		return true;
	}

	const int32 AgentID =
		static_cast<int32>(Handle.AgentID);

	while (AgentGenerations.Num() <= AgentID)
	{
		AgentGenerations.Add(0);
		AgentIDToPackedIndex.Add(INDEX_NONE);
	}

	if (AgentGenerations[AgentID] > Handle.Generation)
	{
		return false;
	}

	if (AgentIDToPackedIndex[AgentID] != INDEX_NONE)
	{
		return false;
	}

	AgentGenerations[AgentID] =
		Handle.Generation;
	FreeAgentIDs.Remove(Handle.AgentID);

	const int32 PackedIndex =
		MovementSubsystem->MovementStorage.Size();

	const int32 MovementIndex =
		MovementSubsystem->Register(
			Payload.Transforms,
			Payload.MoveSpeed);

	const ProxyRegisterResult ProxyResult =
		ProxySubsystem->Register(Payload.Transforms);

	if (MovementIndex != PackedIndex
		|| !ProxyResult.bSucceeded
		|| ProxyResult.ProxyStorageIndex != PackedIndex)
	{
		if (ProxyResult.bSucceeded)
		{
			ProxySubsystem->Unregister(
				ProxyResult.ProxyStorageIndex);
		}

		if (MovementSubsystem->MovementStorage.Transforms.IsValidIndex(
			MovementIndex))
		{
			MovementSubsystem->Unregister(MovementIndex);
		}

		ProxySubsystem->RefreshInstancesFromMovement();
		AgentIDToPackedIndex[AgentID] =
			INDEX_NONE;
		return false;
	}

	const float PayloadMaxHealth =
		Payload.MaxHealth > 0.0f
			? Payload.MaxHealth
			: 100.0f;

	const float HealthPercent =
		PayloadMaxHealth > 0.0f
			? FMath::Clamp(
				Payload.CurrentHealth / PayloadMaxHealth,
				0.0f,
				1.0f)
			: 1.0f;

	const int32 StatusIndex =
		StatusSubsystem->Register(
			PayloadMaxHealth,
			HealthPercent);

	if (StatusIndex != PackedIndex)
	{
		if (StatusSubsystem->StatusStorage.MaxHealths.IsValidIndex(
			StatusIndex))
		{
			StatusSubsystem->Unregister(StatusIndex);
		}

		MovementSubsystem->Unregister(MovementIndex);
		ProxySubsystem->Unregister(ProxyResult.ProxyStorageIndex);
		ProxySubsystem->RefreshInstancesFromMovement();
		AgentIDToPackedIndex[AgentID] =
			INDEX_NONE;
		return false;
	}

	PackedIndexToHandle.Add(Handle);
	AgentIDToPackedIndex[AgentID] =
		PackedIndex;

	if (IsValid(ProxyResult.Actor))
	{
		IndexByActor.Add(
			ProxyResult.Actor,
			PackedIndex);
	}

	ApplyUpdatePayload(Payload);

#if DO_CHECK
	ValidateAgentRegistry();
#endif

	return true;
}

void UBudgetOverlordSubsystem::ApplyRegisterPayload(
	const FHordeNetworkFormat& Payload)
{
	if (!RegisterAgentWithHandle(Payload))
	{
		UE_LOG(
			LogHordeLifecycle,
			Verbose,
			TEXT("Ignored Horde register payload. AgentID=%u Generation=%u"),
			Payload.Handle.AgentID,
			Payload.Handle.Generation);
	}
}

void UBudgetOverlordSubsystem::ApplyUpdatePayload(
	const FHordeNetworkFormat& Payload)
{
	int32 PackedIndex = INDEX_NONE;
	if (!TryResolvePackedIndex(
		Payload.Handle,
		PackedIndex))
	{
		return;
	}

	HordeMovementStorage& MovementStorage =
		MovementSubsystem->MovementStorage;

	if (!MovementStorage.Transforms.IsValidIndex(PackedIndex)
		|| !MovementStorage.CachedFlowDirections.IsValidIndex(PackedIndex)
		|| !MovementStorage.MoveSpeeds.IsValidIndex(PackedIndex)
		|| !MovementStorage.Velocities.IsValidIndex(PackedIndex)
		|| !MovementStorage.MovementStates.IsValidIndex(PackedIndex)
		|| !MovementStorage.TraversalStates.IsValidIndex(PackedIndex)
		|| !MovementStorage.PriorityTiers.IsValidIndex(PackedIndex))
	{
		ensureAlwaysMsgf(
			false,
			TEXT("Invalid Horde update payload local index. PackedIndex=%d AgentID=%u Generation=%u"),
			PackedIndex,
			Payload.Handle.AgentID,
			Payload.Handle.Generation);
		return;
	}

	MovementStorage.Transforms[PackedIndex] =
		Payload.Transforms;
	MovementStorage.MoveSpeeds[PackedIndex] =
		Payload.MoveSpeed;
	MovementStorage.Velocities[PackedIndex] =
		Payload.Velocities;
	MovementStorage.CachedFlowDirections[PackedIndex] =
		Payload.CachedFlowDirections;
	MovementStorage.MovementStates[PackedIndex] =
		Payload.MovementStates;
	MovementStorage.TraversalStates[PackedIndex] =
		Payload.TraversalStates;
	MovementStorage.PriorityTiers[PackedIndex] =
		Payload.PriorityTiers;
}

void UBudgetOverlordSubsystem::ApplyUnregisterPayload(
	const FHordeNetworkFormat& Payload)
{
	int32 PackedIndex = INDEX_NONE;
	if (!TryResolvePackedIndex(
		Payload.Handle,
		PackedIndex))
	{
		return;
	}

	UnregisterAgentByPackedIndex(
		PackedIndex,
		false);
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

#if DO_CHECK
	ValidateAgentRegistry();
#endif

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

		Payload.MaxHealth =
			StatusSubsystem->StatusStorage.MaxHealths.IsValidIndex(
				PackedIndex)
				? StatusSubsystem->StatusStorage.MaxHealths[PackedIndex]
				: 0.0f;

		Payload.CurrentHealth =
			StatusSubsystem->StatusStorage.CurrentHealths.IsValidIndex(
				PackedIndex)
				? StatusSubsystem->StatusStorage.CurrentHealths[PackedIndex]
				: 0.0f;

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

void UBudgetOverlordSubsystem::ValidateAgentRegistry() const
{
#if DO_CHECK
	check(MovementSubsystem);
	check(ProxySubsystem);
	check(StatusSubsystem);

	const HordeMovementStorage& MovementStorage =
		MovementSubsystem->MovementStorage;
	const HordeProxyStorage& ProxyStorage =
		ProxySubsystem->ProxyStorage;
	const HordeStatusStorage& StatusStorage =
		StatusSubsystem->StatusStorage;

	check(MovementStorage.IsValid());
	check(ProxyStorage.IsValid());
	check(StatusStorage.IsValid());

	const int32 AgentCount =
		PackedIndexToHandle.Num();

	check(MovementStorage.Size() == AgentCount);
	check(ProxyStorage.Size() == AgentCount);
	check(StatusStorage.Size() == AgentCount);

	TSet<uint32> ActiveAgentIDs;
	ActiveAgentIDs.Reserve(AgentCount);

	for (int32 PackedIndex = 0;
		 PackedIndex < AgentCount;
		 ++PackedIndex)
	{
		const FHordeAgentHandle& Handle =
			PackedIndexToHandle[PackedIndex];

		check(Handle.IsValid());
		check(Handle.AgentID <= static_cast<uint32>(MAX_int32));

		const int32 AgentID =
			static_cast<int32>(Handle.AgentID);

		check(AgentGenerations.IsValidIndex(AgentID));
		check(AgentIDToPackedIndex.IsValidIndex(AgentID));
		check(AgentGenerations[AgentID] == Handle.Generation);
		check(AgentIDToPackedIndex[AgentID] == PackedIndex);
		check(!ActiveAgentIDs.Contains(Handle.AgentID));

		ActiveAgentIDs.Add(Handle.AgentID);

		if (AActor* ProxyActor =
			ProxySubsystem->GetRegisteredActor(PackedIndex);
			IsValid(ProxyActor))
		{
			const int32* FoundPackedIndex =
				IndexByActor.Find(ProxyActor);

			check(FoundPackedIndex);
			check(*FoundPackedIndex == PackedIndex);
		}
	}

	TSet<uint32> FreeAgentIDSet;
	FreeAgentIDSet.Reserve(FreeAgentIDs.Num());

	for (const uint32 FreeAgentID : FreeAgentIDs)
	{
		check(FreeAgentID <= static_cast<uint32>(MAX_int32));
		check(!FreeAgentIDSet.Contains(FreeAgentID));
		check(!ActiveAgentIDs.Contains(FreeAgentID));

		FreeAgentIDSet.Add(FreeAgentID);

		const int32 AgentID =
			static_cast<int32>(FreeAgentID);

		if (AgentIDToPackedIndex.IsValidIndex(AgentID))
		{
			check(AgentIDToPackedIndex[AgentID] == INDEX_NONE);
		}
	}

	for (const TPair<TWeakObjectPtr<AActor>, int32>& Pair
		: IndexByActor)
	{
		if (!Pair.Key.IsValid())
		{
			continue;
		}

		const int32 PackedIndex =
			Pair.Value;

		check(PackedIndexToHandle.IsValidIndex(PackedIndex));
		check(ProxySubsystem->GetRegisteredActor(PackedIndex)
			== Pair.Key.Get());
	}
#endif
}
