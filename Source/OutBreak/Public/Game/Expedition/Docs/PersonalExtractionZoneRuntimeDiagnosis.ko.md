# OutBreak_Exterior 개인 탈출 구역 미동작 추적 보고서

작성일: 2026-08-10  
대상 레벨: `/Game/Maps/OutBreak_Exterior`  
대상 액터: `BP_ExtractionZone_Personal`

## 1. 결론

현재 레벨에 직접 배치된 `BP_ExtractionZone_Personal`이 발판처럼 밟아도 동작하지 않는 직접 원인은 `CallTrigger`의 실제 런타임 반경이 **32cm**이기 때문이다.

C++ 생성자에는 300cm가 지정되어 있지만, 기존 Blueprint 파생 에셋에 직렬화된 컴포넌트 값 32cm가 C++ 기본값을 덮어쓰고 있다. 캐릭터가 지면에 서 있을 때 액터 위치는 보통 캡슐 중심에 있고 지면보다 약 90cm 높으므로, 탈출 지점 정중앙에 서도 반경 32cm 구체와 캐릭터 Pawn이 겹치지 않는다. 따라서 `OnComponentBeginOverlap`이 호출되지 않고, 플레어 호출 로직까지 진입하지 않는다.

추가로 해당 액터는 개인 탈출구의 정상 생성 경로와 다르게 **레벨에 직접 배치**되어 있다. 런타임 팀 배정을 받지 않아 `OwningTeamId=0`, `RuntimeAssigned=false` 상태이며, 기존 공용 탈출구 수집 코드에는 공용 마커처럼 섞인다. 이는 32cm 문제와 별개의 구성 괴리다.

## 2. 런타임 확인 결과

최종 검증 로그는 `Saved/Logs/CodexExtractionPersonalDiagnosticFinal.log`에 있다.

| 항목 | Personal BP 실측값 | 판정 |
|---|---:|---|
| 위치 | `(-50312, 7324, 2010)` | 액터는 레벨에 로드됨 |
| 호출 단계 | `Ready` | 호출 가능한 단계 |
| 활성 시간 | `0..600초` | 테스트 시점에는 활성 |
| 충돌 활성 | `QueryOnly` | 정상 |
| Pawn 응답 | `Overlap` | 정상 |
| Generate Overlap Events | `true` | 정상 |
| CallTrigger 반경 | **32cm** | 치명적 구성 오류 |
| BoardingTrigger 반경 | 500cm | 정상 |
| ExtractType | `Personal` | 개인 탈출 유형 |
| OwningTeamId | `0` | 팀 미배정 |
| RuntimeAssigned | `false` | 정상 개인 탈출 생성 경로를 거치지 않음 |

동일 실행에서 `BP_ExtractionZone_Public`도 `CallTriggerRadius=32cm`로 확인됐다. 즉 레벨 인스턴스 하나만의 문제가 아니라, C++ 기본값 변경 전에 저장된 Blueprint 컴포넌트 기본값이 Personal/Public 파생 BP에 남아 있는 상태로 보는 것이 타당하다. 런타임에 `PersonalExtractClass`로 생성되는 개인 탈출구도 같은 Personal BP를 사용하므로 수정 전에는 동일한 32cm 반경을 상속할 가능성이 높다.

근거 로그:

- 4739행: Personal, Team 0, Ready, `CallTriggerRadius=32.0`
- 4741행: 32cm 반경으로 BeginOverlap이 발생하지 않을 수 있다는 명시적 오류
- 4742~4743행: Public BP도 동일하게 32cm
- 4746~4749행: 직접 배치 Personal이 기존 공용 지도 목록에 포함되어 총 2개로 수집됨
- 5281행: 검증 프로세스 정상 종료

## 3. 정상 개인 탈출구 생성 구조

개인 탈출구의 의도된 생성 순서는 다음과 같다.

1. 레벨에는 `AOBExtractionSite` 또는 `PersonalExtract` Actor Tag가 붙은 `ATargetPoint`를 배치한다.
2. 헬기 투입 위치가 확정되면 GameMode가 개인 탈출 마커 후보를 수집한다.
3. 팀별 후보 두 곳을 선택한다.
4. `PersonalExtractClass`로 `BP_ExtractionZone_Personal`을 Deferred Spawn한다.
5. `ConfigureAsPersonal(TeamId)`로 `OwningTeamId`와 Personal 유형을 지정한다.
6. 마커가 `AOBExtractionSite`이면 Landing/Flare Anchor와 Approach/Exit Route를 복사한다.
7. MapData의 개인 탈출 활성 시간을 주입한 뒤 Spawn을 완료한다.
8. 생성된 좌표만 해당 팀 PlayerState에 배포한다.

관련 구현은 `OBExpeditionGameMode.cpp`의 `CollectPersonalExtractPoints`와 `AssignPersonalExtractsFor`에 있다. 레벨에 `BP_ExtractionZone_Personal` 자체를 직접 놓는 것은 이 팀 배정 경로를 우회한다.

## 4. 호출이 시작되는 조건

플레어 호출은 입력 키 방식이 아니라 서버의 `CallTrigger.OnComponentBeginOverlap` 자동 호출 방식이다. 다음 조건을 모두 통과해야 한다.

- 서버 권한이어야 함
- Call Phase가 `Ready`여야 함
- 현재 시간이 `ActiveStartSec..ActiveEndSec` 범위여야 함
- 겹친 액터가 Controller를 가진 Pawn이어야 함
- PlayerState가 존재해야 함
- 플레이어 ExpeditionStatus가 `Alive`여야 함
- 개인 탈출구의 OwningTeamId가 0이 아니면 플레이어 TeamId와 같아야 함

이번 실측에서는 Phase, 활성 시간, 충돌 모드, Pawn 응답이 정상이다. 그러나 32cm 구체가 캐릭터 캡슐과 실제로 겹치지 않아 위 조건 검사 함수 자체까지 도달하지 않았다.

## 5. 추가된 진단 기능

### 5.1 콘솔 시각화

PIE 콘솔에서 다음 명령을 사용한다.

```text
ob.Extraction.Debug 1
```

- 호출/탑승 트리거 구체
- ExtractType과 OwningTeamId
- 현재 Call Phase
- 활성 시간과 활성 여부
- 로컬 플레이어 호출 가능 여부 또는 차단 사유
- 실제 Call/Boarding 반경

상세 모드는 다음과 같다.

```text
ob.Extraction.Debug 2
```

상세 모드에서는 Landing Anchor 좌표축, Flare Anchor, Approach Route, Exit Route까지 함께 그린다.

기본 표시 거리는 로컬 Pawn 기준 30,000cm이다. 전체 맵의 구역을 표시하려면 다음을 사용한다.

```text
ob.Extraction.DebugDistance 0
```

색상 의미:

- 빨강: 직접 배치 Personal / Team 0의 잘못된 구성
- 초록: 현재 로컬 플레이어가 호출 가능
- 노랑: 활성 상태지만 플레이어 조건 불충족
- 주황: 활성 시간 밖 또는 Expired
- 자홍: Flare/Waiting/Inbound/Boarding 등 호출 진행 중
- 청록 외곽 구체: 탑승 트리거

개별 BP 또는 레벨 인스턴스의 `Extraction > Debug > Draw Debug Visualization`을 켜도 콘솔 명령 없이 그릴 수 있다.

### 5.2 로그

`[ExtractionDebug]` 접두사로 다음 정보가 추가됐다.

- BeginPlay 시 유형, 팀, 위치, 단계, 활성 시간, 실제 반경, 충돌 상태
- Personal Team 0 직접 배치 경고
- CallTrigger 반경 100cm 미만 오류
- CallTrigger 진입 성공 또는 거절 사유
- 거절 사유: `NoController`, `NoOBPlayerState`, `PlayerStatus`, `TeamMismatch`, `InactiveWindow`, `Phase`
- 호출 시작 시 팀, 플레어 클래스, 헬기 지연 시간
- 플레어 Spawn 결과와 Anchor 위치
- 직접 배치 Personal이 기존 공용 지도 목록에 포함되는 구성 경고

## 6. BP에서 필요한 수정

이번 작업에서는 사용자가 편집 중인 uasset 값을 임의로 덮어쓰지 않았다. 다음 수정은 Blueprint에서 수행해야 한다.

1. `BP_ExtractionZone_Personal`을 연다.
2. 상속 컴포넌트 `CallTrigger`를 선택한다.
3. Shape의 Sphere Radius를 최소 C++ 기본값인 **300cm**로 복구한다.
4. 컴파일·저장한다.
5. `BP_ExtractionZone_Public`도 동일하게 300cm로 복구한다.
6. 레벨에 직접 배치한 `BP_ExtractionZone_Personal`은 제거한다.
7. 그 위치에 `OBExtractionSite`를 배치하고 `PersonalExtract` 태그를 유지한다. `AOBExtractionSite` 네이티브 생성자는 이 태그를 자동 추가한다.
8. GameMode BP의 `PersonalExtractClass`에 `BP_ExtractionZone_Personal`이 연결되어 있는지 확인한다.
9. 필요하면 `OBExtractionSite`의 LandingAnchor, FlareAnchor, ApproachRoute, ExitRoute를 연결한다.

## 7. 재검증 기준

수정 후 PIE에서 다음을 확인한다.

1. 콘솔에서 `ob.Extraction.Debug 2` 실행
2. 실제 팀 배정 Personal 구역의 텍스트가 `Team=1` 이상이며 `runtime team assignment`로 표시
3. CallRadius가 300으로 표시
4. 구체 진입 시 로그에 `Call trigger accepted`
5. 즉시 `Call started`
6. `Flare spawn result`에서 Flare가 `None`이 아님
7. Phase가 `FlareLaunched -> Waiting -> Inbound` 순서로 변경

직접 배치 Personal이 계속 필요하더라도 현재 `OwningTeamId`는 외부에서 편집할 수 없는 복제 전용 값이므로 팀 전용 개인 탈출구로 사용해서는 안 된다. 정상 마커-런타임 생성 경로를 사용하는 것이 맞다.

## 8. 코드 변경 및 검증 범위

변경 파일:

- `Source/OutBreak/Public/Game/Expedition/OBExtractionZone.h`
- `Source/OutBreak/Private/Game/Expedition/OBExtractionZone.cpp`
- `Source/OutBreak/Private/Game/GameMode/OBExpeditionGameMode.cpp`

검증:

- `OutBreakEditor Win64 Development` 빌드 성공
- 최종 코드로 `/Game/Maps/OutBreak_Exterior` 무창 런타임 실행 성공
- 런타임에서 Personal/Public 모두 32cm 상태 재현
- 검증 프로세스 정상 종료

사용자 편집 에셋인 `BP_ExtractionZone_Personal.uasset`과 OutBreak_Exterior 외부 액터 uasset은 이번 C++ 진단 작업에서 수정하지 않았다.
