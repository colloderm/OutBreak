# HordeAgentHandle Usage

## 요약

현재 코드에서 `FHordeAgentHandle`은 **부분적으로만 사용 중**이다.

- 사용 중: `FHordeNetworkFormat::Handle`
- 사용 중: `UBudgetOverlordSubsystem::DispatchPayload()`에서 `Handle.IsValid()` 확인 후 `Handle.AgentID`를 storage index처럼 사용
- 미사용: `Generation` 검증
- 미사용: stable `AgentID -> PackedIndex` registry
- 미사용: `HordeRemoveResult`

즉, 지금 시스템에서 `FHordeAgentHandle`은 아직 완성된 stable handle이 아니라 **네트워크 payload에 들어가는 식별자 껍데기**에 가깝다.
현재 `DispatchPayload()`는 `Handle.AgentID`를 `MovementStorage` 배열 index로 직접 사용한다.

이 방식은 agent가 한 번도 제거되지 않고, 서버와 클라이언트가 같은 순서로 agent를 등록한다는 전제에서만 안전하다.
`RemoveAtSwap()`이 들어가면 `AgentID == PackedIndex` 전제가 쉽게 깨진다.

## 각 타입의 역할

### `FHordeAgentHandle`

```cpp
USTRUCT()
struct FHordeAgentHandle
{
	GENERATED_BODY()
	
	UPROPERTY()
	uint32 AgentID = MAX_uint32;
	
	UPROPERTY()
	uint32 Generation = 0;
};
```

목표 역할은 **agent의 안정적인 외부 식별자**다.

- `AgentID`: 재사용 가능한 stable id
- `Generation`: 같은 `AgentID`가 재사용됐을 때 오래된 handle을 거르는 값

`AgentID`는 최종 구조에서는 배열 index가 아니다.
`AgentID`는 registry를 통해 현재 frame의 `PackedIndex`로 resolve해야 한다.

```text
FHordeAgentHandle
  -> AgentID
  -> registry에서 PackedIndex 조회
  -> MovementStorage[PackedIndex] 접근
```

### `HordeRemoveResult`

```cpp
struct HordeRemoveResult
{
	int32 RemovedIndex = INDEX_NONE;
	int32 LastIndex = INDEX_NONE;
	
	HordeAgentHandle RemovedAgent;
	HordeAgentHandle MovedAgent;
	
	bool bMovedLastAgent = false;
};
```

목표 역할은 `RemoveAtSwap()` 이후 **어떤 agent가 제거됐고, 어떤 agent가 빈 자리로 이동했는지**를 호출자에게 알려주는 것이다.

`RemoveAtSwap()`은 마지막 원소를 제거 위치로 옮기기 때문에, packed index 기반 cache를 갱신하려면 이 정보가 필요하다.

예:

```text
Before
Index 0: Agent A
Index 1: Agent B
Index 2: Agent C

Remove index 1

After RemoveAtSwap
Index 0: Agent A
Index 1: Agent C
```

이때 `HordeRemoveResult`는 다음 의미를 가져야 한다.

```text
RemovedIndex = 1
LastIndex = 2
RemovedAgent = Agent B
MovedAgent = Agent C
bMovedLastAgent = true
```

이 정보를 이용해 `Agent C`의 packed index mapping을 `2 -> 1`로 갱신한다.

## 현재 코드에서의 실제 사용 상태

### 네트워크 payload

`FHordeNetworkFormat`은 handle을 가지고 있다.

```cpp
UPROPERTY()
FHordeAgentHandle Handle;
```

payload가 클라이언트에 도착하면 `DispatchPayload()`가 handle을 읽는다.

```cpp
const FHordeAgentHandle& Handle = Payload.Handle;
if (!Handle.IsValid())
{
	return;
}

const int32 ID = Handle.AgentID;
```

현재는 `ID`를 곧바로 storage index로 사용한다.

```cpp
MovementStorage.Transforms[ID] = Payload.Transforms;
```

따라서 현재 의미는 사실상 다음과 같다.

```text
Handle.AgentID == PackedIndex
```

이것은 임시 구조로 봐야 한다.

### 제거 결과

`HordeRemoveResult`는 현재 선언만 되어 있고 실제 remove 경로에서 반환되지 않는다.

현재 storage 제거 함수들은 모두 `void`다.

```cpp
void RemoveAtSwap(const int32 PackedIndex)
```

따라서 `RemoveAtSwap()`으로 마지막 agent가 이동해도 외부 cache나 mapping이 자동으로 갱신되지 않는다.

## 올바른 사용 방식

### 1. Agent registry를 둔다

`UBudgetOverlordSubsystem` 또는 별도 registry가 다음 정보를 소유해야 한다.

```cpp
TArray<int32> AgentIDToPackedIndex;
TArray<FHordeAgentHandle> PackedIndexToHandle;
TArray<uint32> Generations;
TArray<uint32> FreeAgentIDs;
```

각 배열의 의미:

- `AgentIDToPackedIndex[AgentID]`: 현재 packed storage index
- `PackedIndexToHandle[PackedIndex]`: 해당 storage slot의 stable handle
- `Generations[AgentID]`: 현재 유효한 generation
- `FreeAgentIDs`: 제거되어 재사용 가능한 id 목록

### 2. 등록 시 handle을 발급한다

등록 흐름은 다음 형태가 되어야 한다.

```cpp
FHordeAgentHandle Handle;

if (!FreeAgentIDs.IsEmpty())
{
	Handle.AgentID = FreeAgentIDs.Pop();
}
else
{
	Handle.AgentID = Generations.Add(0);
	AgentIDToPackedIndex.Add(INDEX_NONE);
}

Handle.Generation = Generations[Handle.AgentID];

const int32 PackedIndex = MovementStorage.Add(Transform, MoveSpeed);
AgentIDToPackedIndex[Handle.AgentID] = PackedIndex;
PackedIndexToHandle.Add(Handle);
```

등록 함수는 `PackedIndex`만 반환하지 말고 `Handle`도 반환하거나 저장해야 한다.

```cpp
FHordeAgentHandle UBudgetOverlordSubsystem::RegisterAgent(...);
```

### 3. payload는 packed index가 아니라 handle을 보낸다

서버가 movement payload를 만들 때는 현재 packed index로 storage를 읽되, payload에는 stable handle을 넣는다.

```cpp
const int32 PackedIndex = ...;
const FHordeAgentHandle Handle = PackedIndexToHandle[PackedIndex];

FHordeNetworkFormat Payload;
Payload.Handle = Handle;
Payload.Transforms = MovementStorage.Transforms[PackedIndex];
Payload.MoveSpeed = MovementStorage.MoveSpeeds[PackedIndex];
Payload.Velocities = MovementStorage.Velocities[PackedIndex];

NetworkSubsystem->AddPayload(Payload);
```

### 4. 수신 시 handle을 packed index로 resolve한다

클라이언트는 payload의 `AgentID`를 배열 index로 직접 쓰면 안 된다.
먼저 registry에서 현재 packed index를 찾아야 한다.

```cpp
bool TryResolvePackedIndex(
	const FHordeAgentHandle& Handle,
	int32& OutPackedIndex) const
{
	if (!Handle.IsValid())
	{
		return false;
	}

	if (!Generations.IsValidIndex(Handle.AgentID))
	{
		return false;
	}

	if (Generations[Handle.AgentID] != Handle.Generation)
	{
		return false;
	}

	if (!AgentIDToPackedIndex.IsValidIndex(Handle.AgentID))
	{
		return false;
	}

	const int32 PackedIndex = AgentIDToPackedIndex[Handle.AgentID];
	if (!MovementStorage.Transforms.IsValidIndex(PackedIndex))
	{
		return false;
	}

	OutPackedIndex = PackedIndex;
	return true;
}
```

적용 코드는 다음처럼 되어야 한다.

```cpp
int32 PackedIndex = INDEX_NONE;
if (!TryResolvePackedIndex(Payload.Handle, PackedIndex))
{
	return;
}

MovementStorage.Transforms[PackedIndex] = Payload.Transforms;
MovementStorage.MoveSpeeds[PackedIndex] = Payload.MoveSpeed;
```

### 5. 제거 시 `HordeRemoveResult`로 mapping을 갱신한다

storage 제거 함수는 `HordeRemoveResult`를 반환하는 쪽이 맞다.

```cpp
HordeRemoveResult RemoveAgentByPackedIndex(const int32 RemovedIndex)
{
	const int32 LastIndex = PackedIndexToHandle.Num() - 1;

	HordeRemoveResult Result;
	Result.RemovedIndex = RemovedIndex;
	Result.LastIndex = LastIndex;
	Result.RemovedAgent = PackedIndexToHandle[RemovedIndex];
	Result.bMovedLastAgent = RemovedIndex != LastIndex;

	if (Result.bMovedLastAgent)
	{
		Result.MovedAgent = PackedIndexToHandle[LastIndex];
	}

	MovementStorage.RemoveAtSwap(RemovedIndex);
	ProxyStorage.RemoveAtSwap(RemovedIndex);
	StatusStorage.RemoveAtSwap(RemovedIndex);
	PackedIndexToHandle.RemoveAtSwap(RemovedIndex);

	++Generations[Result.RemovedAgent.AgentID];
	AgentIDToPackedIndex[Result.RemovedAgent.AgentID] = INDEX_NONE;
	FreeAgentIDs.Add(Result.RemovedAgent.AgentID);

	if (Result.bMovedLastAgent)
	{
		AgentIDToPackedIndex[Result.MovedAgent.AgentID] = RemovedIndex;
	}

	return Result;
}
```

이후 호출자는 `Result`를 이용해 actor cache도 갱신한다.

```cpp
if (Result.bMovedLastAgent)
{
	// MovedAgent가 LastIndex에서 RemovedIndex로 이동했음을 반영한다.
}
```

## 현재 시스템에서 당장 주의할 점

현재 `Generation`은 값만 있고 증가/검증 경로가 없다.
따라서 오래된 payload를 완전히 걸러내지는 못한다.

현재 `AgentID`는 실질적으로 packed index처럼 쓰이고 있다.
agent 제거가 없거나 서버/클라이언트 등록 순서가 완전히 같으면 일단 동작할 수 있지만, `RemoveAtSwap()`이 들어가는 순간 잘못된 agent에 payload가 적용될 수 있다.

현재 `HordeRemoveResult`는 아직 미사용이다.
이 구조체는 다음 단계에서 remove 경로를 고칠 때 사용해야 한다.

## 구현 우선순위

1. `RegisterAgent()`가 `FHordeAgentHandle`을 발급하도록 변경한다.
2. `PackedIndexToHandle`, `AgentIDToPackedIndex`, `Generations`, `FreeAgentIDs`를 추가한다.
3. payload 생성 시 `Handle`을 정확히 채운다.
4. payload 수신 시 `Handle.AgentID`를 직접 index로 쓰지 않고 resolve한다.
5. 모든 storage remove 경로가 `HordeRemoveResult`를 만들고 mapping/cache를 갱신하게 한다.
6. `Generation` mismatch payload는 무시한다.

## 결론

지금 시스템에서 `FHordeAgentHandle`은 **네트워크 payload 필드로만 부분 사용** 중이다.
`HordeRemoveResult`는 **아직 사용되지 않는다**.

최종 구조에서는 `FHordeAgentHandle`을 packed index 대신 외부 식별자로 쓰고, 매 접근 시 registry로 현재 packed index를 찾아야 한다.
`HordeRemoveResult`는 `RemoveAtSwap()`으로 이동한 agent의 mapping과 cache를 갱신하기 위한 결과 타입으로 사용해야 한다.
