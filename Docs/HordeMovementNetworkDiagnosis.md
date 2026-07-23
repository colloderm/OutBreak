# Horde Movement Network Diagnosis

작성 기준: 2026-07-07 현재 `C:\Users\Admin\Documents\Unreal Projects\OutBreak` 워크트리.

## 결론

서버 Horde Agent가 움직이지 않는 직접 원인은 `UHordeMovementSubsystem::ProcessSystem()`의 World null 체크가 반대로 작성된 것이다.

현재 코드는 `UWorld* World = GetWorld(); if (World) { return; }` 형태라서, 서버 World와 클라이언트 World가 정상적으로 존재하는 모든 Tick에서 즉시 반환한다. 따라서 `NM_Client` 분기, `SimulateClient()`, `Parallel()`, `SimulateAuthority()`가 모두 실행되지 않는다.

결과적으로 서버 `MovementStorage.Transforms`와 `MovementStorage.Velocities`는 Tick 중 갱신되지 않는다. `UHordeProxySubsystem`은 이후 `MovementStorage.Transforms`를 정상적으로 읽고 있지만, 입력 Transform 자체가 변하지 않았으므로 Proxy와 Instanced Static Mesh도 정지한 것처럼 보인다.

## 확인한 호출 흐름

`UBudgetOverlordSubsystem::Tick()`이 Horde 시스템 실행 순서를 소유한다.

```text
UBudgetOverlordSubsystem::Tick()
  -> UHordeMovementSubsystem::ProcessSystem()
  -> UHordeStatusSubsystem::ProcessSystem()
  -> UHordeProxySubsystem::ProcessSystem()
  -> UBudgetOverlordSubsystem::BuildPacket()
  -> UHordeNetworkSubsystem::ProcessSystem()
  -> UHordeNetworkSubsystem::SendPayloads()
```

이 순서는 이동 계산 후 Proxy 갱신, 이동 계산 후 네트워크 payload 생성이라는 관점에서 맞다. 문제는 `MovementSubsystem->ProcessSystem()` 내부에서 유효한 World일 때 바로 반환해 실제 이동 계산까지 도달하지 못하는 점이다.

`UBaseHordeWorldSubsystem::ProcessSystem()`은 현재 no-op이다. `Super::ProcessSystem(DeltaSeconds)`가 조기 반환하거나 상태를 바꾸는 경로는 없다.

`UBaseHordeWorldSubsystem`은 `UWorldSubsystem` 기반이고 직접 Tick하지 않는다. Tick은 `UBudgetOverlordSubsystem`의 `UTickableWorldSubsystem::Tick()`에서 수동 호출된다. `ShouldCreateSubsystem()`, `DoesSupportWorldType()`, `IsTickable()` override는 없으므로 별도 WorldType 필터는 현재 코드에 없다.

## NetMode 분기

의도한 분기는 다음이 맞다.

```text
NM_Client
  -> SimulateClient()
  -> FlowField QueryDirection 호출 없음
  -> 마지막 네트워크 수신 Transform/Direction/MoveSpeed/Velocity 기반 적분

NM_ListenServer
NM_DedicatedServer
NM_Standalone
  -> SimulateAuthority()
  -> Parallel()
  -> FlowField QueryDirection 호출 후 기존 수식으로 이동
```

PIE에서는 서버 World와 클라이언트 World마다 `UWorldSubsystem` 인스턴스가 별도로 생성된다. 따라서 진단 로그에는 반드시 `World->GetName()`, `World->WorldType`, `World->GetNetMode()`를 같이 남겨야 서버 World 로그와 클라이언트 World 로그를 구분할 수 있다.

## MovementStorage 상태

`UBudgetOverlordSubsystem::RegisterAgent()`는 등록 시 다음 순서로 각 storage에 같은 packed index를 추가한다.

```text
MovementSubsystem->Register()
ProxySubsystem->Register()
StatusSubsystem->Register()
PackedIndexToHandle.Add()
AgentIDToPackedIndex[Handle.AgentID] = PackedIndex
```

`HordeMovementStorage::IsValid()`는 `Transforms`, `Velocities`, `MoveSpeeds`, `CachedFlowDirections`, `MovementStates`, `TraversalStates`, `PriorityTiers` 길이가 같은지 검사한다. 등록 직후 배열 길이 계약은 맞다.

이번 문제는 서버 storage가 등록되지 않았거나 `IsValid()`가 false여서 발생한 것이 아니라, Tick에서 이동 계산 함수가 호출되지 않아 storage 값이 갱신되지 않은 것이다.

## Proxy 갱신 경로

`UHordeProxySubsystem::ProcessSystem()`은 `MovementSubsystem->MovementStorage.Transforms`를 읽어 다음 두 경로를 갱신한다.

```text
AHordeProxyHost::UpdateInstances(Transforms)
UHordeProxySubsystem::ParallelProxy()
  -> PawnProxy->SetActorTransform(Transforms[AgentIndex])
```

즉 Proxy는 이동 계산 이후 실행되고, movement Transform이 변하면 렌더링 프록시도 갱신되는 구조다. 현재 현상은 Proxy가 정지해서가 아니라 `MovementStorage.Transforms`가 먼저 정지했기 때문에 발생한다.

Dedicated Server에서는 렌더 Proxy를 시각적으로 확인할 수 없으므로 논리 이동은 `MovementStorage.Transforms` 로그로 확인해야 한다. Listen Server에서는 로컬 화면이 서버 World proxy를 보고 있는지, 별도 client World proxy를 보고 있는지 로그의 World 이름과 NetMode로 구분해야 한다.

## FlowField 경로

`UHordeMovementSubsystem::Initialize()`는 `Collection.InitializeDependency<UFlowFieldSubsystem>()`로 World-local `UFlowFieldSubsystem` 인스턴스를 저장한다. 이 dependency 자체는 서버 권위 이동 경로에 필요하다.

추가로 확인한 위험 지점은 `UFlowFieldSubsystem::QueryDirection()`이 `FindNavMesh()` 결과를 null 체크하기 전에 `NavMesh->HasFlowField()`와 `NavMesh->QueryDirection()`을 호출한다는 점이다. NavMesh를 찾지 못하면 이동 정지가 아니라 crash 위험이 있다. 최소 수정에서 null 체크 순서를 바로잡는 것이 안전하다.

## 네트워크 수신과 덮어쓰기

현재 전송 흐름은 다음과 같다.

```text
Server BuildPacket()
  -> NetworkSubsystem->AddPayload()
Server NetworkSubsystem->SendPayloads()
  -> AHordeNetworkBridgeActor::ClientReceivePayloads()
Client NetworkSubsystem->ReceivePayloads()
  -> BudgetOverlord->DispatchPayload()
  -> client MovementStorage 덮어쓰기
```

`ClientReceivePayloads`는 Client RPC이므로 일반 remote client에서는 클라이언트 World에서 실행된다. 다만 `ReceivePayloads()`와 `DispatchPayload()` 자체에는 NetMode 방어가 없어서, Listen Server 로컬 호출 또는 잘못된 호출 경로에서 서버 storage를 다시 쓰는 것을 막지 못한다.

또한 `BuildPacket()`은 `UBudgetOverlordSubsystem::Tick()`에서 client World에서도 호출될 수 있다. `UHordeNetworkSubsystem::SendPayloads()`는 client에서 반환하지만 payload 배열을 reset하지 않으므로, client가 payload를 쌓는 경로가 생길 수 있다. 서버 이동 정지의 직접 원인은 아니지만 서버/클라이언트 책임 분리상 client에서는 payload를 만들지 않는 것이 맞다.

## Packed index와 AgentID

`HordeMovementStorage::RemoveAtSwap()`은 movement 배열들의 길이를 함께 줄이므로 storage 내부 배열 길이 일관성은 유지한다.

다만 `UBudgetOverlordSubsystem::DispatchPayload()`는 현재 `Handle.AgentID`를 곧바로 movement 배열 index로 사용한다. RemoveAtSwap 이후 또는 AgentID 재사용 이후에는 `AgentID != PackedIndex`가 될 수 있으므로 장기적으로는 `AgentIDToPackedIndex`와 generation 검증을 사용해야 한다. 이 문제는 이번 "서버에서 아무 Agent도 움직이지 않음"의 직접 원인은 아니다.

## 수정 전 호출 흐름

```text
UBudgetOverlordSubsystem::Tick()
  -> UHordeMovementSubsystem::ProcessSystem()
     -> Super::ProcessSystem()
     -> GetWorld() returns valid World
     -> if (World) return
     -> no NetMode branch
     -> no SimulateClient()
     -> no SimulateAuthority()
     -> no Parallel()
  -> UHordeProxySubsystem::ProcessSystem()
     -> unchanged Transform을 Proxy에 반영
  -> BuildPacket()
     -> unchanged Transform payload 생성
```

## 수정 후 호출 흐름

```text
UBudgetOverlordSubsystem::Tick()
  -> UHordeMovementSubsystem::ProcessSystem()
     -> Super::ProcessSystem()
     -> if (!World) return
     -> if (NetMode == NM_Client) SimulateClient()
     -> else SimulateAuthority()
  -> UHordeProxySubsystem::ProcessSystem()
     -> 갱신된 Transform을 Proxy에 반영
  -> BuildPacket()
     -> 서버에서만 갱신된 Transform payload 생성
  -> UHordeNetworkSubsystem::ProcessSystem()
     -> 서버에서만 payload 전송
```

## 수정 파일 목록

실제 수정 파일은 다음이다.

- `Source/OutBreak/Public/FlowField/Subsystem/HordeMovementSubsystem.h`
- `Source/OutBreak/Private/FlowField/Subsystem/HordeMovementSubsystem.cpp`
- `Source/OutBreak/Private/FlowField/Subsystem/HordeProxySubsystem.cpp`
- `Source/OutBreak/Private/FlowField/Subsystem/BudgetOverlordSubsystem.cpp`
- `Source/OutBreak/Private/FlowField/Subsystem/HordeNetworkSubsystem.cpp`
- `Source/OutBreak/Private/FlowField/Subsystem/FlowFieldSubsystem.cpp`

## 재현 및 검증 방법

1. 콘솔에서 `OutBreak.FlowField.NetDiagnostics 1`을 켠다.
2. PIE Listen Server 1명과 Client 1명으로 실행한다.
3. 로그에서 `[HordeMovement]`의 `World`, `WorldType`, `NetMode`, `AgentCount`, `Function`을 확인한다.
4. 서버 또는 listen server World에서 `SimulateAuthority`와 `Parallel` 로그가 매 Tick 출력되는지 확인한다.
5. remote client World에서 `SimulateClient` 로그가 출력되고 `QueryDirection` 로그가 없는지 확인한다.
6. Agent 0의 이동 전 위치, 방향, MoveSpeed, 이동 후 위치가 서버 World에서 변하는지 확인한다.
7. `[HordeProxy]` 로그에서 movement Transform과 PawnProxy Transform이 같은 Tick에 갱신되는지 확인한다.
8. Dedicated Server에서는 화면 대신 서버 로그의 movement before/after와 network payload 생성 순서를 확인한다.

## 빌드 검증

다음 빌드가 성공했다.

```text
OutBreakEditor Win64 Development
Result: Succeeded
```

빌드 중 기존 UI 코드에서 `UUserWidget::bIsFocusable` deprecation warning 1건이 출력되었으며, Horde 이동 수정과 직접 관련은 없다.
