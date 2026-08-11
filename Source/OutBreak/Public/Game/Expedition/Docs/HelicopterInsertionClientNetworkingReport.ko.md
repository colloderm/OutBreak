# 헬기 투입 시스템 클라이언트 네트워크 대응 보고서

작성일: 2026-08-10

## 1. 결론

기존 투입 시스템은 서버에서 헬기 비행과 승객 이동을 처리하는 구조 자체는 맞았지만, 원격 클라이언트가 투입 프레젠테이션을 복구할 수 있는 상태와 타 플레이어의 좌석/레펠 상태가 충분히 복제되지 않았다.

특히 실제 멀티프로세스 시험에서 `Client_BeginInsertionPresentation` RPC가 도착했을 때 헬기 Actor 참조가 아직 해석되지 않아 null로 들어오는 상황이 재현됐다. 기존 구현에서는 이 한 번의 RPC를 놓치면 헬기 카메라, 투입 입력, 지도 UI를 다시 시작할 경로가 없었다.

이번 수정으로 다음 경로를 추가했다.

1. 서버의 `FOBTeamInsertionState`를 클라이언트 복구용 기준 상태로 사용한다.
2. 클라이언트는 PlayerState, Pawn, 원정 페이즈, 팀 투입 상태가 복제될 때마다 자신의 팀 헬기를 재탐색한다.
3. 헬기 참조가 늦게 도착해도 카메라, 투입 입력, 지도 상태를 현재 투입 페이즈에 맞게 복구한다.
4. 투입 헬기는 거리와 무관하게 해당 팀 클라이언트에게 계속 relevant 상태를 유지한다.
5. 승객의 좌석/레펠 상태는 Controller가 아니라 Pawn 기준의 경량 복제 상태로 모든 해당 팀 클라이언트에 전달한다.
6. 클라이언트가 헬기 ViewTarget 설정을 끝내면 서버에 진단 ACK를 보낸다.

## 2. 기존 괴리

### 2.1 일회성 Client RPC와 Actor 참조 복제 순서

Actor를 인자로 가진 Client RPC가 도착해도 해당 Actor 채널/참조가 클라이언트에서 아직 준비되지 않았으면 인자가 null일 수 있다. 기존 `Client_BeginInsertionPresentation`은 이 경우를 복구하지 못했다.

### 2.2 헬기 거리 기반 relevancy

헬기는 맵 외곽이나 먼 궤도를 비행할 수 있다. 기본 거리 기반 relevancy만 사용하면 플레이어 Pawn 위치와 헬기 사이 거리가 커졌을 때 해당 클라이언트에서 헬기 복제가 중단될 가능성이 있었다.

### 2.3 Controller 기반 승객 연출 이벤트

`AController`는 소유 클라이언트가 아닌 다른 클라이언트에서 동일한 방식으로 사용할 수 없다. 따라서 기존 `BP_OnPassengerSeated(AController*)`, `BP_OnPassengerRappelStarted(AController*)`, `BP_OnPassengerLanded(AController*)`만으로는 타 플레이어 좌석/레펠 연출을 안정적으로 구성할 수 없었다.

## 3. 구현 내용

### 3.1 팀 제한 헬기 relevancy

`AOBInsertionHelicopter::IsNetRelevantFor`를 재정의했다.

- 임무가 Insertion이고 TeamId가 유효할 때, 보는 PlayerState의 TeamId가 헬기 TeamId와 같으면 거리와 관계없이 relevant다.
- 다른 팀에는 기존 `Super::IsNetRelevantFor` 결과를 사용한다.
- 모든 팀 헬기를 무조건 `bAlwaysRelevant`로 공개하지 않으므로 불필요한 네트워크 전송을 피한다.

### 3.2 Pawn 기반 승객 상태 복제

추가 구조체: `FOBHelicopterPassengerNetState`

- `Pawn`
- `SeatIndex`
- `Phase`: Seated 또는 Rappelling
- `RopeIndex`
- `RopeStart`, `RopeEnd`

서버는 다음 시점에 `ReplicatedPassengerStates`를 변경하고 `ForceNetUpdate()`를 호출한다.

- 좌석 배치: Seated 상태 추가
- 레펠 시작: Rappelling 상태와 로프 좌표 기록
- 착지: 상태 제거
- 안전 강제 하차: 배열 전체 정리

클라이언트 `OnRep_PassengerStates`는 이전 프레젠테이션 상태와 새 상태를 비교해 Blueprint 이벤트를 발생시킨다.

### 3.3 클라이언트 프레젠테이션 재동기화

`AOBPlayerController`는 다음 이벤트에서 재동기화를 예약한다.

- `OnRep_PlayerState`
- `OnRep_Pawn`
- 원정 페이즈가 Insertion으로 변경될 때
- `OnTeamInsertionStatesChanged`
- Begin/Update RPC가 헬기 참조 또는 프레젠테이션보다 먼저 도착했을 때

`ReconcileInsertionPresentationFromGameState()`는 로컬 PlayerState의 TeamId로 `FOBTeamInsertionState`를 조회한다. 헬기 Actor가 유효해진 뒤 다음을 실행한다.

- ViewTarget을 해당 헬기로 전환
- 헬기 객실 회전으로 ControlRotation 초기화
- 투입 전용 Enhanced Input Context 활성화
- 현재 헬기 페이즈에 맞춰 E 지점 선택 가능 여부 결정
- WaitingForTarget/Orbiting/LoadingTarget/ValidatingTarget일 때만 투입 지도를 연다
- 이미 Approaching 이후라면 늦게 복구됐다는 이유로 지도를 다시 열지 않는다
- 복제된 현재 페이즈를 Blueprint UI에 재적용

직접 Client RPC는 낮은 지연의 정상 경로로 유지하고, GameState는 순서 역전이나 지연 복제를 견디는 복구 경로로 사용한다.

### 3.4 서버 준비 ACK

클라이언트 프레젠테이션 구성이 완료되면 다음 정보가 서버 로그에 기록된다.

- PlayerController와 TeamId
- 헬기와 Helicopter TeamId
- 클라이언트가 확인한 투입 페이즈
- 현재 ViewTarget이 헬기인지 여부
- 팀 일치 검증 결과

로그 접두사: `[InsertionNet] Client ready ack`

이 RPC는 게임 진행 권한을 바꾸지 않고 진단/검증만 수행한다. 착륙 후보 요청과 투입 진행 권한은 계속 서버에 있다.

## 4. Blueprint/uasset 연결 지점

`BP_OBInsertionHelicopter` 또는 C++ 클래스를 상속한 사용 중 헬기 Blueprint에서 아래 이벤트를 구현할 수 있다.

### On Replicated Passenger Seated

파라미터:

- `PassengerPawn`
- `SeatIndex`

용도:

- `GetSeatTransform(SeatIndex)`를 이용한 좌석 연출
- 해당 Pawn의 탑승 자세/가시성 처리
- 원격 팀원의 좌석 표시

### On Replicated Passenger Rappel Started

파라미터:

- `PassengerPawn`
- `RopeIndex`
- `RopeStart`
- `RopeEnd`

용도:

- 제작한 로프 Niagara, Cable, Mesh uasset 생성/활성화
- RopeIndex별 좌/우 로프 선택
- Pawn 레펠 애니메이션 시작

### On Replicated Passenger Landed

파라미터:

- `PassengerPawn`

용도:

- 원격 Pawn 레펠/좌석 연출 정리
- 로프 및 임시 이펙트 해제
- 착지 애니메이션 전환

기존 Controller 기반 이벤트는 서버 및 로컬 소유 승객 호환을 위해 유지했다. 타 플레이어를 포함한 네트워크 공통 연출에는 새 Pawn 기반 이벤트를 사용해야 한다.

현재 C++ 변경은 Blueprint/uasset을 자동 수정하지 않는다. 제작한 헬기, 로프, 애니메이션 uasset은 위 이벤트 구현 내부에서 기존 방식대로 파라미터로 연결하면 된다.

## 5. 실제 네트워크 검증

시험 구성:

- UE 5.7 `OutBreakEditor Win64 Development`
- 전용 서버: `/Game/Maps/OutBreak_Exterior`, port 7787
- 별도 `UnrealEditor-Cmd -game` 클라이언트
- 서버와 클라이언트는 서로 다른 프로세스

확인된 순서:

1. 클라이언트가 `OutBreak_Exterior`의 `BP_ExpeditionGameMode` 서버에 접속했다.
2. Update RPC가 프레젠테이션보다 먼저 도착해 지연 처리됐다.
3. Begin RPC에서 헬기 Actor 참조가 아직 null인 순서 역전이 재현됐다.
4. 이후 클라이언트가 헬기와 승객 1명의 복제 상태를 받았다.
5. GameState 재동기화가 프레젠테이션을 시작했다.
6. 클라이언트 ViewTarget이 `BP_OBInsertionHelicopter_C_0`로 설정됐다.
7. 서버가 `Team=1`, `HelicopterTeam=1`, `ViewReady=true`, `ValidTeam=true` ACK를 수신했다.

검증 로그:

- 서버: `Saved/Logs/CodexHelicopterNetServer.log`
- 최초 서버 로드 중 타임아웃 재현 클라이언트: `Saved/Logs/CodexHelicopterNetClient.log`
- 정상 재접속/복구 클라이언트: `Saved/Logs/CodexHelicopterNetClient2.log`

핵심 클라이언트 로그:

```text
[InsertionNet] Begin RPC arrived before helicopter reference resolved
[InsertionNet] Passenger state applied Client ... Count=1
[InsertionNet] Presentation active Source=GameStateReconcile ... ViewTarget=BP_OBInsertionHelicopter_C_0
[InsertionNet] Presentation update Source=GameStateReconcile ... Phase=2
```

핵심 서버 로그:

```text
[InsertionNet] Client ready ack ... Team=1 ... HelicopterTeam=1 ... ViewReady=true ValidTeam=true
```

빌드 결과:

- UHT `-WarningsAsErrors` 통과
- `OutBreakEditor Win64 Development` 성공
- 최종 증분 재빌드 성공

## 6. 편집기에서 추가 확인할 항목

헤드리스 멀티프로세스 시험은 네트워크 Actor 해석, 승객 상태, 카메라 ViewTarget, 프레젠테이션 복구와 서버 ACK까지 검증했다. 다음 시각/입력 항목은 실제 렌더링 PIE 클라이언트 2개로 확인한다.

- 각 클라이언트에서 마우스 헬기 카메라 회전
- M 지도 토글
- 리더 클라이언트의 E 라인트레이스와 서버 착륙 후보 판정
- 비리더 클라이언트에서 E 선택 차단
- 원격 팀원 좌석/레펠 Blueprint 연출
- 레펠 완료 후 각 소유 클라이언트의 Pawn 카메라 및 사격 복구

로그 필터는 `InsertionNet`, `InsertionInput`, `OBHelicopterInsertion`을 사용한다.
