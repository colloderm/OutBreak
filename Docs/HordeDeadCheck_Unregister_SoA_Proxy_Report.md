# Horde DeadCheck / Unregister / SoA Reuse / Proxy 반환 점검 보고서

작성 기준: 현재 워크트리의 `Source/OutBreak` 코드

## 요약

현재 구조는 `RegisterAgent()`가 Movement, Proxy, Status 서브시스템에 같은 packed index 순서로 데이터를 추가하고, `DeadCheck()`에서 체력이 0 이하인 index를 `UnregisterAgent()`로 넘겨 세 서브시스템에서 `RemoveAtSwap()`을 수행하는 방식이다.

의도만 보면 SoA 배열은 `RemoveAtSwap(..., EAllowShrinking::No)`를 사용하므로 `Num`은 줄고 capacity는 유지된다. 따라서 다음 `Register` 때 TArray 내부 capacity를 재사용할 수 있다.

하지만 현재 구현은 end-to-end 기준으로는 안전하게 반환 및 재할당된다고 보기 어렵다.

- `HordeStatusStorage::RemoveAtSwap()`이 `CurrentHealths`를 제거하지 않는다.
- `DeadCheck()`가 앞에서 뒤로 순회하며 swap remove를 호출하므로 죽은 agent를 건너뛸 수 있다.
- `IndexByActor`, `DamageEventIndexMap`, `HordeDamageEvents`가 unregister 이후 갱신/삭제되지 않아 stale index가 남는다.
- `UHordeProxySubsystem::Unregister()`는 `ProxyStorage` 배열만 줄이고, 실제 `AHordeProxyActor`와 `AHordeProxyHost`의 ISM instance를 반환하지 않는다.
- Movement transform 개수와 ISM instance 개수가 달라져 다음 proxy update에서 check 실패 가능성이 높다.

## 현재 Register 흐름

관련 위치:

- `Source/OutBreak/Private/FlowField/Subsystem/BudgetOverlordSubsystem.cpp:66`
- `Source/OutBreak/Private/FlowField/Subsystem/HordeMovementSubsystem.cpp:23`
- `Source/OutBreak/Private/FlowField/Subsystem/HordeProxySubsystem.cpp:28`
- `Source/OutBreak/Private/FlowField/Subsystem/HordeStatusSubsystem.cpp:47`

`UBudgetOverlordSubsystem::RegisterAgent()`는 다음 순서로 등록한다.

1. `MovementSubsystem->Register(inTransform, inMoveSpeed)`
2. `ProxySubsystem->Register(inTransform)`
3. `IndexByActor.FindOrAdd(ProxyResult.Actor, ProxyResult.Index)`
4. `StatusSubsystem->Register(MaxHealth, HealthPercent)`

세 storage가 모두 append-only로 같은 순서에 추가된다면 packed index 정렬은 맞는다. 즉, 신규 등록 시점에는 Movement/Proxy/Status가 같은 index를 공유한다는 전제가 성립한다.

주의점은 `IndexByActor`가 proxy actor를 key로 `ProxyResult.Index`를 저장한다는 점이다. 이후 `RemoveAtSwap()`으로 다른 agent가 해당 index로 이동하면, 이동된 actor의 `IndexByActor` 값도 갱신해야 한다. 현재는 이 갱신 경로가 없다.

## 현재 DeadCheck / Unregister 흐름

관련 위치:

- `Source/OutBreak/Private/FlowField/Subsystem/HordeStatusSubsystem.cpp:65`
- `Source/OutBreak/Private/FlowField/Subsystem/BudgetOverlordSubsystem.cpp:84`

`UHordeStatusSubsystem::DeadCheck()`는 `StatusStorage.CurrentHealths[i] <= 0.f`이면 `BudgetOverlord->UnregisterAgent(i)`를 호출한다.

`UnregisterAgent()`는 같은 index로 다음 세 storage를 제거한다.

1. `MovementSubsystem->Unregister(Index)`
2. `ProxySubsystem->Unregister(Index)`
3. `StatusSubsystem->Unregister(Index)`

Movement와 Proxy storage는 `RemoveAtSwap()`을 호출한다. Status도 호출하지만 현재 Status 구현에는 치명적인 누락이 있다.

## SoA 공간 반환 및 재할당 가능 여부

### MovementStorage

관련 위치:

- `Source/OutBreak/Public/FlowField/Struct/HordeSystemType.h:56`
- `Source/OutBreak/Public/FlowField/Struct/HordeSystemType.h:106`

`HordeMovementStorage::RemoveAtSwap()`은 모든 SoA 배열에서 같은 index를 제거한다.

- `Transforms`
- `Velocities`
- `CachedFlowDirections`
- `MoveSpeeds`
- `MovementStates`
- `TraversalStates`
- `PriorityTiers`

그리고 `EAllowShrinking::No`를 사용한다. 따라서 MovementStorage 자체는 packed index 제거와 capacity 재사용 관점에서 의도에 가깝다.

### StatusStorage

관련 위치:

- `Source/OutBreak/Public/FlowField/Struct/HordeSystemType.h:122`
- `Source/OutBreak/Public/FlowField/Struct/HordeSystemType.h:152`

현재 `HordeStatusStorage::RemoveAtSwap()`은 `MaxHealths`만 제거한다.

`CurrentHealths`는 제거하지 않는다.

결과:

- `MaxHealths.Num()`과 `CurrentHealths.Num()`가 불일치한다.
- `Size()`는 `MaxHealths.Num()`만 반환하므로 `CurrentHealths`의 stale 값이 남는다.
- 다음 `Add()` 때 `CurrentHealths.Add()`와 `MaxHealths.Add()`가 서로 다른 logical slot을 가리키게 된다.
- 이후 `DeadCheck()`와 damage 적용은 잘못된 체력 값을 읽거나 쓸 수 있다.

따라서 StatusStorage는 현재 SoA 공간 반환 및 재할당이 올바르게 동작하지 않는다.

추가로 `IsValid()`가 배열 길이 동등성을 검사하지 않고 `AgentCount`를 bool로 반환한다. Status storage의 정합성 검증으로는 부족하다.

### ProxyStorage

관련 위치:

- `Source/OutBreak/Public/FlowField/Struct/HordeSystemType.h:162`
- `Source/OutBreak/Public/FlowField/Struct/HordeSystemType.h:200`
- `Source/OutBreak/Private/FlowField/Subsystem/HordeProxySubsystem.cpp:66`

`HordeProxyStorage::RemoveAtSwap()`은 내부 배열에서는 `PoseIndices`, `InstanceIds`, `PawnProxies`를 같이 제거한다.

하지만 `UHordeProxySubsystem::Unregister()`는 이 storage 배열만 줄인다.

실제 외부 리소스는 반환하지 않는다.

- `AHordeProxyActor`를 `Destroy()`하지 않는다.
- `AHordeProxyHost::RemoveInstance()`를 호출하지 않는다.
- `IndexByActor`에서 제거된 proxy actor key를 삭제하지 않는다.
- swap으로 이동한 proxy actor의 actor-to-index mapping을 갱신하지 않는다.

따라서 ProxyStorage 배열의 packed slot은 줄어들지만, proxy actor와 ISM instance는 반환되지 않는다.

## ProxyActor / ISM 반환 문제

관련 위치:

- `Source/OutBreak/Private/FlowField/Subsystem/HordeProxySubsystem.cpp:31`
- `Source/OutBreak/Private/FlowField/Subsystem/HordeProxySubsystem.cpp:52`
- `Source/OutBreak/Private/FlowField/Subsystem/HordeProxySubsystem.cpp:68`
- `Source/OutBreak/Private/FlowField/HordeProxyHost.cpp:36`
- `Source/OutBreak/Private/FlowField/HordeProxyHost.cpp:41`

`Register()`는 두 종류의 proxy 리소스를 만든다.

1. `HordeProxy->AddInstance(Transform)`로 ISM instance 추가
2. `World->SpawnActor<AHordeProxyActor>()`로 collision proxy actor spawn

그런데 `Unregister()`는 `ProxyStorage.RemoveAtSwap(Index)`만 호출한다.

`AHordeProxyHost::RemoveInstance()` 함수는 존재하지만 호출되지 않는다. `AHordeProxyActor`도 destroy되지 않는다.

이 상태에서 agent가 죽으면 다음 문제가 발생한다.

- MovementStorage에서는 transform이 제거되어 `Transforms.Num()`가 감소한다.
- ISM instance는 제거되지 않아 `InstancedStaticMesh->GetInstanceCount()`는 그대로다.
- `AHordeProxyHost::UpdateInstances()`는 `check(Transforms.Num() == InstancedStaticMesh->GetInstanceCount())`를 수행한다.

즉, 죽은 agent가 한 명이라도 unregister되면 다음 `ProxySubsystem->ProcessSystem()`에서 transform 개수와 ISM instance 개수가 달라져 check 실패 가능성이 있다.

또한 actor proxy는 world에 계속 남기 때문에 hit/overlap 대상이 유효한 것처럼 보일 수 있고, stale `IndexByActor`와 결합하면 이미 제거된 index로 damage event가 들어갈 수 있다.

## DeadCheck 순회 문제

관련 위치:

- `Source/OutBreak/Private/FlowField/Subsystem/HordeStatusSubsystem.cpp:65`

현재 순회는 앞에서 뒤로 진행한다.

```cpp
for (int32 i = 0; i < StatusStorage.Size(); i++)
{
    if(StatusStorage.CurrentHealths[i] <= 0.f)
    {
        BudgetOverlord->UnregisterAgent(i);
    }
}
```

`RemoveAtSwap()`은 마지막 agent를 제거된 index로 이동시킨다. 따라서 index `i`를 제거한 뒤 `i++`가 실행되면, 방금 `i`로 이동한 agent는 이번 DeadCheck에서 검사되지 않는다.

예:

1. index 0이 죽어서 제거된다.
2. 마지막 agent가 index 0으로 이동한다.
3. loop는 index 1로 넘어간다.
4. 새로 index 0에 온 agent는 이번 pass에서 검사되지 않는다.

이 문제는 뒤에서 앞으로 순회하거나, death queue를 만든 뒤 descending index 순서로 unregister해야 한다.

## DamageEvent / Index cache 문제

관련 위치:

- `Source/OutBreak/Public/FlowField/Subsystem/HordeStatusSubsystem.h:18`
- `Source/OutBreak/Public/FlowField/Subsystem/HordeStatusSubsystem.h:21`
- `Source/OutBreak/Private/FlowField/Subsystem/HordeStatusSubsystem.cpp:23`
- `Source/OutBreak/Private/FlowField/Subsystem/HordeStatusSubsystem.cpp:76`
- `Source/OutBreak/Private/FlowField/Subsystem/BudgetOverlordSubsystem.cpp:51`
- `Source/OutBreak/Private/FlowField/Subsystem/BudgetOverlordSubsystem.cpp:75`

`AddDamageEvent()`는 `BudgetOverlord->GetIndexByActor(DamagedActor)`로 status index를 가져와 damage event에 저장한다.

현재 위험 요소:

- `GetIndexByActor()`가 `INDEX_NONE`을 반환해도 event가 추가된다.
- `Parallel()`에서 `CurrentHealths[StatusIndex]` 접근 전에 index 유효성 검사가 없다.
- `HordeDamageEvents`는 처리 후 비워지지 않는다.
- `DamageEventIndexMap`도 비워지지 않는다.
- Unregister 시 `IndexByActor`에서 제거된 actor가 삭제되지 않는다.
- RemoveAtSwap으로 이동한 actor의 index가 갱신되지 않는다.

이 구조에서는 한 번 들어온 damage가 매 tick 반복 적용될 수 있고, unregister 이후 stale index로 health array를 접근할 수 있다.

## 결론

질문 기준으로 답하면 다음과 같다.

### 죽었을 때 Unregister 처리로 SoA 공간 반환이 되는가?

부분적으로만 된다.

MovementStorage는 `RemoveAtSwap(..., EAllowShrinking::No)`로 packed slot을 줄이고 capacity를 유지하므로 공간 재사용 의도에 맞다.

ProxyStorage도 내부 배열만 보면 slot은 줄어든다.

하지만 StatusStorage는 `CurrentHealths`를 제거하지 않아 SoA 정합성이 깨진다.

### 반환된 공간에 재할당이 가능한가?

TArray capacity 관점에서는 가능하다.

하지만 현재 end-to-end gameplay object 기준으로는 안전하지 않다.

이유:

- Status SoA가 깨진다.
- Actor-to-index cache가 갱신되지 않는다.
- Damage event cache가 stale 상태로 남는다.
- ProxyActor와 ISM instance가 반환되지 않는다.
- 다음 proxy update에서 transform count와 instance count mismatch가 발생할 수 있다.

### ProxyActor는 제대로 반환되는가?

아니다.

현재 `UHordeProxySubsystem::Unregister()`는 `ProxyStorage.RemoveAtSwap(Index)`만 수행한다. 실제 `AHordeProxyActor::Destroy()`와 `AHordeProxyHost::RemoveInstance()`가 호출되지 않는다.

## 권장 수정 방향

우선순위 순서:

1. `HordeStatusStorage::RemoveAtSwap()`에서 `CurrentHealths.RemoveAtSwap()`도 같이 호출한다.
2. `HordeStatusStorage::Initialize()`에서 `CurrentHealths.Reserve(Capacity)`를 추가한다.
3. `HordeStatusStorage::IsValid()`를 `MaxHealths.Num() == CurrentHealths.Num()` 검사로 바꾼다.
4. `DeadCheck()`를 뒤에서 앞으로 순회하거나 death queue + descending unregister로 바꾼다.
5. `UnregisterAgent()`가 `HordeRemoveResult`처럼 removed/moved 정보를 받아 cache를 갱신할 수 있게 한다.
6. `IndexByActor`에서 제거된 actor를 삭제하고, swap으로 이동한 actor의 index를 새 index로 갱신한다.
7. `DamageEventIndexMap`과 `HordeDamageEvents`를 처리 후 clear하거나, tick 단위 pending event queue로 운용한다.
8. `AddDamageEvent()`에서 `INDEX_NONE`과 storage index 유효성을 검사한다.
9. `UHordeProxySubsystem::Unregister()`에서 제거 대상 proxy actor를 destroy하거나 pool에 반환한다.
10. `UHordeProxySubsystem::Unregister()`에서 `AHordeProxyHost::RemoveInstance()`를 호출한다.
11. ISM `RemoveInstance()`도 swap/remove 성격이 있으므로 `InstanceIds`와 packed index의 관계를 명확히 갱신한다.

## 권장 설계 메모

현재 구조는 packed index를 모든 subsystem이 공유하는 방향이다. 이 방향을 유지하려면 unregister는 단순히 `Index` 하나만 넘기는 API보다, 제거 결과를 명시적으로 반환하는 API가 안전하다.

예:

- removed packed index
- removed actor
- moved-from last index
- moved actor
- moved actor의 새 index
- removed ISM instance id
- swap 이후 변경된 ISM instance id

이미 `HordeRemoveResult` 구조체가 있으므로 이 목적에 맞게 확장하거나 실제 unregister 경로에 연결하는 것이 적합하다.

핵심은 "배열에서 제거했다"와 "외부 리소스 및 index cache까지 반환/갱신했다"를 같은 unregister transaction으로 묶는 것이다.
