# Unreal 네트워크 기능 구현 속성 가이드

이 문서는 OutBreak 프로젝트에서 네트워크 가능한 기능을 구현할 때 지켜야 할 규칙과 설계 기준을 정리한다. 예시는 현재 Flow Field 기반 Pawn 이동 구조를 기준으로 한다.

## 1. 현재 Flow Field 병목 지점

### 1.1 클라이언트 Navigation Data 의존성

현재 Flow Field 이동은 서버 위치 복제를 쓰지 않고 각 월드가 같은 Flow Field 입력으로 로컬 이동을 계산한다.

```cpp
bReplicates = true;
SetReplicateMovement(false);
bNetLoadOnClient = true;
```

따라서 클라이언트도 `AFlowFieldRecastNavMesh`와 Navigation Query가 있어야 한다. `Config/DefaultEngine.ini`의 `bAllowClientSideNavigation=False` 상태에서는 클라이언트 NavigationSystem/NavData가 생성되지 않아 다음 경로가 끊긴다.

```text
AFlowFieldAgentPawn::Tick
  AFlowFieldAgentPawn::UpdateFlowGoal
    UFlowFieldSubsystem::SetGoal
      AFlowFieldRecastNavMesh::BuildFlowField

UFlowFieldMovementComponent::TickComponent
  TryBuildFreshFlowMove
    UFlowFieldSubsystem::QueryConstrainedMove
      AFlowFieldRecastNavMesh::QueryConstrainedMove
        MoveUpdatedComponent
```

이 문제는 `bAllowClientSideNavigation=True`가 맞는 설계다. 서버 위치 복제를 켜는 방식은 이 프로젝트의 목표와 다르다.

### 1.2 `BuildFlowField()` 비용

`AFlowFieldRecastNavMesh::BuildFlowField()`는 단순히 Goal 위치 하나만 저장하지 않는다. 내부에서 전체 타일/폴리곤을 다시 수집하고, NavLink를 수집하고, Dijkstra 방식으로 Integration Cost를 계산하고, 방향을 선택한다.

핵심 비용 구간:

```text
BuildFlowField
  CollectFlowNodes
    GetAllNavMeshTiles
    GetPolysInTile
    GetPolyNeighbors
    CollectNavLinkNeighbors
    CollectGeneratedNavLinkNeighbors
  CalculateIntegrationCosts
  SelectFlowDirections
  SmoothFlowDirections
```

Goal Actor가 자주 움직이면 이 계산이 서버와 클라이언트 양쪽에서 반복된다. Goal 갱신은 거리 임계값, 시간 임계값, 상태 변경 기준을 둬야 한다.

### 1.3 Pawn마다 Goal 제출을 시도하는 구조

현재 `AFlowFieldAgentPawn::Tick()`에서 각 Pawn이 `UpdateFlowGoal()`을 호출한다. 500 Pawn이면 매 프레임 500번 Goal 상태를 확인한다. `SetGoal()` 내부 캐시로 대부분은 빠르게 return할 수 있지만, 구조적으로는 공유 Goal을 각 Pawn이 제출하는 형태다.

권장 구조:

```text
GameState 또는 FlowFieldGoalManager
  공유 Goal Actor 또는 Goal 위치 1회 복제
  서버와 클라이언트에서 Goal 변경 시 FlowFieldSubsystem::SetGoal 1회 호출

FlowFieldAgentPawn
  Goal을 제출하지 않음
  MovementComponent에서 이미 만들어진 Flow Field만 Query
```

Pawn별로 Goal이 다른 게임 규칙이라면 Pawn별 Goal 복제가 가능하지만, 모든 Pawn이 같은 Goal을 쓰는 좀비 Horde 구조에서는 공유 관리자 하나가 맞다.

### 1.4 500 Pawn의 이동 쿼리 비용

`UFlowFieldMovementComponent::TickComponent()`는 매 프레임 다음 작업을 할 수 있다.

```text
QueryNodeRef
QueryNavLink
QueryConstrainedMove
FindMoveAlongSurface
GetPolyWallSegments
MoveUpdatedComponent
Sweep / Slide
Gravity
Rotation update
```

따라서 대량 Pawn에서는 다음을 지켜야 한다.

- Flow Field 재계산은 낮은 빈도로 한다.
- Pawn별 이동 쿼리는 프레임 분산한다.
- 캐시된 방향은 짧은 시간 재사용한다.
- Debug Draw와 로그는 기본 꺼짐 상태여야 한다.
- Actor Transform 복제를 켜서 비용을 네트워크로 옮기지 않는다.

현재 코드에는 `bEnableFlowQueryThrottling`, `FlowQueryIntervalFrames`, `MaxCachedFlowQueryAgeSeconds`가 있으므로 대량 Pawn에서는 이 옵션을 적극 사용한다.

## 2. Unreal 네트워크 기본 규칙

### 2.1 서버 권위가 기본이다

게임 결과를 바꾸는 결정은 서버가 한다.

서버가 결정해야 하는 것:

- Pawn 생성과 Destroy
- Damage, Health, Death
- Item 획득
- 점수, Wave 상태, Objective 상태
- AI 목표 선정
- 최종 판정이 필요한 상호작용

클라이언트가 해도 되는 것:

- 입력 수집
- 로컬 예측
- 카메라, UI, 사운드, 이펙트
- 서버가 승인한 상태를 화면에 반영
- 서버와 동일 입력으로 재현 가능한 로컬 시뮬레이션

### 2.2 복제할 것은 결과가 아니라 "공유해야 하는 상태"다

네트워크 기능 설계에서 가장 먼저 결정할 질문은 이것이다.

```text
이 기능은 무엇을 복제해야 모든 머신이 같은 결론에 도달하는가?
```

나쁜 예:

```text
매 프레임 Pawn Transform 복제
매 프레임 이동 방향 RPC
매 프레임 애니메이션 상태 RPC
```

좋은 예:

```text
Goal 위치 1회 복제
Wave 상태 복제
공격 시작 이벤트 Multicast
서버 Damage 결과 복제
클라이언트가 같은 Goal/NavMesh로 이동 계산
```

Flow Field Pawn은 `위치 결과`가 아니라 `이동을 결정하는 입력`을 공유해야 한다.

### 2.3 Actor 존재 복제와 Movement 복제는 다르다

```cpp
bReplicates = true;
SetReplicateMovement(false);
```

`bReplicates = true`는 Actor의 존재, replicated property, RPC 경로를 복제한다.  
`SetReplicateMovement(true)`는 Actor 위치/회전/속도를 `ReplicatedMovement`로 복제한다.

OutBreak Flow Field Pawn은 Actor 존재는 복제하지만 이동 Transform은 복제하지 않는 설계다.

### 2.4 Role을 기준으로 코드를 나눈다

자주 쓰는 판단:

```cpp
HasAuthority()              // 서버 권위 Actor인가
GetNetMode() == NM_Client   // 이 World가 클라이언트인가
IsLocallyControlled()       // 이 Pawn을 내 로컬 입력으로 조종하는가
GetLocalRole()              // Authority, AutonomousProxy, SimulatedProxy
```

일반 규칙:

- `HasAuthority()`는 게임 판정과 상태 변경에 쓴다.
- `IsLocallyControlled()`는 입력, 카메라, 로컬 UI에 쓴다.
- `ROLE_SimulatedProxy`에서도 시각적 로컬 시뮬레이션은 가능하다.
- MovementComponent Tick을 무조건 `HasAuthority()`로 막으면 클라이언트 로컬 시뮬레이션이 죽는다.

## 3. Actor 속성 가이드

### 3.1 `bReplicates`

Actor를 네트워크에 올릴지 결정한다.

```cpp
bReplicates = true;
```

서버에서 Spawn한 Actor가 클라이언트에 나타나야 하면 true다. Runtime Spawn Actor는 서버에서 Spawn해야 한다.

### 3.2 `bNetLoadOnClient`

맵에 배치된 Actor가 클라이언트에서도 로드되어야 하는지 결정한다.

```cpp
bNetLoadOnClient = true;
```

Flow Field Pawn, Goal Actor처럼 맵에 미리 배치되고 클라이언트 로컬 계산에 필요한 Actor는 true가 필요하다.

### 3.3 `SetReplicateMovement`

Actor Transform 복제 여부다.

```cpp
SetReplicateMovement(false);
```

Flow Field Pawn처럼 로컬 시뮬레이션을 의도한 Actor는 false가 맞다.

켜야 하는 경우:

- 물리 Projectile처럼 서버 Transform이 정답인 경우
- 수가 적고 정확한 위치 동기화가 더 중요한 경우
- 클라이언트 로컬 재현이 불가능한 이동인 경우

켜면 안 되는 경우:

- 500개 이상 대량 Pawn
- 클라이언트가 같은 입력으로 움직일 수 있는 시스템
- 매 프레임 Transform을 네트워크로 보내면 병목이 되는 시스템

### 3.4 Net Cull Distance, Relevancy

대량 Actor는 모든 클라이언트에 항상 복제하면 안 된다.

사용할 수 있는 설정:

```cpp
NetCullDistanceSquared
bAlwaysRelevant
bOnlyRelevantToOwner
NetUpdateFrequency
MinNetUpdateFrequency
NetDormancy
```

규칙:

- 좀비 Pawn은 일반적으로 `bAlwaysRelevant=false`가 맞다.
- 모든 플레이어가 항상 봐야 하는 GameState성 데이터만 Always Relevant로 둔다.
- 자주 변하지 않는 Actor는 Dormancy를 쓴다.
- 대량 Actor의 `NetUpdateFrequency`는 낮게 잡고, 꼭 필요한 상태만 복제한다.

## 4. UPROPERTY Replication 속성

### 4.1 일반 복제

```cpp
UPROPERTY(Replicated)
int32 WaveIndex = 0;
```

```cpp
void AMyActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AMyActor, WaveIndex);
}
```

서버에서 값이 바뀌면 클라이언트로 전달된다.

### 4.2 `ReplicatedUsing`

값이 클라이언트에 도착했을 때 후처리가 필요하면 사용한다.

```cpp
UPROPERTY(ReplicatedUsing=OnRep_FlowGoalLocation)
FVector_NetQuantize10 FlowGoalLocation;

UFUNCTION()
void OnRep_FlowGoalLocation();
```

Flow Field Goal 위치는 좋은 후보이다. 클라이언트는 `OnRep_FlowGoalLocation()`에서 `UFlowFieldSubsystem::SetGoal()`을 호출하면 된다.

### 4.3 복제 조건

```cpp
DOREPLIFETIME_CONDITION(AMyActor, Ammo, COND_OwnerOnly);
```

자주 쓰는 조건:

- `COND_OwnerOnly`: 소유자에게만
- `COND_SkipOwner`: 소유자 제외
- `COND_InitialOnly`: 초기 1회
- `COND_SimulatedOnly`: Simulated Proxy에게만

대량 시스템에서는 복제 조건을 적극적으로 써야 한다.

### 4.4 값 타입 선택

네트워크용 값은 정밀도를 의식해서 고른다.

```cpp
FVector_NetQuantize
FVector_NetQuantize10
FVector_NetQuantize100
FRotator
uint8
int16
FName
FGameplayTag
```

좌표가 매 프레임 필요하지 않고 Goal 입력 정도라면 `FVector_NetQuantize10`이 적절하다.

## 5. RPC 규칙

### 5.1 Server RPC

클라이언트가 서버에 요청할 때 사용한다.

```cpp
UFUNCTION(Server, Reliable)
void ServerRequestUseItem(int32 SlotIndex);
```

규칙:

- 서버가 다시 검증해야 한다.
- 클라이언트 값을 믿지 않는다.
- 입력 요청에는 사용해도 되지만 결과 판정은 서버가 한다.
- 매 프레임 호출하지 않는다.

### 5.2 Client RPC

서버가 특정 소유 클라이언트에게만 알릴 때 사용한다.

```cpp
UFUNCTION(Client, Reliable)
void ClientShowInventoryError();
```

UI 오류, 개인 알림, 소유자 전용 피드백에 적합하다.

### 5.3 NetMulticast RPC

서버가 관련 클라이언트 모두에게 이벤트를 보낼 때 사용한다.

```cpp
UFUNCTION(NetMulticast, Unreliable)
void MulticastDamageHit(float DamageAmount, FVector_NetQuantize HitLocation);
```

이펙트, 사운드, 짧은 연출에 적합하다.

금지:

- 매 프레임 Transform 전송
- 매 프레임 이동 방향 전송
- 대량 Pawn 전체에 반복 이벤트 전송

### 5.4 Reliable 남용 금지

`Reliable`은 반드시 도착해야 하는 메시지다. 너무 많이 보내면 큐가 밀리고 더 중요한 패킷까지 늦어진다.

Reliable 후보:

- 아이템 획득 요청
- 문 열기 요청
- Match 시작/종료
- 중요한 UI 알림

Unreliable 후보:

- 피격 이펙트
- 발소리
- 총구 화염
- 짧은 애니메이션 cue
- 반복 가능한 위치 보정

## 6. 기능별 설계 패턴

### 6.1 공유 Goal 기반 Flow Field

권장 설계:

```text
서버
  Goal Actor 또는 Goal Location 결정
  GameState / FlowFieldGoalManager에 Goal 입력 복제

클라이언트
  OnRep_GoalLocation
    FlowFieldSubsystem::SetGoal(GoalLocation)

서버와 클라이언트
  각자 MovementComponent Tick
  QueryConstrainedMove
  MoveUpdatedComponent
```

예시:

```cpp
UPROPERTY(ReplicatedUsing=OnRep_FlowGoalLocation)
FVector_NetQuantize10 FlowGoalLocation;

void AFlowFieldGoalManager::SetGoalAuthority(const FVector& NewGoal)
{
    if (!HasAuthority())
    {
        return;
    }

    FlowGoalLocation = NewGoal;
    ApplyGoalLocal();
}

void AFlowFieldGoalManager::OnRep_FlowGoalLocation()
{
    ApplyGoalLocal();
}

void AFlowFieldGoalManager::ApplyGoalLocal()
{
    if (UWorld* World = GetWorld())
    {
        if (UFlowFieldSubsystem* FlowField = World->GetSubsystem<UFlowFieldSubsystem>())
        {
            FlowField->SetGoal(FlowGoalLocation);
        }
    }
}
```

이 방식은 500 Pawn이 같은 Goal Actor 참조를 각각 복제하지 않고, 공유 Goal 위치 하나만 복제한다.

### 6.2 서버 Spawn Actor

규칙:

```text
클라이언트는 Spawn 요청만 한다.
서버가 검증한다.
서버가 Spawn한다.
Actor bReplicates=true면 클라이언트에 생성된다.
```

예시:

```cpp
UFUNCTION(Server, Reliable)
void ServerRequestSpawnZombie(FVector_NetQuantize Location);

void AMySpawner::ServerRequestSpawnZombie_Implementation(FVector_NetQuantize Location)
{
    if (!CanSpawnAt(Location))
    {
        return;
    }

    GetWorld()->SpawnActor<AFlowFieldAgentPawn>(ZombieClass, Location, FRotator::ZeroRotator);
}
```

현재 `UFlowFieldSubsystem::SpawnFlowFieldPawn()`이 `NM_Client`에서 return하는 것은 올바른 방향이다.

### 6.3 Damage

권장 흐름:

```text
클라이언트 입력
  ServerFire / ServerMelee

서버
  Hit 검증
  Damage 계산
  Health 변경
  Death 결정
  필요한 이펙트 Multicast

클라이언트
  복제된 Health/UI 반영
  Multicast 이펙트 재생
```

규칙:

- 클라이언트가 DamageAmount를 최종 결정하지 않는다.
- 서버가 사망과 Destroy를 결정한다.
- 이펙트는 Multicast 가능하지만 Health 결과는 replicated property가 안전하다.

### 6.4 Inventory / Equipment

소유자만 알아야 하는 정보와 모두 알아야 하는 정보를 분리한다.

소유자 전용:

- 탄약 상세 수량
- 인벤토리 슬롯
- 퀵슬롯

전체 관련:

- 손에 든 무기 Mesh
- 발사 이펙트
- Reload 애니메이션 cue

설계:

```text
InventoryComponent
  OwnerOnly 복제

EquipmentComponent
  외부 시각 상태 복제
```

### 6.5 UI

UI는 복제하지 않는다. UI가 읽는 데이터를 복제한다.

좋은 구조:

```text
PlayerState / AbilitySystem / InventoryComponent
  복제 데이터 보유

ViewModel / Widget
  OnRep 또는 Delegate를 받아 화면 갱신
```

나쁜 구조:

```text
서버가 클라이언트 Widget 함수를 계속 RPC로 호출
```

## 7. Flow Field 네트워크 설계 원칙

### 7.1 복제해야 하는 입력

Flow Field에서 네트워크로 공유할 후보:

- Goal 위치
- Goal Actor Net GUID 또는 shared manager 참조
- Wave 상태
- NavLink 활성/비활성 상태
- 장애물 상태 중 게임플레이에 영향을 주는 것
- 드물게 발생하는 Anchor Correction

복제하면 안 되는 것:

- 매 프레임 Pawn 위치
- 매 프레임 FlowDirection
- 매 프레임 QueryConstrainedMove 결과
- 전체 FlowNodes 배열
- 전체 Integration Cost 배열

### 7.2 서버와 클라이언트가 같은 입력을 가져야 한다

로컬 시뮬레이션은 결정 입력이 같을 때만 성립한다.

확인할 것:

- 클라이언트에 같은 NavMesh가 있는가
- Goal 위치가 같은가
- NavLink 상태가 같은가
- MovementComponent 설정값이 같은가
- Collision 설정이 같은가
- Tick이 막히지 않았는가

### 7.3 오차는 생길 수 있다

서버와 클라이언트가 같은 입력으로 움직여도 다음 때문에 오차가 생길 수 있다.

- Tick Delta 차이
- Collision sweep 결과 차이
- NavMesh generation 차이
- Pawn 간 분리 이동 차이
- 부동소수점 누적

대책:

```text
짧은 게임플레이 구간
  로컬 시뮬레이션만 사용

장시간 유지되는 Horde
  낮은 주기의 Anchor Correction 추가

정확한 전투 판정
  서버 위치와 서버 판정 사용
```

Anchor Correction은 매 프레임 Transform 복제가 아니다. 예를 들어 1초에 1회 또는 큰 오차가 있을 때만 서버 기준 위치를 낮은 정밀도로 보내고 클라이언트가 부드럽게 보정한다.

## 8. 새 네트워크 기능 구현 체크리스트

기능을 만들기 전에 다음 질문에 답한다.

```text
1. 이 기능의 권위자는 서버인가 클라이언트인가?
2. 클라이언트가 서버에 요청해야 하는 입력은 무엇인가?
3. 서버가 검증해야 하는 조건은 무엇인가?
4. 모든 클라이언트가 알아야 하는 상태는 무엇인가?
5. 소유자만 알아야 하는 상태는 무엇인가?
6. 단발 이벤트인가, 지속 상태인가?
7. Reliable이 필요한가?
8. NetMulticast가 정말 필요한가?
9. Actor 수가 100개, 500개, 1000개가 되어도 괜찮은가?
10. Join-in-progress 클라이언트가 들어와도 현재 상태를 복원할 수 있는가?
```

판단 규칙:

- 지속 상태는 replicated property가 우선이다.
- 단발 연출은 RPC가 가능하다.
- 늦게 들어온 클라이언트도 알아야 하면 property로 남긴다.
- 잠깐 보이고 사라지는 이펙트는 RPC로 충분하다.
- 대량 Actor는 공유 상태를 관리자에 모은다.

## 9. 로그와 디버깅 규칙

대량 Pawn 시스템에서 로그는 기능이다. 무제한 로그는 병목이다.

좋은 로그:

```text
상태 변경 시 1회
첫 실패 시 1회
2초 또는 5초 간격 제한
콘솔 변수로 켰을 때만 출력
Actor, NetMode, Role, 핵심 값 포함
```

나쁜 로그:

```text
모든 Pawn 매 프레임 Warning
Tick마다 FString 대량 생성
Shipping에서도 항상 켜짐
원인 없이 "Failed"만 출력
```

현재 추가된 진단 스위치:

```text
OutBreak.FlowField.NetDiagnostics 1
```

확인 가능한 항목:

- Actor Name
- NetMode
- LocalRole
- FlowGoalActor 유효 여부
- Goal Location
- SetGoal 성공/실패
- FlowFieldSubsystem 유효 여부
- HasFlowField
- QueryNodeRef 성공 여부
- QueryDirection 성공 여부
- QueryConstrainedMove 성공 여부
- OutMoveOffset
- UpdatedComponent 유효 여부
- Component Tick 활성 여부

## 10. OutBreak 프로젝트 권장 아키텍처

### 10.1 Flow Field Goal Manager

현재 Pawn마다 Goal 제출을 시도하는 구조는 작동은 가능하지만 확장성이 낮다. 다음 단계에서는 공유 관리자 Actor를 두는 것이 좋다.

```text
AFlowFieldGoalManager
  bReplicates=true
  bAlwaysRelevant=true 또는 GameState 소유
  ReplicatedUsing FlowGoalLocation
  서버에서만 Goal 변경
  서버/클라이언트 OnRep에서 SetGoal 호출
```

위치는 GameState가 가장 무난하다. GameState는 모든 클라이언트가 알아야 하는 Match 상태를 담는 용도이기 때문이다.

### 10.2 Pawn

Pawn은 이동 입력을 만들지 않고 이미 구축된 Flow Field를 소비한다.

```text
AFlowFieldAgentPawn
  Actor 존재 복제
  Movement Transform 복제 안 함
  Health/Death는 서버 권위
  시각 효과만 Multicast

UFlowFieldMovementComponent
  서버/클라이언트 모두 Tick 허용
  QueryConstrainedMove 사용
  캐시/분산 쿼리 사용
```

### 10.3 NavLink / 장애물 상태

NavLink가 게임 중 활성/비활성으로 바뀐다면 그 상태는 서버에서 결정하고 공유해야 한다.

```text
서버
  NavLink 상태 변경
  상태 복제

클라이언트
  OnRep에서 NavLink 상태 적용
  필요 시 Flow Field 재계산
```

NavMesh 자체 전체를 복제하려고 하면 안 된다. 같은 정적 NavMesh를 양쪽에서 만들고, 바뀌는 최소 상태만 공유한다.

## 11. 구현 금지 목록

OutBreak Flow Field 구조에서 금지한다.

- `AFlowFieldAgentPawn`에 `SetReplicateMovement(true)` 적용
- Pawn Transform을 매 프레임 Multicast
- Pawn Transform을 매 프레임 Client RPC
- 500 Pawn 각각에 같은 Goal Actor 참조 중복 복제
- 클라이언트가 Damage/Death 최종 판정
- MovementComponent Tick을 `HasAuthority()`로 차단
- Flow Field 전체 노드 배열을 네트워크 복제
- 원인 분석 없이 RPC만 추가

## 12. 빠른 결론

네트워크 기능은 "무엇을 보낼지"보다 "무엇을 보내지 않아도 같은 결과가 나오는지"를 먼저 설계해야 한다.

OutBreak Flow Field의 정답은 다음이다.

```text
서버 권위 유지
Actor 존재만 복제
Movement Transform 복제 금지
공유 Goal 입력만 복제
클라이언트 Navigation 허용
서버/클라이언트가 같은 Flow Field를 로컬 생성
각자 MoveUpdatedComponent로 이동
필요할 때만 낮은 주기의 위치 보정
```

이 원칙을 지키면 대량 Pawn에서도 네트워크 대역폭을 이동 결과가 아니라 게임 상태에 사용할 수 있다.
