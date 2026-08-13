# Enemy Hit React StateTree 설정 가이드

## 적용 대상

- 실제 사용 자산: `/Game/Enemy/Blueprint/ST_EnemyAI_Advanced`
- 참고: `BP_EnemyController`는 현재 `ST_EnemyAI_Advanced`를 참조한다.
- C++ Evaluator: `Evaluate Enemy AI Context`
- 새 Evaluator 출력: `ActionState` (`Active`, `Stunned`, `Knockdown`, `Dead`)

코드 안전 게이트가 `Stunned` 동안 이동 요청과 공격 Montage 실행을 차단한다. `Stunned`는 다리 피격과 Crawling 물리 리액트에만 적용한다. 머리·몸통·양팔 피격은 `Upper Body` Slot으로 재생하며 `ActionState == Active`를 유지하므로 기존 Locomotion 이동이 계속된다.

## 1. C++ 변경 반영

1. Unreal Editor가 열려 있었다면 종료한 뒤 새 빌드 DLL로 다시 연다.
2. Content Browser에서 `/Game/Enemy/Blueprint/ST_EnemyAI_Advanced`를 연다.
3. `Evaluate Enemy AI Context` Evaluator를 선택한다.
4. `Output`에 `ActionState`가 보이는지 확인한다.
5. 보이지 않으면 StateTree 창에서 Compile을 누르거나 자산을 닫았다 다시 연다.

## 2. Stunned State 추가

1. 모든 행동 State가 공유하는 최상위 Root 아래에 새 State를 추가한다.
2. 이름을 `Stunned`로 지정한다.
3. 기존 `Combat`, `Idle` 등 정상 행동 분기와 같은 레벨의 자식으로 둔다.
4. `Stunned`에 `Delay Task`를 추가한다.
5. `Delay Task`의 `Run Forever`를 켠다.

빈 State는 즉시 완료될 수 있으므로 `Delay Task / Run Forever`가 필요하다. 잠금 종료는 Delay 시간이 아니라 아래 `ActionState == Active` 전환이 결정한다.

## 3. Stunned 진입 전환

Root State에 다음 Transition을 추가한다. Root는 정상 행동 자식 State가 무엇이든 활성 경로에 남으므로 이 전환이 공통 진입점이 된다.

| 항목 | 설정값 |
|---|---|
| Trigger | `On Tick` |
| Target | `Stunned` |
| Priority | `High` |
| Condition | `Enum Compare` |
| Left | `Evaluate Enemy AI Context.ActionState`에 Binding |
| Operator | `Equal` |
| Right | `EEnemyActionState::Stunned` |

프로젝트에 이미 공통 `Dead` 전환이 있다면 `Dead`의 Priority는 `Critical`, `Stunned`는 `High`로 둔다. 죽은 적이 Stunned로 돌아가면 안 된다.

## 4. Stunned 해제 전환

`Stunned` State에 다음 Transition을 추가한다.

| 항목 | 설정값 |
|---|---|
| Trigger | `On Tick` |
| Target | 정상 분기를 다시 선택하는 상위 Root State |
| Priority | `High` |
| Condition | `Enum Compare` |
| Left | `Evaluate Enemy AI Context.ActionState`에 Binding |
| Operator | `Equal` |
| Right | `EEnemyActionState::Active` |

Target을 Root로 지정하면 현재 타깃/감각 조건에 따라 `Idle`, `Chase`, `Attack` 중 적합한 자식 State가 다시 선택된다. Root를 Target으로 고를 수 없는 트리 구조라면 기존의 정상 진입 허브 State를 Target으로 지정한다.

`Knockdown`을 나중에 구현할 예정이면 `Stunned` 해제 조건을 단순 `Not Equal Stunned`로 만들지 않는다. 반드시 `Active`일 때만 정상 분기로 복귀시켜 상태 우선순위를 보존한다.

## 5. Montage Slot 확인

StateTree 설정과 별도로 여섯 Hit React Montage가 실제 AnimBP 출력 경로와 같은 Slot을 사용해야 한다.

1. `DA_Enemy`의 Head, Spine, 양쪽 Shoulder, 양쪽 Leg Montage를 각각 연다.
2. Head, Spine, Left/Right Shoulder Montage의 Slot을 `Upper Body`로 설정한다.
3. Left/Right Leg Montage는 하체 반응에 필요한 기존 전신 Slot을 유지한다.
4. Enemy AnimBP에서 Locomotion State Machine 출력에 `Layered Blend per Bone`을 사용해 `Upper Body` Slot을 상체 뼈부터 합성한다. 시작 뼈는 Skeleton 구조에 맞춰 `spine_01` 또는 `spine_02`를 사용한다.
5. `Upper Body` 분기의 Mesh Space Rotation Blend를 켜서 이동 중 상체 회전이 튀지 않는지 확인한다.

Montage가 재생되지 않는데 물리 Impulse만 보인다면 가장 먼저 Slot 불일치를 확인한다.

## 6. DA_Enemy 권장값

`DA_Enemy > Physical React > Hit React`에서 다음 초기값으로 시작한다.

| 필드 | 권장 시작값 |
|---|---:|
| `Hit React Play Rate` | `1.0` |
| `Fallback Lock Duration` | `0.45 s` (Montage 재생 실패 시에만 사용) |
| `Hit React Montage Blend Out Time` | `0.08 s` |
| `Restart On Repeated Hit` | `true` |

기존 `React Scale`, `Blend Weight Anim Physics`, `React Curve Float`는 연출 강도와 물리 혼합을 제어한다. `React Scale`이 0이면 Montage는 재생되지만 Impulse는 발생하지 않는다.

머리·몸통·양팔 피격은 `Stunned`를 적용하지 않으므로 Montage 재생 중에도 이동한다. 양다리 피격은 서버의 Montage End Delegate가 호출될 때까지 `Stunned`가 유지된다. Crawling/SlowCrawling은 Hit React Montage를 재생하지 않고 물리 Timeline이 끝날 때까지 이동 잠금을 유지한다. `Fallback Lock Duration`은 정상 종료 callback이 유실되는 예외 상황용 안전값이다.

## 7. 검증 순서

1. PIE에서 한 부위씩 Head, Spine, 좌우 Arm, 좌우 Leg를 피격한다.
2. Head, Spine, 좌우 Arm 피격 중 `ActionState == Active`가 유지되고 이동이 계속되는지 본다.
3. 좌우 Leg 피격에서는 `Active -> Stunned -> Active`로 변하고, Montage 종료 직후 이동이 재개되는지 확인한다.
4. 피격 직전 공격 중이었다면 공격 Montage가 중단되고 Hit React Montage가 재생되는지 확인한다.
5. Hit React Montage와 해당 뼈 아래 Physics Impulse가 동시에 섞이는지 확인한다.
6. `Restart On Repeated Hit`가 켜진 상태에서 연속 피격 시 기존 물리 연출이 중첩되지 않고 새 부위 Montage로 교체되는지 확인한다. 상체 반응으로 교체되면 기존 다리 피격 잠금도 즉시 해제되어야 한다.
7. 치명타, 절단 사망, 풀 반환 후 재활성화에서도 Montage, Timer, Physics가 남지 않는지 확인한다.
8. Hit React Montage 재생 중 다리를 절단하면 Montage가 즉시 중단되고 Crawling으로 전환되는지 확인한다.
9. Crawling 상태에서 다시 피격하면 Hit React Montage 없이 부분 Physics·Impulse만 실행되고, Physics Timeline 종료 시 `ActionState`가 `Active`로 복귀하는지 확인한다.

## 최종 형태

```text
Root
|- Transition: On Tick + ActionState == Stunned -> Stunned (High)
|- Stunned
|  |- Task: Delay Task (Run Forever)
|  `- Transition: On Tick + ActionState == Active -> Root (High)
`- 기존 정상 행동 분기
   |- Idle
   |- Chase
   `- Combat / Attack
```

Dead 공통 전환이 있다면 다음 우선순위를 사용한다.

```text
Dead     : Critical
Stunned  : High
Normal   : Normal 이하
```
