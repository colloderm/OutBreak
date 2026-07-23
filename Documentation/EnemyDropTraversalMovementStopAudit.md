# Enemy Drop Traversal Movement Stop Audit

## 1. 문제 개요

Custom Nav Link 기반 Drop Traversal 진입 후 좀비가 절벽 방향으로 매우 느리게 이동했다. 입력 벡터와 가속도 계산은 정상인데, 다음 프레임 전에 `Velocity`가 0으로 초기화되는 현상이 관찰됐다.

## 2. 재현 조건

- `AEnemyCharacter`가 `UEnemyMovementComponent`를 기본 Character Movement Component로 사용한다.
- `AEnemyController`가 `ADetourCrowdAIController`를 상속하므로 실제 path following은 `UCrowdFollowingComponent` 기반이다.
- AI가 `UEnemyGenNavLinksProxy` 기반 generated custom nav link에 진입한다.
- 링크 목적지가 현재 위치보다 낮아 `TraversalType == ETraversalType::Drop`으로 판정된다.

## 3. 기존 호출 흐름

수정 전 의도된 흐름은 다음과 같았다.

```text
UCrowdManager::UpdateAgentPaths
-> UCrowdFollowingComponent::StartUsingCustomLink
-> UPathFollowingComponent::StartUsingCustomLink
-> UEnemyGenNavLinksProxy::OnLinkMoveStarted
-> UEnemyMovementComponent::StartNavLinkTraversal
-> UEnemyMovementComponent::TickTraversalDrop
```

그러나 `StartNavLinkTraversal()`은 매 진입마다 `StopMovementImmediately()`를 호출했고, Drop 착지 복구 분기에서 `FinishNavLinkTraversal()`을 호출하지 않았다.

## 4. 로그 분석

제공된 로그에서 `PendingInput`과 `Acceleration`은 정상 범위였다.

```text
PendingInput=(0.57, 0.82)
Acceleration=(1158.51, 1688.83)
Velocity=(19.31, 28.15)
MovementMode=MOVE_Walking
```

문제는 다음 프레임 시작 전에 속도가 0으로 돌아가는 것이었다.

```text
StopMovementImmediately called.
VelocityBefore=V(0)
Traversing=true
```

수정 전 override는 `Super::StopMovementImmediately()`를 로그 전후로 두 번 호출했다. 따라서 `VelocityBefore=V(0)` 로그는 이미 첫 번째 `Super` 호출로 초기화된 값을 찍는 진단 오류도 포함했다.

## 5. StopMovementImmediately 반복 호출 증거

프로젝트 C++ 전역 검색 결과, 적 movement에서 반복적으로 영향을 줄 수 있는 직접 호출 지점은 `UEnemyMovementComponent::StartNavLinkTraversal()` 내부의 `StopMovementImmediately()`였다. 다른 `StopMovementImmediately()` 호출은 투사체, 플레이어 추출 처리, 일반 캐릭터 코드에 있었고 Drop traversal의 적 AI 링크 진행 경로와 직접 연결되지 않았다.

## 6. 실제 호출 스택

로컬 PIE 디버거 스택은 이 작업 환경에서 캡처하지 않았다. 대신 UE 5.7 엔진 소스와 프로젝트 호출부 기준으로 반복 시작의 소스 스택을 확인했다.

```text
UCrowdManager::UpdateAgentPaths
-> UCrowdFollowingComponent::StartUsingCustomLink
-> UPathFollowingComponent::StartUsingCustomLink
-> UEnemyGenNavLinksProxy::OnLinkMoveStarted
-> UEnemyMovementComponent::StartNavLinkTraversal
-> UMovementComponent::StopMovementImmediately
```

PathFollowing/Crowd가 traversal 중 일반 이동을 계속 요청할 수 있는 별도 경로도 확인했다.

```text
UCrowdManager::ApplyVelocity
-> UCrowdFollowingComponent::ApplyCrowdAgentVelocity
-> UEnemyMovementComponent::RequestPathMove
```

## 7. 확인된 근본 원인

근본 원인은 `ADetourCrowdAIController`가 사용하는 crowd link 처리와 프로젝트 movement 상태 전이의 결합 문제다.

`UCrowdManager::UpdateAgentPaths()`는 Detour off-mesh link animation이 active이고 `AnimInfo.t == 0`인 동안 custom link start를 다시 통지할 수 있다. 기존 `StartNavLinkTraversal()`은 같은 PathFollowing/CustomLink 조합의 중복 콜백을 구분하지 않고 매번 `StopMovementImmediately()`를 호출했다. 그 결과 Drop 접근 중 매 프레임 속도가 초기화됐다.

또한 Drop 착지 복구 후 `FinishNavLinkTraversal()`이 호출되지 않아 `UPathFollowingComponent::FinishUsingCustomLink()`로 custom link 종료가 전달되지 않았다. 이 때문에 crowd/path-following 쪽 custom link 상태가 명확히 닫히지 않았다.

## 8. 배제한 원인과 근거

- `AddInputVector()` 방향 정규화 문제: 제공 로그에서 입력과 가속도는 정상이며, 코드도 수평 방향 `GetSafeNormal()`을 사용한다.
- `OnLinkMoveStarted()` 반환값 false 문제: 프로젝트 override는 최종 `true`를 반환한다. 다만 `UBaseGeneratedNavLinksProxy`의 기본 구현은 `OnLinkMoveStarted()`를 제공하지 않아 기본 false 의미와 혼동될 수 있으므로 불필요한 `Super::OnLinkMoveStarted()` 호출은 제거했다.
- Movement Component 중복 문제: `AEnemyCharacter` 생성자에서 `ACharacter::CharacterMovementComponentName`이 `UEnemyMovementComponent`로 교체된다. 소스상 별도 `UFloatingPawnMovement` 생성은 없다.
- AIController Tick의 MoveTo 재요청 문제: `AEnemyController::Tick()`은 `Super::Tick()`만 호출하고, 프로젝트 C++ 검색에서 반복 `MoveToActor`, `MoveToLocation`, `SimpleMoveTo` 호출은 발견되지 않았다.

## 9. 수정한 파일 목록

- `Source/OutBreak/Public/AI/Components/EnemyMovementComponent.h`
- `Source/OutBreak/Private/AI/Components/EnemyMovementComponent.cpp`
- `Source/OutBreak/Private/AI/Nav/EnemyGenNavLinksProxy.cpp`
- `Documentation/EnemyDropTraversalMovementStopAudit.md`

## 10. 수정 전후 코드 흐름

수정 전:

```text
OnLinkMoveStarted
-> StartNavLinkTraversal
-> StopMovementImmediately
-> bIsTraversingNavLink = true
-> TickTraversalDrop
-> 착지 복구
-> FinishUsingCustomLink 호출 없음
```

수정 후:

```text
OnLinkMoveStarted
-> StartNavLinkTraversal
-> 같은 PathFollowing/CustomLink 중복이면 return
-> 최초 진입일 때만 StopMovementImmediately
-> bIsTraversingNavLink = true
-> TickTraversalDrop
-> 절벽 방향 AddInputVector
-> Falling 진입 시 래그돌 시작
-> 착지 복구
-> FinishNavLinkTraversal
-> UPathFollowingComponent::FinishUsingCustomLink
```

## 11. OnLinkMoveStarted 반환값 분석

UE 5.7 `UPathFollowingComponent::StartUsingCustomLink()`는 다음 의미로 반환값을 사용한다.

- `true`: custom move가 시작됐으며 외부에서 `FinishUsingCustomLink()`가 호출될 때까지 custom link 상태를 유지한다.
- `false`: 통지만 처리한 것으로 보고 `CurrentCustomLinkOb`를 비운다.

`UEnemyGenNavLinksProxy`는 custom movement를 `UEnemyMovementComponent`가 직접 처리하므로 `true` 반환이 맞다. `Super::OnLinkMoveStarted()`는 `UBaseGeneratedNavLinksProxy` 기반에서는 유효한 처리 흐름을 추가하지 않으므로 제거했다.

## 12. PathFollowing과 CharacterMovement의 권한 분리

Drop traversal 중 실제 이동 입력은 `UEnemyMovementComponent::TickTraversalDrop()`의 `AddInputVector(Direction, true)`가 담당한다.

수정 후 `bIsTraversingNavLink`가 true일 때 PathFollowing/Crowd에서 들어오는 `RequestPathMove()`와 `RequestDirectMove()`는 무시한다. 이로써 custom link 이동 중 PathFollowing과 사용자 movement가 동시에 속도/입력을 제어하지 않는다.

## 13. MoveTo 반복 요청 여부

프로젝트 C++ 검색 결과 `AEnemyController`나 관련 C++ tick/service에서 Drop traversal 중 매 프레임 MoveTo를 재요청하는 코드는 발견되지 않았다. Behavior Tree asset 내부 Blueprint 설정은 이 환경에서 실행 검증하지 못했으므로 남은 확인 항목으로 둔다.

## 14. 중복 링크 진입 방어

`StartNavLinkTraversal()` 초기에 다음 조건을 추가했다.

```text
bIsTraversingNavLink == true
ActivePathFollowing == PathFollowing
ActiveCustomLink == EnemyGenNavLinksProxy
```

조건이 맞으면 같은 링크에 대한 중복 시작 콜백으로 보고 즉시 return한다. 다른 링크/PathFollowing이면 기존 traversal을 정리한 뒤 새 traversal을 시작한다.

## 15. Drop 접근 속도 검증 결과

빌드 검증은 성공했다. PIE 런타임에서 34, 68, 102처럼 속도가 누적 증가하는 로그는 이 작업 환경에서 직접 캡처하지 못했다.

수정 후 구조상 같은 링크 중복 시작 콜백은 `StopMovementImmediately()`를 다시 호출하지 않으므로, 기존의 `Velocity 0 -> 입력 적용 -> StopMovementImmediately -> Velocity 0` 반복은 제거된다.

## 16. Falling 및 래그돌 전환 검증

Drop 진입 즉시 `MOVE_Falling`으로 강제 전환하지 않는 구조를 유지했다. 절벽 방향 입력으로 캡슐이 바닥을 벗어난 뒤 `UCharacterMovementComponent`가 Falling을 감지한다.

기존 래그돌 시작/복구 구조는 보존했다.

- Falling 진입 시 `LatestRelativeMeshTransform` 저장
- Mesh physics/collision 활성화
- Capsule `ECC_Pawn` 응답 ignore
- 착지 후 Mesh physics/collision 해제
- Mesh를 Capsule에 재부착하고 relative transform 복구
- Capsule `ECC_Pawn` block 복구

## 17. FinishUsingCustomLink 종료 검증

착지 복구 분기에서 `FinishNavLinkTraversal()`을 호출하도록 수정했다. 이 함수는 active pointer를 로컬에 보관한 뒤 내부 상태를 먼저 비우고, 유효한 경우 한 번만 `PathFollowing->FinishUsingCustomLink(CustomLink)`를 호출한다.

## 18. 다중 좀비 테스트 결과

소스 구조상 `ActivePathFollowing`과 `ActiveCustomLink`는 각 `UEnemyMovementComponent` 인스턴스에 저장되므로 캐릭터별로 독립적이다.

100마리 동시 런타임 테스트는 이 작업 환경에서 수행하지 못했다. 다만 Shipping 빌드에서 과도한 스택 로그는 추가하지 않았고, 남긴 로그는 `LogEnemyDropTraversal`의 `VeryVerbose` 레벨이다.

## 19. 알려진 한계

- Vault, Mantle, ClimbUp tick 함수는 아직 비어 있다. Drop 외 traversal link가 들어오면 별도 구현이 필요하다.
- Behavior Tree asset 내부의 Blueprint `AI MoveTo` 반복 설정은 C++ 검색만으로는 완전히 배제할 수 없다.
- 실제 PIE에서 다중 좀비와 낙하/착지 타이밍을 눈으로 검증하는 단계는 별도로 필요하다.

## 20. 향후 개선 사항

- Drop traversal 전용 automation 또는 functional test 추가
- `StartNavLinkTraversal()`에 traversal type별 timeout/failsafe 추가
- Behavior Tree/Blueprint MoveTo 요청에 `IsTraversingNavLink()` guard 적용 여부 점검
- `LogEnemyDropTraversal VeryVerbose`를 활용한 PIE 검증 로그 샘플 수집
- Vault, Mantle, ClimbUp 구현 전까지 해당 link type의 즉시 완료 또는 명시적 오류 처리 정책 결정

## 빌드 결과

다음 명령으로 전체 C++ 빌드를 수행했다.

```text
C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat OutBreakEditor Win64 Development -Project="C:\Users\Admin\Documents\Unreal Projects\OutBreak\OutBreak.uproject" -WaitMutex -NoHotReload
```

결과:

```text
Result: Succeeded
```
