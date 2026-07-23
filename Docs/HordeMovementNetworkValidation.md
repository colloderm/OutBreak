# Horde Movement Network Validation

## 현재 호출 흐름

현재 프로젝트 코드 기준 네트워크 이동 동기화 흐름은 다음과 같다.

```text
Server Horde system
  -> UHordeNetworkSubsystem::AddPayload()
  -> UHordeNetworkSubsystem::Payloads 누적
  -> UBudgetOverlordSubsystem::Tick()
  -> UHordeNetworkSubsystem::ProcessSystem()
  -> UHordeNetworkSubsystem::SendPayloads()
  -> per-player AHordeNetworkBridgeActor::ClientReceivePayloads()
  -> client UHordeNetworkSubsystem::ReceivePayloads()
  -> UBudgetOverlordSubsystem::DispatchPayload()
  -> client MovementStorage 갱신
```

`AddPayload()` 호출부는 아직 없다. 따라서 현재 상태에서 payload가 자동으로 생성되지는 않는다.
다만 서버에서 `AddPayload()`가 호출되어 `Payloads`에 값이 들어오면, 이후 전송 경로는 컴파일 기준으로 연결되어 있다.

## 검증한 파일과 함수

- `UHordeNetworkSubsystem::AddPayload`
- `UHordeNetworkSubsystem::ProcessSystem`
- `UHordeNetworkSubsystem::SendPayloads`
- `UHordeNetworkSubsystem::ReceivePayloads`
- `AHordeNetworkBridgeActor::ClientReceivePayloads`
- `UBudgetOverlordSubsystem::DispatchPayload`
- `UHordeMovementSubsystem::ProcessSystem`
- `FHordeNetworkFormat`
- `FHordeAgentHandle`

## 확인 결과

`FHordeNetworkFormat`은 `USTRUCT`이며 `TArray<FHordeNetworkFormat>` RPC 파라미터로 사용할 수 있다.
`FHordeAgentHandle`도 `USTRUCT`라 payload 내부에서 RPC 직렬화 가능하다.

`SendPayloads()`는 `NM_Client`에서는 반환하므로 클라이언트가 서버 RPC 전송을 시도하지 않는다.
서버 또는 Listen Server에서는 현재 World의 PlayerController를 순회하며 Bridge를 등록하고, payload가 비어 있지 않을 때만 각 Bridge에 배열 RPC를 호출한다.
RPC 호출 후에는 `Payloads.Reset()`을 수행한다.

Bridge는 `bReplicates = true`, `bOnlyRelevantToOwner = true`, `Owner = PlayerController` 조건으로 생성된다.
따라서 Client RPC 라우팅 대상은 해당 PlayerController의 owning client다.

## 수정한 문제

### Client Movement Tick 충돌

Remote Client에서도 `UHordeMovementSubsystem::ProcessSystem()`이 실행되면, 서버에서 받은 Transform을 다음 클라이언트 Tick의 로컬 이동 계산이 다시 변경할 수 있다.
Remote Client는 서버 payload 기반 storage 갱신만 수행해야 하므로 `NM_Client`에서는 movement 계산을 건너뛰도록 수정했다.

### Storage Out-of-Bounds 방어

`UBudgetOverlordSubsystem::DispatchPayload()`가 `Handle.AgentID`를 곧바로 MovementStorage 배열 인덱스로 사용하고 있었다.
클라이언트에 해당 agent storage가 아직 생성되지 않았거나 서버/클라이언트 등록 순서가 다르면 out-of-bounds가 발생할 수 있다.

수정 후에는 다음 조건을 먼저 확인한다.

- `Handle.IsValid()`
- `Transforms.IsValidIndex(ID)`
- `CachedFlowDirections.IsValidIndex(ID)`
- `MoveSpeeds.IsValidIndex(ID)`
- `Velocities.IsValidIndex(ID)`
- `MovementStates.IsValidIndex(ID)`
- `TraversalStates.IsValidIndex(ID)`
- `PriorityTiers.IsValidIndex(ID)`

검증 실패 시 `ensureAlwaysMsgf`를 남기고 payload 적용을 중단한다.

### Movement 필드 적용 범위

`DispatchPayload()`는 이제 movement payload의 주요 필드를 같은 index에 반영한다.

- `Transforms`
- `MoveSpeed`
- `Velocities`
- `CachedFlowDirections`
- `MovementStates`
- `TraversalStates`
- `PriorityTiers`

`PoseIndex`, `InstanceId`는 Proxy 정보이므로 현재 movement sync 적용에서는 건드리지 않았다.

## AgentID와 Packed Index 관계

현재 코드는 `FHordeAgentHandle::AgentID`를 MovementStorage의 packed array index로 사용한다.
이 방식이 안전하려면 다음 조건이 모두 필요하다.

- 서버와 클라이언트가 agent를 같은 순서로 등록한다.
- 제거 순서와 `RemoveAtSwap()` 결과가 서버와 클라이언트에서 동일하다.
- 지연 payload가 제거 후 재사용된 index에 적용되지 않는다.
- `Generation`을 실제 검증에 사용한다.

현재 프로젝트에는 `AgentID -> PackedIndex` registry 또는 generation 검증 경로가 아직 없다.
따라서 AddPayload가 들어와도 `AgentID`가 클라이언트 storage index와 같다는 전제가 깨지면 잘못된 agent에 적용될 수 있다.
이번 수정은 crash 방어까지만 수행하며, registry 재설계는 하지 않았다.

## Listen Server Host 처리

Listen Server Host도 Client RPC implementation을 통해 `ReceivePayloads()`를 호출할 수 있다.
현재 적용은 같은 storage index에 같은 movement 값을 다시 쓰는 형태라 직접적인 누적 부작용은 작다.
별도 host-only visual storage가 없는 현재 구조에서는 Host 수신을 강제로 막지 않았다.

## Remote Client 처리

Remote Client는 `SendPayloads()`를 실행하지 않는다.
Bridge RPC를 받으면 `ReceivePayloads()`를 통해 `DispatchPayload()`만 수행한다.
이번 수정으로 Remote Client의 권위 이동 계산은 건너뛴다.

단, Remote Client가 payload 수신 전에 같은 index의 MovementStorage를 이미 가지고 있어야 실제 적용된다.
등록 동기화가 없으면 payload는 ensure 로그 후 무시된다.

## Payload Reset 안전성

서버에서 `Bridge->ClientReceivePayloads(Payloads)` 호출 직후 `Payloads.Reset()`을 수행한다.
Unreal RPC 호출 시 파라미터는 네트워크 직렬화 경로로 넘겨지므로, 호출 뒤 원본 `TArray`를 비우는 것은 일반적으로 안전하다.

## 컴파일 결과

다음 빌드가 성공했다.

```text
OutBreakEditor Win64 Development
Result: Succeeded
```

빌드 중 기존 UI 코드에서 `UUserWidget::bIsFocusable` deprecation warning이 1개 발생했다.
네트워크 이동 동기화 변경과는 무관하다.

## 실제 실행 검증

Listen Server Host 1명 + Remote Client 1명 환경의 런타임 검증은 수행하지 않았다.
따라서 Remote Client Bridge 복제 시점, 첫 unreliable RPC 유실 여부, agent 등록 순서 일치 여부는 실제 플레이 테스트로 확인해야 한다.

특히 Bridge가 새로 spawn된 직후 같은 tick에 보내는 첫 `Unreliable` Client RPC는 클라이언트 actor channel 준비 전이면 유실될 수 있다.
이 경우 다음 payload 전송부터 정상 수신될 가능성이 높지만, 첫 payload 보장이 필요하면 Bridge 준비 상태 확인이나 한 tick 지연이 필요하다.

## 남은 선행 조건

`AddPayload()`만 추가했을 때 정상 반영되려면 최소한 다음이 충족되어야 한다.

- 서버에서만 `AddPayload()`를 호출한다.
- `FHordeNetworkFormat::Handle.AgentID`가 클라이언트 MovementStorage index와 일치한다.
- 클라이언트에도 해당 agent storage가 생성되어 있다.
- 제거 또는 `RemoveAtSwap()` 이후 stale payload가 오지 않는다.
- 첫 payload 유실을 허용하거나 Bridge 복제 준비 후 전송한다.

위 조건을 만족하면 payload 배열 전송, 수신, reset 경로는 현재 코드 기준으로 동작한다.
