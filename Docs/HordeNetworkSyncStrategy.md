# Horde Network Sync Strategy

## 결론

500마리 Horde Agent의 Transform을 전부 매 프레임 동기화하는 방식은 쓰면 안 된다.

이 시스템은 서버가 권위를 가지되, 클라이언트는 마지막으로 받은 Horde 스냅샷과 Flow 방향으로 계속 Local Simulate하고, 다음 동기화 시점에 서버 상태로 보정하는 구조가 맞다. 단, `UHordeNetworkSubsystem` 자체는 `UWorldSubsystem`이라 Unreal replication 채널이 아니므로, 실제 전송은 별도 replicated Actor 또는 PlayerController Client RPC 경로가 필요하다.

권장 방향:

- 서버: 전체 Horde를 authoritative SoA로 시뮬레이션한다.
- 클라이언트: 마지막 스냅샷의 `Transform`, `Velocity`, `CachedFlowDirection`, `MovementState`, `TraversalState`로 예측 이동한다.
- 동기화: 모든 Agent를 같은 주기로 보내지 않고, Agent별 priority tier와 per-client relevancy로 전송 주기를 다르게 한다.
- 보정: 작은 오차는 부드럽게 보간하고, 큰 오차/사망/Traversal 전환은 즉시 스냅한다.

## 현재 코드 기준 상태

### 이미 있는 기반

- `UBudgetOverlordSubsystem`이 중앙 Tick 순서를 잡고 있다.
  - `MovementSubsystem->ProcessSystem(DeltaTime)`
  - `StatusSubsystem->ProcessSystem(DeltaTime)`
  - `ProxySubsystem->ProcessSystem(DeltaTime)`
- `HordeMovementStorage`는 네트워크 예측에 필요한 핵심 데이터를 이미 가지고 있다.
  - `Transforms`
  - `Velocities`
  - `CachedFlowDirections`
  - `MovementStates`
  - `TraversalStates`
  - `PriorityTiers`
- `HordeNetworkFormat`도 최소 형태의 네트워크 payload 후보로 존재한다.
- `UFlowFieldSettings`에 `NetworkUpdateInterval`, `OffscreenUpdateIntervalScale`, `MaxAgentCount`가 있다.

### 아직 부족한 부분

- `UHordeNetworkSubsystem`은 현재 빈 껍데기다.
- `UWorldSubsystem`은 자체적으로 replication되지 않는다.
- `PriorityTiers`는 저장만 되고 계산/사용되지 않는다.
- `HordeNetworkFormat`은 `FTransform`을 그대로 들고 있어 크고, timestamp/revision/error correction 정보가 없다.
- `RegisterAgent()`와 `UnregisterAgent()`에 네트워크 hook 주석은 있지만 실제 구현은 없다.
- `RemoveAtSwap()` 후 이동된 마지막 Agent의 mapping 갱신 계약이 없다.
- `IndexByActor`는 Proxy Actor 기준 index cache지만 swap remove 이후 갱신되지 않으면 Status/Network 모두 틀어진다.
- `HordeStatusStorage::RemoveAtSwap()`은 `CurrentHealths`를 같이 지우지 않는다.
- `DeadCheck()`에서 순회 중 `UnregisterAgent(i)`를 호출하면 swap remove 때문에 다음 Agent를 건너뛸 수 있다.

네트워크를 얹기 전에 stable handle과 packed index mapping을 먼저 고정해야 한다.

## 설계 원칙

### 1. 복제 단위는 Actor가 아니라 Horde Snapshot이다

500마리를 각각 replicated Actor로 만들면 목적과 반대다.

복제해야 하는 것은 다음 중 하나다.

- Agent 생성/삭제 이벤트
- Agent별 낮은 빈도의 movement snapshot
- 전역 FlowField goal/revision
- 전투/피격/사망/Traversal 같은 gameplay event

Proxy Actor와 ISM/VAT는 로컬 표현이다. 서버 권위 판단에 필요한 일부 collision proxy를 제외하면 `SetReplicateMovement(true)` 대상으로 만들지 않는다.

### 2. FlowField 전체를 보내지 않는다

FlowField graph 전체를 네트워크로 보내는 것은 비용이 크고 유지보수가 어렵다.

권장 기본값:

- 서버가 Agent별 `CachedFlowDirection`을 계산한다.
- 클라이언트는 마지막으로 받은 `CachedFlowDirection`으로 계속 이동한다.
- 다음 스냅샷에서 위치/속도/방향을 갱신하고 보정한다.

선택 옵션:

- 클라이언트 NavMesh가 안정적으로 켜져 있고 서버와 동일한 NavData를 보장할 수 있으면 `GoalLocation + FlowRevision`만 복제하고 클라이언트가 FlowField를 재계산할 수 있다.
- 현재 프로젝트 구조에서는 이 옵션을 기본으로 잡지 않는 편이 안전하다.

### 3. 서버 권위, 클라이언트 Local Simulate

서버는 항상 최종 판정자다.

클라이언트는 시각적 연속성을 위해 다음 값을 로컬로 적분한다.

```text
Position += Velocity * DeltaTime
Velocity = CachedFlowDirection * MoveSpeed
Rotation = Velocity direction
```

동기화 시점에는 서버 스냅샷의 timestamp 기준으로 클라이언트 현재 예측 위치와 서버 외삽 위치를 비교한다.

```text
ServerSnapshotPosition
ServerSnapshotVelocity
SnapshotServerTime
ClientEstimatedServerNow
PredictedServerPosition = ServerSnapshotPosition + ServerSnapshotVelocity * TimeSinceSnapshot
Error = PredictedServerPosition - ClientLocalPosition
```

## 권장 네트워크 구조

### 클래스 역할

```text
UBudgetOverlordSubsystem
  Agent lifecycle, Tick order, authoritative packed storage

UHordeMovementSubsystem
  Server authoritative movement
  Client local simulate movement

UHordeNetworkSubsystem
  Network scheduling only
  직접 replication하지 않음

AHordeNetworkBridge 또는 UActorComponent on PlayerController
  실제 Client RPC / replicated payload 전송

UHordeProxySubsystem
  로컬 렌더링/캡슐 프록시 갱신
```

`UHordeNetworkSubsystem`은 전송할 Agent를 고르고 payload를 만든다. 실제 송신은 per-client 경로가 필요하므로 다음 중 하나를 쓴다.

- `APlayerController` 소유 component의 `ClientReceiveHordeSnapshot(...)` unreliable RPC
- Player별 `AHordeNetworkBridge` Actor를 생성하고 owner-only RPC로 전송
- 장기적으로는 Replication Graph 또는 Iris fragment로 per-connection filtering

초기 구현은 PlayerController Client RPC가 가장 단순하다.

### Tick 파이프라인

```text
Server Tick
  1. Apply queued spawn/despawn/damage/traversal events
  2. Update FlowField goal/revision if needed
  3. MovementSubsystem.ProcessSystem
  4. StatusSubsystem.ProcessSystem
  5. NetworkSubsystem.BuildPerClientBatches
  6. ProxySubsystem.ProcessSystem

Client Tick
  1. Apply spawn/despawn events
  2. Apply received snapshots into correction targets
  3. MovementSubsystem.LocalSimulate
  4. Smooth correction or snap if needed
  5. ProxySubsystem.ProcessSystem
```

Network 단계는 Proxy 갱신보다 앞에 있어도 되고 뒤에 있어도 된다. 중요한 것은 Network payload가 한 frame 안에서 일관된 movement/status snapshot을 읽어야 한다는 점이다.

## Agent 식별자와 mapping

네트워크 payload는 packed index를 직접 보내면 안 된다.

packed index는 `RemoveAtSwap()` 때문에 서버와 클라이언트에서 언제든 달라질 수 있다. 외부 이벤트, 네트워크, proxy binding은 stable handle을 써야 한다.

필수 mapping:

```text
HordeAgentHandle = AgentID + Generation

AgentID -> PackedIndex
PackedIndex -> AgentID
AgentID -> Generation
FreeAgentIDs
```

서버 snapshot에는 `HordeAgentHandle`을 넣고, 클라이언트는 자기 쪽 `AgentID -> PackedIndex`로 local storage 위치를 찾는다.

생성/삭제는 reliable event로 보낸다.

```text
Spawn:
  Handle
  InitialTransform
  MoveSpeed
  InitialHealth
  InitialPriorityTier
  Optional visual archetype id

Despawn:
  Handle
  Reason
  ServerTime
```

Movement snapshot은 unreliable로 보내도 된다. 다음 snapshot이 이전 snapshot을 대체하기 때문이다.

## Snapshot payload

현재 `HordeNetworkFormat`은 방향성은 맞지만 그대로 쓰기에는 크고 애매하다.

권장 payload:

```cpp
struct FHordeAgentNetSnapshot
{
    HordeAgentHandle Handle;
    FVector_NetQuantize10 Location;
    FVector_NetQuantizeNormal FlowDirection;
    FVector_NetQuantize10 Velocity;
    uint16 YawCompressed;
    uint8 MovementState;
    uint8 TraversalState;
    uint8 PriorityTier;
    uint16 FlowRevision;
    uint16 SnapshotSequence;
};
```

추가로 batch header가 필요하다.

```cpp
struct FHordeSnapshotBatch
{
    uint16 BatchSequence;
    float ServerTimeSeconds;
    uint16 FlowRevision;
    FVector_NetQuantize GoalLocation;
    TArray<FHordeAgentNetSnapshot> Agents;
};
```

압축 방향:

- `FTransform` 전체 대신 위치 + yaw + velocity/flow direction만 보낸다.
- scale은 Agent archetype에서 고정한다.
- roll/pitch가 필요 없으면 yaw만 보낸다.
- `MoveSpeed`는 자주 변하지 않으면 spawn payload 또는 rare state update로 분리한다.

## 우선순위 tier

`MovementStorage.PriorityTiers`를 실제 scheduler 입력으로 사용한다.

기본 tier:

| Tier | 대상 | 권장 주기 | 설명 |
| --- | --- | ---: | --- |
| 0 Critical | 플레이어 근접, 공격 중, 피격 직후, Traversal 중, 충돌 판정 중요 | 0.05-0.10s | 거의 실시간 보정 |
| 1 High | 화면 안/근거리/플레이어 진행 방향 | 0.10-0.20s | 일반 전투 가시 영역 |
| 2 Medium | 화면 밖이지만 근거리, 곧 보일 가능성 있음 | 0.30-0.50s | Local simulate 비중 큼 |
| 3 Low | 원거리/offscreen/전투 영향 낮음 | 1.00-2.00s | 큰 보정만 허용 |
| 4 Dormant | 아주 멀거나 relevance 없음 | event only | spawn/despawn/goal revision 정도만 |

주기는 고정값보다 누적 age 방식이 낫다.

```text
AgentNetAge += DeltaTime
RequiredInterval = TierInterval[PriorityTier]

if AgentNetAge >= RequiredInterval:
  CandidateForSend
```

전송 후보가 너무 많으면 score로 자른다.

```text
Score =
  AgeRatio * 100
  + DistanceImportance
  + ScreenImportance
  + CombatImportance
  + ErrorImportance
  + TraversalImportance
```

per-client 기준으로 계산해야 한다. 같은 Agent라도 어떤 플레이어에게는 Tier 0, 다른 플레이어에게는 Tier 3일 수 있다.

## Relevancy 계산

각 클라이언트마다 다음 정보를 기준으로 priority를 계산한다.

- 플레이어와 Agent 거리
- 카메라 frustum 안에 있는지
- 플레이어가 바라보는 방향과 Agent 방향 dot
- Agent가 최근 damage를 받았는지
- Agent가 플레이어를 공격하거나 blocking 중인지
- Agent가 NavLink traversal 중인지
- 이전 동기화 이후 client/server 오차가 커졌는지

초기 버전은 거리만으로 시작해도 된다.

```text
Distance < 1200    -> Tier 0
Distance < 3000    -> Tier 1
Distance < 7000    -> Tier 2
Distance < 12000   -> Tier 3
Else               -> Tier 4
```

이후 화면 안 여부와 전투 이벤트로 tier를 끌어올린다.

## 전송 예산

주기만으로는 burst를 막기 어렵다. frame별 byte/agent budget이 필요하다.

권장 설정:

```text
MaxHordeSnapshotAgentsPerClientPerFrame = 32
MaxHordeSnapshotBytesPerClientPerFrame = 4096
MaxCriticalAgentsPerClientPerFrame = 16
```

전송 루프:

```text
1. per-client candidate 수집
2. Tier 0 먼저 선택
3. 나머지는 score 높은 순으로 선택
4. frame budget 초과 시 다음 tick으로 이월
5. 보낸 Agent는 NetAge = 0
```

500마리 전체가 있어도 한 client에게 한 frame에 500개를 보내지 않는다.

## 보정 정책

클라이언트 보정은 오차 크기에 따라 나눈다.

| Error | 처리 |
| ---: | --- |
| < 30cm | 무시 또는 아주 약한 보간 |
| 30-150cm | 100-200ms 동안 위치 보간 |
| 150-500cm | 50-100ms 강한 보정 |
| > 500cm | snap |
| TraversalState 변경 | state machine 기준 snap 가능 |
| Death/Despawn | 즉시 적용 |

Low tier Agent는 화면 밖일 가능성이 높으므로 큰 오차도 부드러운 보정보다 snap이 싸다. 화면 안 Tier 0/1은 시각적 튐을 줄이기 위해 보간한다.

보정 데이터는 movement storage와 분리된 client-only buffer에 둔다.

```text
CorrectionTargetLocation
CorrectionTargetYaw
CorrectionRemainingTime
LastReceivedSequence
LastReceivedServerTime
```

서버 snapshot sequence가 오래된 것이면 버린다.

## FlowField revision

서버는 FlowField goal이 바뀌거나 NavMesh/Link 상태가 바뀌면 `FlowRevision`을 증가시킨다.

snapshot에는 `FlowRevision`을 넣는다.

클라이언트 처리:

- 같은 revision이면 기존 `CachedFlowDirection` 기반 local simulate 지속
- 새 revision이면 batch의 `GoalLocation`과 각 Agent의 `FlowDirection`을 갱신
- optional client nav mode에서는 새 revision 수신 시 local FlowField rebuild

중요한 점은 클라이언트가 direction을 못 받는 긴 구간에서도 마지막 direction으로 계속 움직인다는 것이다. 이것이 user가 말한 “마지막 갱신된 FlowField 정보로 계속 이동”의 구현 지점이다.

## 이벤트 분리

Movement snapshot과 gameplay event를 섞으면 안 된다.

Reliable 또는 상태 복구 가능한 이벤트:

- Spawn
- Despawn/Death
- Wave state
- Goal change
- Archetype 변경

Unreliable 가능한 이벤트:

- Movement snapshot
- Animation pose hint
- 단발 visual effect

Damage는 서버 권위로 처리하고, 클라이언트에는 결과만 보낸다.

```text
Client hit request -> Server validates -> StatusSubsystem applies damage
Server sends health/death event if visible/relevant
```

## 구현 순서

### Phase 1. 네트워크 전 안정화

먼저 이걸 하지 않으면 네트워크가 계속 무너진다.

- `HordeAgentHandle` 기반 registry 추가
- `AgentID -> PackedIndex`, `PackedIndex -> AgentID` mapping 추가
- 모든 `RemoveAtSwap()`이 `HordeRemoveResult`를 반환하도록 정리
- swap으로 이동된 Agent의 mapping, proxy, status, network state를 갱신
- `HordeStatusStorage::RemoveAtSwap()`에서 `CurrentHealths`도 같이 삭제
- `DeadCheck()`는 뒤에서 앞으로 순회하거나 death queue로 분리

### Phase 2. Network bridge 추가

- `UHordeNetworkSubsystem`은 scheduling만 담당
- PlayerController component 또는 `AHordeNetworkBridge`를 추가
- spawn/despawn은 reliable
- movement batch는 unreliable
- batch에는 server time, sequence, flow revision 포함

### Phase 3. Priority scheduler

- `PriorityTiers` 계산 함수 추가
- per-client net age 배열 추가
- `MaxAgentsPerFrame`, `MaxBytesPerFrame` budget 추가
- Tier 0부터 보내고 나머지는 score로 컷

### Phase 4. Client local simulate

- 클라이언트에도 Horde SoA storage를 둔다.
- snapshot 수신 시 storage를 직접 덮지 말고 correction target에 넣는다.
- 매 tick 마지막 `CachedFlowDirection`으로 이동한다.
- correction buffer를 적용한 뒤 Proxy를 갱신한다.

### Phase 5. 고도화

- 화면 안 여부 기반 tier 보정
- 공격/피격/Traversal 중 tier 승격
- FlowRevision 기반 goal 변경 처리
- bandwidth 로그와 cvar 추가
- 장기적으로 Replication Graph/Iris 검토

## 피해야 할 구현

- 500개 Proxy Actor에 `SetReplicateMovement(true)` 적용
- 매 tick 모든 Agent Transform RPC
- packed index를 네트워크 ID로 사용
- `UWorldSubsystem`에 replicated property를 넣고 동작할 것이라 기대
- `NetMulticast`로 모든 클라이언트에 같은 Horde batch 전송
- FlowField graph 전체를 매번 복제
- 클라이언트 보정 없이 snapshot 위치로 즉시 덮어쓰기

## 최소 구현 목표

1차 목표는 완전한 최적화가 아니라 구조가 무너지지 않는 동기화다.

성공 기준:

- 서버 500마리 시뮬레이션 유지
- 클라이언트는 모든 Agent를 local simulate
- 근거리 Agent만 자주 보정
- 원거리 Agent는 드문드문 보정
- spawn/death는 놓치지 않음
- packed index swap 후에도 네트워크/상태/proxy mapping이 깨지지 않음
- frame별 전송 수가 budget을 넘지 않음

이 기준을 만족하면 이후 전송 압축, 화면 기반 priority, Replication Graph/Iris 최적화를 붙일 수 있다.
