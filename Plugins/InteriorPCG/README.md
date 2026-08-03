# 인테리어 PCG 사용법

OutBreak 프로젝트용 Unreal Engine 5.7 에디터 중심 인테리어 자동 배치 플러그인입니다.

## 1. 인테리어 볼륨 배치

1. 에디터의 **액터 배치(Place Actors) > 모든 클래스(All Classes)**를 엽니다.
2. `Interior PCG Volume`을 검색하여 레벨에 배치합니다.
3. 볼륨 Brush가 한 개 방의 바닥과 실내 높이를 감싸도록 크기를 조절합니다.
4. 첫 구현은 단일 층과 Yaw 회전만 전제로 하므로 볼륨에 Pitch/Roll 회전을 주지 않는 것이 좋습니다.

## 2. 가구와 Prop 등록

볼륨을 선택하고 Details Panel의 **Interior PCG > Generation > Asset Entries** 배열에 항목을 추가합니다.

각 항목에서 다음 값을 설정합니다.

- `Label`: 사용자가 구분하기 위한 이름
- `Enabled`: 생성에 사용할지 여부
- `Asset Kind`: `Static Mesh` 또는 `Actor Class`
- `Static Mesh` / `Actor Class`: 직접 사용할 에셋
- `Quantity Mode`: 고정 개수 또는 가중치 선택
- `Count`: 고정 생성 개수
- `Selection Weight`: 가중치 풀에서 선택될 확률 비중
- `Collision Half Extent`: 배치 충돌 검사에 사용하는 박스 반크기
- `Position Offset`: 메시 Pivot 등의 위치 보정
- `Rotation Offset`: 기본 회전 보정
- `Rotation Mode`: 고정, 완전 랜덤 Yaw, 각도 단위 랜덤 Yaw
- `Yaw Step Degrees`: 90도, 45도 같은 랜덤 회전 단위

Content Browser를 자동 검색하지 않으므로 이 배열에 직접 넣은 에셋만 사용됩니다.

## 3. 랜덤 인테리어 생성

1. 볼륨의 `Seed`를 설정합니다.
2. Details Panel의 **Interior PCG Actions**에서 **Generate Random Interior**를 누릅니다.
3. 볼륨 내부의 최종 XY 위치에서 아래 방향으로 바닥을 탐색합니다.
4. 유효한 바닥과 충돌하지 않는 위치에 독립 Actor를 생성합니다.

같은 Seed, 같은 배열 순서, 같은 볼륨 및 충돌 환경에서는 같은 결과가 생성됩니다.

## 4. 생성 결과 직접 편집

생성된 가구는 Instanced Static Mesh가 아닌 독립 Actor입니다. 일반 레벨 Actor처럼 다음 작업을 할 수 있습니다.

- 이동
- 회전
- 스케일 변경
- Static Mesh 변경
- 삭제

Actor에 붙은 `InteriorPCGItemComponent`에서 `Included In Preset`을 끄면 현재 Actor는 유지하면서 프리셋 재생성 대상에서는 제외할 수 있습니다.

## 5. 사용자가 배치한 Prop 등록

1. 인테리어 볼륨을 선택합니다.
2. `Props To Register` 배열에 스포이드로 레벨의 Prop Actor를 지정합니다.
3. **Register Listed / Selected Props**를 누릅니다.

등록된 Prop에도 Stable ID와 Generator 정보가 추가되며, 이후 프리셋 저장과 Preview 정리에 포함됩니다. 볼륨의 Details 버튼이 보이는 다중 선택 상태에서는 현재 선택된 Prop도 함께 등록할 수 있습니다.

## 6. 새 PCG Preset 저장

1. 생성 결과를 원하는 상태로 직접 수정합니다.
2. 볼륨을 선택합니다.
3. **Save As New PCG Preset**을 누릅니다.
4. Content Browser에서 저장 위치와 에셋 이름을 정합니다.

프리셋에는 에셋 또는 클래스, Stable ID, 정규화된 XY 위치, 볼륨 상대 회전, 스케일, 바닥 높이 보정, 충돌 크기, 활성/제외 상태가 저장됩니다.

## 7. 기존 Preset 업데이트

1. 볼륨의 `Selected Preset`에 업데이트할 프리셋을 지정합니다.
2. 현재 레벨 배치를 수정합니다.
3. **Update Selected Preset**을 누릅니다.

버튼을 누른 시점의 등록된 Actor 상태만 캡처합니다. PCG Graph가 편집 상태를 실시간 감시하지는 않습니다.

## 8. Preview 제거와 Preset 재생성

1. **Clear Preview**를 눌러 현재 Generator에 등록된 Actor를 정리합니다.
2. `Selected Preset`이 올바른지 확인합니다.
3. **Generate Selected Preset**을 누릅니다.

저장된 정규화 XY를 현재 볼륨 크기에 맞게 복원한 뒤, 현재 공간의 바닥을 다시 탐색하여 저장된 높이 보정을 적용합니다.

## 9. 다른 방에 같은 Preset 적용

1. 다른 방에 새 `Interior PCG Volume`을 배치합니다.
2. `Selected Preset`에 기존 프리셋을 지정합니다.
3. **Generate Selected Preset**을 누릅니다.

볼륨 크기가 달라도 정규화된 XY 패턴이 새 볼륨에 맞게 확장 또는 축소됩니다. 가구 자체의 스케일은 프리셋에 저장된 값을 사용합니다.

## PCG Graph 사용

`/Game/InteriorPCG/PCG_InteriorGenerator`에는 `Generate Editable Interior Actors` 커스텀 PCG 노드가 들어 있습니다.

- `Graph Generation Mode = Random Entries`: 에셋 배열에서 랜덤 생성
- `Graph Generation Mode = Selected Preset`: 선택한 프리셋으로 생성

전용 Details 버튼은 Undo와 Transaction 동작을 안정적으로 유지하기 위해 같은 Runtime 생성기를 동기식으로 호출합니다.

## 충돌 관련 설정

- `Floor Trace Channel`: 바닥 추적 채널
- `Placement Collision Channel`: 가구와 구조물 충돌 검사 채널
- `Minimum Floor Normal Z`: 바닥으로 인정할 최소 경사
- `Collision Half Extent`: 각 가구가 차지하는 기본 공간
- `Check Preset Collision`: 프리셋 적용 시에도 충돌 검사

같은 방에서 저장한 배치를 정확하게 복원하는 것이 충돌 거부보다 중요하다면 `Check Preset Collision`을 끌 수 있습니다.

## 현재 제한사항

- 단일 층만 지원합니다.
- 볼륨 Pitch/Roll은 지원 범위 밖입니다.
- 문, 창문, 방 종류, 통로와 내비게이션은 분석하지 않습니다.
- Actor Class 인스턴스의 임의 컴포넌트 프로퍼티 변경은 저장하지 않으며 클래스와 Transform을 저장합니다.
- 충돌은 사용자가 지정한 박스 크기와 월드 Blocking Collision을 사용하는 기본 검사입니다.
- 런타임 네트워크 복제와 최종 인스턴싱 최적화는 포함하지 않습니다.
