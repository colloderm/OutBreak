# 사격 이펙트 소실 회귀 상세 분석 보고서

- 작성일: 2026-08-10
- 조사 대상: 헬기 레펠 종료 후 플레이어 원거리 무기 사격
- 증상: 발사 입력 시 카메라 쉐이크와 반동만 발생하고 총구 화염, 발사음, 사격 몽타주, 탄착 이펙트 및 피해가 발생하지 않음
- 결론 상태: 원인 확정 및 기능 수정 완료
- 구현 검증: Editor 빌드 성공, 논리 볼륨 자동화 테스트 2/2 성공, `OutBreak_Exterior` 배치 인스턴스 런타임 충돌 정책 확인

## 1. 결론

사격 이펙트 애셋이 삭제되거나 연결이 끊긴 문제가 아니다. `GC_Weapon_Fire.uasset`은 그대로 존재하며 이 파일은 현재 작업 트리에서 수정되지 않았다.

직접 원인은 카메라 기준 사격을 구현하면서 새로 추가한 서버 카메라 원점 검증 트레이스가 `OBHelicopterInsertionAreaVolume`을 벽으로 판정한 것이다. 이 검증은 총구 화염·발사음 Gameplay Cue, 사격 몽타주, 실제 탄환 트레이스 및 피해보다 먼저 실행되고 실패 시 즉시 `return`한다.

최신 실행 로그에서 사격 시도 54회 모두 다음과 같이 처리됐다.

| 항목 | 횟수 |
|---|---:|
| 로컬 카메라 뷰 획득 성공 | 54 |
| 서버 카메라 원점 가림 판정 | 54 |
| 서버 조준 승인 | 0 |
| 잘못된 TargetData 판정 | 0 |
| 거리·각도 초과 판정 | 0 |

54회의 가림 판정에서 충돌한 액터는 전부 다음 하나였다.

`OBHelicopterInsertionAreaVolume_UAID_D843AE82EAF79BF602_2004238613`

따라서 현재 증상은 재현 조건에 따른 추정이 아니라 실행 로그로 확정된 회귀다.

## 2. 무엇을 변경했고 왜 회귀가 생겼는가

### 2.1 변경 전

변경 전 사격 순서는 단순했다.

1. 발사 어빌리티가 서버에서 활성화된다.
2. 서버가 탄약을 소모한다.
3. 서버가 `PerformServerWeaponTrace()`를 즉시 호출한다.
4. 함수 안에서 총구 Gameplay Cue, 몽타주, 탄환 트레이스와 피해를 실행한다.

이 구조에는 별도의 클라이언트 카메라 TargetData 검증 단계가 없었다. 대신 사용되지 않는 네이티브 `FollowCamera`를 서버 조준 기준으로 사용해 화면 중앙과 탄착점이 불일치하는 문제가 있었다.

### 2.2 변경 후

화면에 실제로 렌더링되는 Gameplay Cameras 뷰를 사격에 사용하기 위해 다음 구조로 바꿨다.

1. 소유 클라이언트가 `GetPlayerViewPoint()`로 실제 뷰 위치와 회전을 획득한다.
2. 그 값을 GAS TargetData로 서버에 전달한다.
3. 서버가 거리, 회전 각도, 카메라 원점 가림 여부를 검증한다.
4. 검증에 통과해야만 총구 Gameplay Cue, 몽타주, 탄환 트레이스와 피해를 실행한다.

카메라 기준 탄착 문제를 바로잡으려는 방향 자체와 달리, 발사 연출과 피해 전체를 검증 함수 뒤에 묶은 것이 구조적 실수였다. 여기에 착륙 가능 영역을 나타내는 볼륨이 `Weapon` 채널을 차단하는 충돌 설정까지 겹쳐 모든 사격이 검증 단계에서 종료됐다.

## 3. 실제 실행 순서와 증상 대응

현재 `FireOneShot()`의 순서는 다음과 같다.

```text
발사 입력
  -> 로컬 카메라 TargetData 생성
  -> 서버 탄약 1발 소모
  -> 소유 클라이언트 NotifyFired / 반동 / 카메라 쉐이크
  -> TargetData를 서버 처리 함수에 전달
  -> 서버 카메라 원점 검증
       -> OBHelicopterInsertionAreaVolume 충돌
       -> 즉시 return
  -> 아래 단계는 실행되지 않음
       - GameplayCue.Weapon.Fire
       - 총구 화염
       - 발사음
       - 사격 몽타주
       - 탄환 트레이스
       - 충돌 이펙트
       - 피해
```

카메라 쉐이크가 보이는 이유는 `FireOneShot()`의 로컬 예측 구간에서 먼저 실행되기 때문이다.

- 현재 구현 기준 `OBGameplayAbility_RangedWeapon.cpp:268` 부근: 로컬 예측 반동과 카메라 쉐이크
- 현재 구현 기준 `OBGameplayAbility_RangedWeapon.cpp:559` 부근: 서버 카메라 원점 가림 검증
- 현재 구현 기준 `OBGameplayAbility_RangedWeapon.cpp:642` 부근: 승인 뒤의 `GameplayCue.Weapon.Fire`
- 현재 구현 기준 `OBGameplayAbility_RangedWeapon.cpp:649` 부근: 승인 뒤의 사격 몽타주
- 현재 구현 기준 `OBGameplayAbility_RangedWeapon.cpp:661` 이후: 승인 뒤의 탄자·피해 처리

즉, 카메라 쉐이크만 나온다는 현상은 현재 코드의 실행 순서와 정확히 일치한다.

## 4. 로그 증거

조사 로그는 `Saved/Logs/OutBreak.log`이며, 실제 사격 구간은 2026-08-10 15:40:41부터 15:41:11까지다. 로그의 UTC 표기는 06:40:41부터 06:41:11이다.

첫 사격:

```text
[WeaponAim] Local view captured
Character=BP_SandboxCharacter_Player_C_0
ViewTarget=BP_SandboxCharacter_Player_C_0
Origin=(-82073.01, -23253.38, 2158.43)
PawnOffset=219.8

[WeaponAim] Rejected camera origin behind obstruction
PawnView=(-81867.35, -23207.44, 2195.93)
Origin=(-82073.01, -23253.38, 2158.43)
Hit=(-81867.35, -23207.44, 2195.93)
Actor=OBHelicopterInsertionAreaVolume_UAID_D843AE82EAF79BF602_2004238613
```

마지막 사격:

```text
[WeaponAim] Local view captured
Origin=(-80752.84, -18757.48, 2248.01)
PawnOffset=181.0

[WeaponAim] Rejected camera origin behind obstruction
PawnView=(-80822.40, -18857.14, 2213.83)
Origin=(-80752.84, -18757.48, 2248.01)
Hit=(-80822.40, -18857.14, 2213.83)
Actor=OBHelicopterInsertionAreaVolume_UAID_D843AE82EAF79BF602_2004238613
```

핵심 관찰:

- `ViewTarget`은 플레이어 캐릭터이므로 카메라 뷰 획득 실패가 아니다.
- 카메라와 폰의 거리는 약 175~321 cm로 최대 허용 거리 1200 cm 이내다.
- 잘못된 TargetData나 회전 각도 오류 로그가 없다.
- `Server view accepted` 로그는 단 한 번도 없다.
- 가림 트레이스의 충돌 위치가 대부분 `PawnView` 시작점과 동일하다. 플레이어가 볼륨 내부에 있어 트레이스 시작과 동시에 볼륨 브러시에 맞았다는 의미다.

## 5. 충돌 설정 측 원인

`OB_TraceChannel_Weapon`은 `ECC_GameTraceChannel2`이며 `DefaultEngine.ini`에서 이름 `Weapon`, 기본 응답 `Block`으로 정의돼 있다.

`AOBHelicopterInsertionAreaVolume`은 현재 `AVolume`을 상속하고 `bAllowInsertion` 값만 선언한다. 생성자에서 브러시 컴포넌트의 충돌을 끄거나 `Weapon` 채널을 `Ignore`하도록 강제하지 않는다.

그 결과 레벨에 배치된 투입 허용 볼륨의 브러시가 `Weapon` 트레이스에 응답한다. 투입 영역은 논리적인 점 포함 검사에만 사용돼야 하지만, 현재는 사격과 카메라 검증 모두에 물리적 벽처럼 관여한다.

이 문제는 카메라 검증에만 한정되지 않는다. 동일 볼륨이 계속 `Weapon` 채널을 막으면 검증을 통과시킨 뒤 실제 카메라 조준 트레이스나 탄환 트레이스도 볼륨에 맞을 수 있다. 따라서 검증 코드에서 이 액터 하나만 무시하는 임시 처리는 완전한 해결이 아니다.

## 6. 설계상 두 번째 원인: 발사 트랜잭션 분리 실패

현재 한 발의 상태 변경이 서로 다른 단계에 흩어져 있다.

| 처리 | 현재 위치 | 검증 실패 시 결과 |
|---|---|---|
| 탄약 소모 | 서버 `FireOneShot()` | 이미 소모됨 |
| 반동·카메라 쉐이크 | 소유 클라이언트 `FireOneShot()` | 이미 표시됨 |
| 총구 화염·발사음 | 서버 검증 뒤 | 표시 안 됨 |
| 사격 몽타주 | 서버 검증 뒤 | 표시 안 됨 |
| 탄환·피해 | 서버 검증 뒤 | 실행 안 됨 |

이 구조에서는 TargetData가 누락되거나 어떤 검증이 실패해도 탄약과 카메라 쉐이크만 발생하는 반쪽 발사가 만들어진다. 이번 볼륨 충돌은 이 잠재 결함을 항상 재현되게 만든 조건일 뿐이다.

또한 코드 주석은 발사 큐와 몽타주가 “명중 여부와 무관”하다고 설명하지만, 실제로는 조준 검증 성공 여부에 종속된다. 주석과 실행 의미도 일치하지 않는다.

## 7. 애셋 상태

다음 이펙트 관련 애셋은 프로젝트에 존재한다.

- `Content/GameAbilitySystem/Cues/GC_Weapon_Fire.uasset`
- `Content/GameAbilitySystem/Cues/GC_Weapon_Impact.uasset`
- `Content/Weapons/Effects/ParticleEffects/NS_AssaultRifle_MuzzleFlash.uasset`
- `Content/GameAbilitySystem/CameraShake/CS_WeaponFire.uasset`

`GC_Weapon_Fire.uasset`은 현재 Git 변경 목록에 없다. 따라서 이번 증상의 직접 원인은 이펙트 애셋 삭제, 블루프린트 파라미터 해제 또는 Gameplay Tag 제거가 아니다. `GameplayCue.Weapon.Fire`를 실행하는 코드에 도달하지 못한 것이 원인이다.

## 8. 책임 범위와 검증 누락

이 회귀는 카메라 기준 사격을 수정하는 과정에서 추가한 코드로 발생했다.

구체적인 잘못은 다음 세 가지다.

1. 투입 허용 볼륨의 충돌 응답을 확인하지 않고 `Weapon` 채널을 카메라 원점 검증에도 재사용했다.
2. 검증 실패보다 앞서 탄약과 로컬 쉐이크를 실행하면서, 발사 Cue와 몽타주를 검증 뒤에 배치했다.
3. 컴파일 성공만 확인하고 실제 사격 입력을 통한 런타임 검증을 완료하지 않았다.

특히 세 번째가 배포 전 발견하지 못한 직접적인 과정상 문제다. 당시 자동 실행 검증에서는 게임 진입과 카메라 복구까지만 확인됐고 실제 발사 입력은 발생하지 않았다. 빌드 성공은 이 종류의 충돌 채널·Gameplay Cue 회귀를 검증하지 못한다.

## 9. 권장 복구안

### 9.1 1단계: 논리 볼륨과 무기 충돌 분리

`AOBHelicopterInsertionAreaVolume`은 착륙 후보가 볼륨 안에 있는지 확인하는 논리 데이터다. 생성자에서 브러시 충돌을 비활성화하거나, 최소한 `Weapon`, `CameraProbe`, `Visibility`, `Camera`, `Pawn`에 대해 물리적 차단을 하지 않도록 클래스 기본값을 고정해야 한다.

권장 우선순위는 다음과 같다.

1. 볼륨 브러시를 `NoCollision`로 설정한다.
2. `EncompassesPoint` 기반 착륙 가능 영역 검사가 그대로 동작하는지 확인한다.
3. 만약 에디터 시각화 때문에 Query가 필요하면 전용 오브젝트/프로필을 만들고 게임플레이 트레이스 채널은 전부 `Ignore`한다.

레벨 인스턴스에서 수동으로 채널 하나만 바꾸는 방식은 다른 맵과 새 인스턴스에서 다시 회귀하므로 클래스 기본값으로 강제해야 한다.

### 9.2 2단계: 카메라 가림 검증 채널 교정

카메라 원점 검증은 실제 엄폐물만 대상으로 해야 한다. 트리거, 투입 허용 볼륨, 탈출 볼륨, 오디오 볼륨 같은 논리 볼륨을 대상으로 하면 안 된다.

가능한 방식:

- 전용 `AimValidation` 트레이스 채널을 만들고 월드 지오메트리만 `Block`한다.
- 또는 오브젝트 쿼리로 `WorldStatic`/필요한 `WorldDynamic`만 조회한다.
- 무기 탄환용 `Weapon` 채널을 그대로 쓰더라도 모든 논리 볼륨 클래스가 이를 `Ignore`하도록 공통 정책을 둔다.

특정 액터 이름이나 현재 배치 인스턴스만 `AddIgnoredActor` 하는 처리는 금지한다. 새 볼륨이나 다른 맵에서 재발한다.

### 9.3 3단계: 한 발을 원자적인 서버 트랜잭션으로 처리

권장 최종 순서:

```text
클라이언트 발사 입력
  -> 실제 PlayerCameraManager 뷰 캡처
  -> GAS TargetData 전달
  -> 서버 TargetData 형식·거리·각도·엄폐 검증
  -> 검증 성공 시 한 함수에서 원자적으로 처리
       1. 발사 가능/탄약 재확인
       2. 탄약 소모
       3. GameplayCue.Weapon.Fire 실행
       4. 사격 몽타주 복제
       5. 머즐 기준 탄환 트레이스
       6. 충돌 Cue와 피해 적용
  -> 검증 실패 시
       - 탄약을 소모하지 않음
       - 서버 발사 Cue를 내지 않음
       - 명확한 거부 로그와 클라이언트 예측 취소
```

이렇게 하면 “탄약과 쉐이크만 발생”하는 중간 상태가 없어지고, 승인된 한 발의 연출과 피해가 항상 함께 처리된다.

### 9.4 클라이언트 예측 연출

입력 반응성을 위해 로컬 카메라 쉐이크와 총구 이펙트를 예측 실행할 수는 있다. 이 경우 GAS Prediction Key로 서버 Cue와 중복되지 않게 조정하고, 서버 거부 시 예측 취소 경로가 있어야 한다.

단기적으로 예측 Cue를 추가하는 것만으로 현재 서버 거부를 가리면 안 된다. 화면에는 총이 쏘지만 탄약·피해·서버 상태가 불일치하는 더 위험한 문제가 된다.

## 10. 적용 시 주의할 사항

- `OBHelicopterInsertionAreaVolume`의 충돌을 끈 뒤에도 `EncompassesPoint()` 착륙 검사와 World Partition 로딩이 정상인지 확인해야 한다.
- 카메라 원점 검증은 3인칭 카메라의 정상적인 벽 충돌·카메라 랙을 허용해야 한다.
- 실제 총알 시작점은 계속 머즐이어야 하며, 카메라는 조준점 산출에만 사용해야 한다.
- 카메라와 머즐 사이에 벽이 있으면 머즐/몸통 기준으로 벽에 막혀야 한다.
- 단발, 점사, 연사는 발당 TargetData와 서버 승인 토큰이 정확히 1:1이어야 한다.
- TargetData 타임아웃이나 어빌리티 취소 시 대기 토큰과 델리게이트를 정리해야 한다.
- 투입 직후뿐 아니라 일반 스폰, 실내, 엄폐물 근접, 맵 경계에서도 검증해야 한다.

## 11. 필수 회귀 테스트

| 환경 | 조건 | 기대 결과 |
|---|---|---|
| Standalone | 투입 볼륨 내부 일반 사격 | 총구·음향·몽타주·탄착·피해 정상 |
| Standalone | 레펠 직후 사격 | 카메라 중앙 조준, 연출과 피해 정상 |
| Standalone | 실제 벽 뒤로 카메라 이동 | 벽 관통 피해 없음, 상태 불일치 없음 |
| Listen Server | 호스트 사격 | 1회 발사당 Cue/몽타주 1회 |
| Listen Server | 원격 클라이언트 사격 | TargetData 승인 후 모든 클라이언트에 정상 복제 |
| Dedicated Server | 원격 클라이언트 사격 | Prediction Key와 TargetData 수명 정상 |
| 모든 환경 | 단발·점사·연사 | 발당 탄약, Cue, 트레이스 수 일치 |
| 모든 환경 | 투입 볼륨 경계 통과 중 사격 | 볼륨이 무기 트레이스를 차단하지 않음 |
| 모든 환경 | 탄약 0 | 쉐이크·Cue·피해 모두 발생하지 않음 |

로그 승인 기준:

- 정상 발사마다 `Local view captured` 1회
- 정상 발사마다 `Server view accepted` 1회
- 투입 허용 볼륨을 Actor로 표시하는 `Rejected camera origin behind obstruction` 0회
- 승인된 발사 수, 탄약 감소량, `GameplayCue.Weapon.Fire` 실행 수가 동일

## 12. 수정 대상 파일

복구 작업 시 최소 검토 대상은 다음과 같다.

- `Source/OutBreak/Public/Game/Expedition/OBHelicopterInsertionAreaVolume.h`
  - 논리 볼륨의 충돌 정책 고정
- 필요 시 `Source/OutBreak/Private/Game/Expedition/OBHelicopterInsertionAreaVolume.cpp`
  - 생성자에서 BrushComponent 충돌 설정
- `Source/OutBreak/Private/Ability/Abilities/OBGameplayAbility_RangedWeapon.cpp`
  - 카메라 검증 채널 교정
  - 탄약·Cue·몽타주·탄환·피해 처리의 원자화
  - 검증 실패/타임아웃 처리
- `Source/OutBreak/Public/Ability/Abilities/OBGameplayAbility_RangedWeapon.h`
  - 검증 설정과 대기 상태 정리
- `Config/DefaultEngine.ini`
  - 전용 검증 채널 또는 논리 볼륨용 충돌 프로필이 필요할 경우 추가

## 13. 최종 판단

현재 사격 이펙트가 사라진 것은 애셋 문제가 아니라 코드 실행 경로가 서버 검증에서 100% 차단됐기 때문이다. 차단한 대상은 헬기 투입 가능 영역을 정의하기 위해 새로 배치한 논리 볼륨이며, 이 볼륨을 실제 벽으로 취급한 충돌 정책과 발사 연출 전체를 검증 뒤에 둔 실행 순서가 결합해 회귀를 만들었다.

수정은 단순히 `return` 하나를 제거하는 방식으로 끝내면 안 된다. 논리 볼륨을 무기 충돌에서 완전히 분리하고, 승인된 한 발의 탄약·연출·피해를 하나의 서버 트랜잭션으로 묶은 뒤 네트워크 환경별 실제 발사 테스트를 통과시켜야 한다.

## 14. 2026-08-10 구현 결과

보고서의 복구안을 다음과 같이 코드에 반영했다.

### 14.1 논리 볼륨 충돌 분리

- `AOBHelicopterInsertionAreaVolume`과 `AOBHelicopterExclusionVolume`에 명시적인 생성자와 `PostInitializeComponents()` 후처리를 추가했다.
- BrushComponent는 `QueryOnly`로 유지해 `EncompassesPoint()`에 필요한 형상은 보존한다.
- 모든 충돌 채널 응답을 `Ignore`로 강제한다.
- 오버랩 이벤트와 내비게이션 영향은 비활성화한다.
- 레벨 또는 Blueprint에 직렬화된 예전 충돌 프로필이 있어도 컴포넌트 초기화 뒤 정책을 다시 적용한다.
- `Weapon`과 `CameraProbe`가 `Ignore`가 아니면 `ensureAlwaysMsgf`로 즉시 탐지한다.

`OutBreak_Exterior` 무인 런타임에서 실제 배치 인스턴스에 다음 로그가 확인됐다.

```text
[InsertionArea] Logical collision enforced
Volume=OBHelicopterInsertionAreaVolume_UAID_D843AE82EAF79BF602_2004238613
Collision=ECollisionEnabled::QueryOnly
Weapon=0
CameraProbe=0
```

여기서 응답값 `0`은 `ECR_Ignore`다.

### 14.2 카메라 검증 채널 분리

카메라 원점 가림 검증은 탄환용 `Weapon` 채널 대신 카메라 충돌 전용 `CameraProbe` 채널을 사용하도록 변경했다. 실제 조준점과 탄환 트레이스는 계속 `Weapon` 채널을 사용한다.

### 14.3 승인형 서버 사격 트랜잭션

기존 `PerformServerWeaponTrace()`를 `CommitServerShot()`으로 바꾸고 책임을 명확히 했다.

현재 서버 순서:

```text
서버 발사 요청 토큰 예약
  -> TargetData 수신
  -> 형식·거리·각도·카메라 가림 검증
  -> 조준점과 머즐 시작점 산출
  -> 탄약/ASC 최종 재확인
  -> 탄약 소모
  -> GameplayCue.Weapon.Fire
  -> 사격 몽타주 복제
  -> 탄환·탄착 Cue·피해 처리
```

검증에 실패하면 탄약을 소모하지 않는다. 따라서 이전처럼 “탄약과 쉐이크만 발생하고 서버 발사 전체가 사라지는” 반쪽 서버 발사는 만들어지지 않는다. 로컬 카메라 쉐이크는 입력 반응성을 위한 예측 연출로 유지한다.

서버가 한 발을 실제 커밋하면 다음 로그가 남는다.

```text
[WeaponFire] Authoritative shot committed
Character=<Character>
Weapon=<Weapon>
Ammo=<Before>-><After>
FireCue=GameplayCue.Weapon.Fire
Montage=<ResolvedMontage>
```

### 14.4 TargetData 수명과 탄약 예약

- 대기 중인 서버 사격 요청 수를 탄약 예약으로 계산해 네트워크 지연 중 탄창보다 많은 발이 큐에 들어가지 않게 했다.
- 로컬 카메라 뷰를 만들지 못하면 해당 예측 발사를 즉시 취소한다.
- 원격 서버 인스턴스가 TargetData를 받지 못하면 기본 1초 뒤 취소한다.
- 타임아웃된 요청은 아직 탄약을 소모하지 않았으므로 환불이 필요 없다.
- 능력 종료 시 발사 반복 타이머, TargetData 타이머, 델리게이트와 대기 토큰을 모두 정리한다.

### 14.5 자동화 및 빌드 검증

추가한 자동화 테스트:

- `OutBreak.Expedition.LogicalVolumes.InsertionAreaCollisionContract`
- `OutBreak.Expedition.LogicalVolumes.ExclusionCollisionContract`

결과:

| 검증 | 결과 |
|---|---|
| OutBreakEditor Win64 Development 빌드 | 성공 |
| 논리 볼륨 자동화 테스트 | 2 성공 / 0 실패 |
| `OutBreak_Exterior` 실제 배치 볼륨 런타임 정책 | QueryOnly, Weapon Ignore, CameraProbe Ignore 확인 |

무인 런타임은 헬기 탑승 상태에서 종료되므로 실제 사용자 발사 입력과 Niagara/음향의 화면·청각 확인까지 자동화하지는 못했다. 최종 수동 확인에서는 레펠 직후 발사 후 `Server view accepted`와 `Authoritative shot committed`가 발당 한 번씩 기록되는지 확인하면 된다.

무인 실행 중 기존 프로젝트/엔진 애셋의 별도 오류도 관찰됐다.

- `FEnemyPhysicalReact::ReactScale` 기본값 미초기화
- UE 5.7 Landmass 실험 플러그인의 Blueprint 핀/구조체 오류
- 종료 과정의 Steam worker thread assertion

이 항목들은 이번 사격·볼륨 변경 파일과 무관하며, 엔진 로그상 게임 월드 자체는 벤치마크 종료 요청으로 정상 정리됐다.
