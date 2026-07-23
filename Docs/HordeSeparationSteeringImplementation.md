# Horde Separation Steering Implementation

## 1. 작업 목적

Flow Field가 같은 방향을 반환할 때 Horde 에이전트들이 겹쳐 이동하는 현상을 줄이기 위해, 현재 프레임의 위치 스냅샷을 기준으로 주변 에이전트에서 멀어지는 Separation Steering을 추가했다. 이번 구현은 독립 이동력이 아니라 기존 Flow Field 이동 방향을 제한적으로 수정하는 조향력이다.

## 2. 수정한 파일

- `Source/OutBreak/Private/FlowField/Subsystem/HordeMovementSubsystem.cpp`
- `Source/OutBreak/Public/FlowField/Settings/FlowFieldSettings.h`
- `Source/OutBreak/Public/FlowField/Struct/HordeSystemType.h`
- `Docs/HordeSeparationSteeringImplementation.md`

## 3. 기존 이동 파이프라인

Authority는 `UHordeMovementSubsystem::Parallel()`에서 각 에이전트에 대해 `QueryConstrainedMove()`를 호출하고, 생성된 `MoveOffset`을 마지막 `ParallelFor`에서 Transform과 `Velocities`에 적용했다. Client는 네트워크로 받은 `CachedFlowDirections`와 `MoveSpeeds`를 사용해 로컬 이동을 수행했다.

## 4. 추가된 Separation 수식

각 에이전트는 반경 안의 주변 에이전트에 대해 `Normalize(P_i - P_j) * Square(1 - Distance / Radius)`를 누적한다. 누적 결과는 `GetClampedToMaxSize(1.0f)`로 제한한다. 기존 Flow Field 이동량과 합성할 때는 `BaseMoveOffset + SeparationDirection * BaseMoveDistance * Weight`를 계산한 뒤, 최종 길이를 `BaseMoveDistance` 이하로 제한한다.

## 5. Spatial Grid 구조

`FHordeSeparationGridEntry` 배열에 `(CellX, CellY, AgentIndex)`를 저장하고 cell key 기준으로 정렬한다. 이후 `FHordeSeparationCellRange` 배열을 만들어 각 cell의 entry range를 기록한다. 병렬 계산에서는 자기 cell과 주변 8개 cell만 binary search로 찾아 검사한다. cell 크기는 `SeparationRadius`와 같다.

## 6. 위치 스냅샷이 필요한 이유

Separation은 `MovementStorage.PositionSnapshot`에 복사된 프레임 시작 위치만 읽는다. 마지막 Transform 적용 `ParallelFor`가 다른 에이전트의 위치를 갱신하는 중인 값을 읽지 않기 때문에, 같은 프레임의 모든 에이전트가 동일한 기준 상태로 계산된다.

## 7. Authority 처리

Authority 처리 순서는 `AgentCount` 확인, 위치 스냅샷 생성, Spatial Grid 구성, Separation 계산, 기존 `QueryConstrainedMove()` 실행, MoveOffset 합성, Transform/Velocity 적용 순서다. 기존 Flow Field 실패 fallback은 유지했다. 성공한 Flow query의 실제 이동은 기존처럼 원본 `MoveOffset`을 기준으로 하고, 그 위에 Separation만 합성한다.

## 8. Client 처리

Client도 동일한 스냅샷과 Spatial Grid 계산을 수행한다. 이동 방향은 `CachedFlowDirections`의 2D normal에 `SeparationDirection * SeparationSteeringWeight`를 더한 뒤 다시 2D 정규화한다. 네트워크 패킷에는 SeparationDirection을 추가하지 않았다. 서버와 클라이언트 위치가 다르면 Separation 결과도 달라질 수 있다.

## 9. 스레드 안전성 검토

게임 스레드에서 snapshot, grid entry, cell range, output 배열 크기를 모두 확정한다. Separation `ParallelFor`는 `PositionSnapshot`, `SeparationGridEntries`, `SeparationCellRanges`를 읽기 전용으로만 사용하고, 자기 `SeparationDirections[AgentIndex]`만 기록한다. 병렬 구간에서 `UWorld`, `UObject`, Actor, Component, `TArray::Add`, `Remove`, `SetNum`은 호출하지 않는다.

## 10. 완전 중첩 위치 처리

두 에이전트의 2D 거리가 0에 가까우면 `(min index, max index)` hash로 cardinal axis를 고르고, 낮은 index와 높은 index가 서로 반대 방향을 받는다. 랜덤값, 시간 기반 값, `FRandomStream`은 사용하지 않는다.

## 11. 설정값과 튜닝 방법

`UFlowFieldSettings`에 `SeparationRadius = 90.0f`, `SeparationSteeringWeight = 0.35f`를 `Config, EditAnywhere`로 추가했다. 반경은 주변 탐색 cell 크기와 동일하게 쓰이며, 가중치는 Flow Field 방향에 Separation이 섞이는 정도다. 디버그는 `OutBreak.HordeMovement.SeparationDebug`와 `OutBreak.HordeMovement.SeparationDebugIndex` cvar로 한 에이전트만 표시한다.

## 12. 성능 복잡도

grid entry 정렬은 프레임당 `O(N log N)`이고, 각 에이전트는 최대 9개 cell만 검사한다. 같은 cell에 과도하게 몰린 최악의 경우에는 해당 cell 안에서 비교량이 늘 수 있다. `PositionSnapshot`, grid, range, base/final move offset은 `HordeMovementStorage` scratch 배열을 재사용해 반복 할당을 줄였다.

## 13. 확인한 기존 코드 문제

- `FlowDirectionSmoothingAlpha`로 보간한 방향은 `CachedFlowDirections`에 저장되지만, Authority의 성공 이동에는 원본 `MoveOffset`이 사용된다. 따라서 보간은 주로 fallback과 Client 이동에 반영된다.
- `Velocities`는 현재 초당 속도라기보다 프레임 이동 오프셋으로 쓰인다.
- Authority는 `MoveSpeed * DeltaSeconds`와 `MaxVelocity`를 `Min`으로 비교한다. `MaxVelocity`가 초당 속도라면 단위가 맞지 않는다.
- Client도 `MoveSpeeds[AgentIndex] * DeltaSeconds`를 `CurrentAcceleration`으로 부르고, 결과 이동 오프셋을 `MaxSpeed`로 clamp한다. 이름과 단위가 실제 의미와 다르다.
- Separation은 현재 `QueryConstrainedMove()` 이후에 합성된다. NavMesh 경계 근처에서는 최종 이동이 제한 결과에서 벗어날 수 있다.
- 기존 코드에는 프레임 임시 배열이 있었고, 이번 변경은 새 Separation 관련 배열을 Storage scratch로 재사용하게 했다.
- `RemoveAtSwap` 이후 packed index 기반 debug 대상은 다른 에이전트를 가리킬 수 있다. Separation fallback은 현재 packed index 조합 기준이라 메모리 안정성에는 문제가 없지만, 에이전트 identity 기준의 장기 일관성은 제공하지 않는다.

## 14. 현재 구현의 한계

Separation은 Flow Field 이동량이 0이면 독립적으로 에이전트를 움직이지 않는다. 최종 desired direction을 NavMesh에 다시 constrain하지 않으므로 경계 정확성에는 한계가 있다. Client와 Authority는 같은 규칙을 쓰지만 각자의 로컬 위치를 기준으로 계산하므로 완전히 동일한 결과를 보장하지 않는다.

## 15. 다음 리팩터링 권장 순서

`QueryConstrainedMove()`를 `QueryDirection()`과 `ConstrainDesiredMove()` 단계로 분리하는 것이 우선이다. 이상적인 순서는 Flow 방향 질의, 방향 보간, Separation 합성, 최종 desired direction 생성, NavMesh 표면 제한, Transform 적용이다.

## 16. 빌드 및 검증 결과

- `OutBreakEditor Win64 Development` 빌드 성공.
- 빌드 중 `OBMainMenuWidget.cpp`의 `UUserWidget::bIsFocusable` deprecation warning이 1건 발생했으며 이번 변경과 무관하다.
- `OutBreakServer Win64 Development`는 현재 UE 배포판이 server target을 지원하지 않아 UBT가 즉시 실패했다.
- `OutBreak Win64 Development`는 기존 `OutBreak.Build.cs -> AnimationData -> MovieSceneTools -> UnrealEd` 의존성 때문에 non-editor target 규칙 생성에서 실패했다.
- `Source/OutBreak` 아래에 기존 `*Test*`, `*Spec*` 소스 파일은 확인되지 않았다. 별도 테스트 프레임워크는 새로 만들지 않았다.
- 실행 중 에이전트 동작 검증은 에디터 시나리오를 실행하지 못해 수행하지 않았다. 컴파일 검증과 코드 경로 검토 기준으로는 에이전트 수 0, 1, 다수 조건과 음수 cell 좌표, 병렬 read-only 규칙을 만족하도록 구현했다.
