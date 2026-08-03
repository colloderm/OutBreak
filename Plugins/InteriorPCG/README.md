# 인테리어 PCG 사용법

OutBreak 프로젝트용 Unreal Engine 5.7 에디터 중심 인테리어 자동 배치 플러그인입니다. 에셋은 Content 폴더에서 자동 검색하지 않으며, Details Panel 배열에 사용자가 직접 넣은 에셋과 클래스만 사용합니다.

## 1. 일반 가구와 Prop 생성

1. **액터 배치(Place Actors) > 모든 클래스(All Classes)**에서 `Interior PCG Volume`을 레벨에 배치합니다.
2. Volume Brush가 생성할 단일 층 또는 여러 층의 바닥과 실내 높이를 모두 감싸도록 크기를 조정합니다. Volume의 Yaw 회전을 지원하며 Pitch/Roll은 사용하지 않는 것을 권장합니다.
3. Details Panel의 **Interior PCG > Generation > Asset Entries**에 항목을 추가합니다.
4. `Seed`를 지정하고 **Interior PCG Actions > Generate Random Interior**를 누릅니다.

### Asset Entries 항목

- `Label`: 사용자가 알아보기 위한 이름
- `Enabled`: 생성에 포함할지 여부
- `Asset Kind`: `Static Mesh` 또는 `Actor Class`
- `Static Mesh` / `Actor Class`: 직접 사용할 에셋
- `Quantity Mode`: 고정 개수 또는 가중치 풀
- `Count`: 고정 생성 개수
- `Selection Weight`: 가중치 풀에서 선택될 비율
- `Collision Half Extent`: 배치 충돌 검사 박스의 반크기
- `Position Offset`: Pivot과 바닥 사이의 위치 보정
- `Rotation Offset`: 에셋 축의 회전 보정
- `Rotation Mode`: 고정, 완전 랜덤 Yaw, 각도 단위 랜덤 Yaw
- `Yaw Step Degrees`: 90도, 45도 같은 랜덤 회전 단위

Volume 안의 임의 XY에서 아래 방향으로 바닥을 추적하며, 유효한 바닥과 충돌하지 않는 위치에 독립 Actor를 생성합니다. 같은 Seed, 배열 순서, Volume, 바닥 및 충돌 환경에서는 같은 결과가 생성됩니다.

### 여러 층 자동 생성 옵션

기본값은 기존 동작을 유지하기 위해 단층 생성입니다. 여러 층을 자동으로 찾으려면 **Interior PCG > Multi Floor**에서 `Generate On All Detected Floors`를 켭니다.

1. 하나의 Volume Brush로 생성할 모든 층을 감쌉니다.
2. 각 층 바닥이 `Floor Trace Channel`을 Blocking하도록 설정합니다.
3. `Generate On All Detected Floors`를 켭니다.
4. **Volume 내부 바닥 층 검사** 버튼을 눌러 `Last Detected Floor World Heights`를 확인합니다. 생성 버튼을 누를 때도 자동으로 다시 검사합니다.
5. 일반 가구 생성, 프리셋 생성 또는 벽 생성 버튼을 실행합니다.

설정 의미는 다음과 같습니다.

- `Floor Detection Samples Per Axis`: Volume XY를 검사하는 격자의 한 축 샘플 수입니다. 값 5는 25개 지점을 검사합니다.
- `Floor Layer Height Tolerance`: 높이가 조금 다른 바닥 Hit를 같은 층으로 묶는 허용 오차이며, 경사진 바닥의 층 선택에도 사용됩니다.
- `Minimum Floor Sample Coverage`: 전체 검사 지점 중 해당 높이의 바닥이 감지되어야 하는 최소 비율입니다. 작은 탁자나 선반이 층으로 오인되는 것을 줄입니다.
- `Maximum Detected Floor Count`: 낮은 층부터 사용할 최대 층 수입니다.

`Count`, `Weighted Selection Count`, `Partition Wall Count`는 감지된 **각 층마다** 적용됩니다. 기본 `Seed`와 `Wall Seed`는 그대로 사용하지만 내부적으로 층 인덱스를 섞은 파생 Seed를 사용하므로 같은 Seed여도 층마다 다른 랜덤 배치가 생성됩니다. 같은 바닥 구성과 같은 입력으로 다시 생성하면 각 층의 결과는 동일하게 재현됩니다.

## 2. 내부 벽 생성용 자식 Volume

내부 벽 기능이 필요하면 일반 `Interior PCG Volume` 대신 자식 클래스인 `Interior PCG Wall Volume`을 배치합니다. 기존 가구 생성, 직접 편집, Prop 등록, Preset 저장 기능을 모두 그대로 사용할 수 있습니다.

### 준비할 벽 Actor Class

- `Wall Classes`: 일반 벽 한 칸을 표현하는 에셋 배열
- `Door Wall Classes`: 문 또는 문틀이 포함된 벽 한 칸을 표현하는 에셋 배열
- `Stair Classes`: 한 층에서 다음 층으로 연결되는 계단 에셋 배열
- 세 배열 모두 사용자가 직접 등록한 에셋만 사용합니다.
- 각 배열 항목의 `Asset Kind`에서 `Static Mesh` 또는 `Actor Class`를 선택할 수 있습니다. Content 폴더 자동 검색은 하지 않습니다.
- 각 벽 Actor는 로컬 `+X`가 벽의 길이 방향이고 Pivot이 바닥 중앙에 있는 구성을 권장합니다.
- 모든 벽/문 벽 클래스는 같은 기본 모듈 길이를 사용해야 합니다. 축이나 Pivot이 다르면 항목별 `Position Offset`과 `Rotation Offset`으로 보정합니다.
- 실제 메시의 크기에 맞춰 항목별 `Scale`과 `Collision Half Extent`를 지정합니다.
- 외곽 벽과 바닥은 아래 설정에서 선택한 Trace Channel을 Blocking 해야 합니다.

### 주요 벽 설정

- `Wall Seed`: 가구 Seed와 독립적인 벽 전용 Seed
- `Partition Wall Count`: 만들 내부 칸막이 선의 수
- `Wall Direction Mode`: Volume 로컬 X, 로컬 Y 또는 벽마다 랜덤 X/Y
- `Wall Module Length`: 벽 Actor 한 칸의 중심 간격
- `Wall Scan Height`: 바닥에서 외곽 벽을 수평 탐색할 높이
- `Wall End Clearance`: 감지한 외곽 벽과 새 내부 벽 끝 사이의 여유
- `Partition Margin`: 후보 칸막이 중심을 Volume 가장자리에서 띄우는 거리
- `Wall Trace Channel`: 외곽 벽을 읽는 충돌 Trace Channel
- `Maximum Boundary Wall Normal Z`: 벽으로 허용할 표면의 최대 수직 성분
- `Door Chance Per Partition`: 각 칸막이에 문 벽 한 칸을 넣을 확률
- `Door End Padding Modules`: 칸막이 양 끝에서 문 벽을 피할 모듈 수

### 문·계단 연결 통로 강제

다층에서 문과 계단 사이의 통로를 강제로 확보하려면 `Require Connected Door And Stair Paths`를 켭니다. 이 옵션은 다층 자동 생성이 켜져 있고 바닥이 2개 이상 감지될 때 동작합니다.

1. `Door Wall Classes`에 실제로 통과 가능한 문 또는 문틀이 포함된 벽 모듈을 등록합니다.
2. `Stair Classes`에 계단 Static Mesh 또는 Actor Class를 등록합니다.
3. 각 계단 항목의 `Lower Access Point Offset`을 아래층 계단 입구, `Upper Access Point Offset`을 위층 계단 출구의 로컬 위치로 설정합니다.
4. 문 항목의 `Door Access Point Offset`을 플레이어가 통과할 문 중심 위치로 설정합니다.
5. `Require Connected Door And Stair Paths`를 켜고 **벽·문·계단 생성**을 실행합니다.

연결 모드의 규칙은 다음과 같습니다.

- 감지된 층 전환마다 계단 Actor 하나를 생성합니다. 3층이면 0→1층과 1→2층 계단 두 개가 생성됩니다.
- 모든 칸막이에는 문 모듈이 반드시 하나 들어갑니다. `Door Chance Per Partition`보다 연결 규칙이 우선합니다.
- 각 층에서 문 중심과 가장 가까운 계단 접근점 사이에 통로를 확보합니다.
- 3층 이상 건물의 중간층에서는 이전 계단 출구와 다음 계단 입구 사이의 통로를 먼저 확보합니다.
- 통로는 Volume 로컬 X/Y 방향의 두 가지 L자 후보를 검사합니다. 바닥 연속성, 구조물, 기존 가구, 생성 벽, 계단 충돌 박스와 통로 폭/높이를 검사합니다.
- 확보된 통로 공간은 이후 일반 벽 모듈이 침범할 수 없습니다. 통로와 칸막이가 만나는 위치에는 문 모듈만 허용됩니다.
- 제한된 시도 안에 모든 연결을 만족하지 못하면 생성한 벽·문·계단을 전부 롤백합니다. 기존 가구/Prop은 유지됩니다.

연결 설정은 다음과 같습니다.

- `Max Stair Placement Attempts`: 각 층 전환의 계단 후보 최대 시도 횟수
- `Walkway Half Width`: 확보할 통로 폭의 절반
- `Walkway Half Height`: 바닥 위에서 확보할 통로 높이의 절반
- `Walkway Sample Spacing`: L자 통로를 충돌 검사하는 샘플 간격
- `Last Connectivity Path Points`: 마지막 성공 생성에서 확보한 통로 샘플 위치

문 메시의 Collision은 실제 문 개구부를 통과할 수 있어야 하며 계단 메시 자체도 두 층을 물리적으로 연결하도록 제작해야 합니다. 생성기는 지정한 접근점과 충돌 박스를 기준으로 주변 통로를 보장하지만, 잘못 제작된 문/계단 메시 내부의 통과 가능성까지 수정하지는 않습니다.

### 벽 생성 버튼

`Interior PCG Wall Volume`을 선택하면 **내부 벽 PCG 작업** 카테고리가 추가됩니다.

- **벽·문·계단 생성**: 기존 가구/Prop은 유지하고 벽, 문 벽, 계단 역할 Actor를 다시 생성합니다.
- **가구 + 내부 벽 생성**: 기존 Preview를 정리하고 가구를 먼저 생성한 뒤, 가구 충돌을 피하며 벽을 생성합니다.
- **벽·문·계단 정리**: 내부 벽, 문 벽, 계단 역할 Actor만 삭제하고 가구/Prop은 유지합니다.

생성 과정은 후보 위치의 바닥을 먼저 찾고, `Wall Scan Height`에서 로컬 X 또는 Y 양쪽으로 수평 Trace를 쏴 실제 Blocking 벽 두 면을 찾습니다. 두 벽 사이를 `Wall Module Length`로 나눠 모듈을 배치합니다. 한쪽이라도 벽이 감지되지 않으면 해당 후보는 사용하지 않습니다.

## 3. 생성 결과 직접 편집

가구, Prop, 벽, 문 벽은 Instanced Static Mesh가 아니라 독립 Actor입니다. 레벨에서 개별 선택하여 이동, 회전, 스케일 변경, 에셋 또는 클래스가 허용하는 속성 변경, 삭제를 할 수 있습니다.

생성 Actor의 `InteriorPCGItemComponent`에서 `Included In Preset`을 끄면 현재 Actor는 유지하되 다음 Preset 저장 시 제외 상태로 보존됩니다.

새 편집용 Prop은 다음 순서로 등록합니다.

1. Generator의 `Props To Register` 배열에 Actor를 지정하거나 레벨에서 Generator와 Prop을 선택합니다.
2. **Register Listed / Selected Props**를 누릅니다.
3. 등록된 Actor에는 Stable ID와 Generator 연결 정보가 붙고 Preset 저장 대상이 됩니다.

## 4. PCG Preset 저장과 재생성

1. 자동 생성 결과를 원하는 상태로 직접 수정합니다.
2. **Save As New PCG Preset**을 눌러 새 Content Browser 에셋으로 저장합니다.
3. 기존 Preset을 갱신하려면 `Selected Preset`을 지정하고 **Update Selected Preset**을 누릅니다.
4. **Clear Preview**로 현재 Generator의 등록 Actor를 정리합니다.
5. **Generate Selected Preset**으로 다시 생성합니다.

Preset에는 사용 에셋/클래스, Stable ID, Volume 기준 정규화 XY, 상대 회전, 스케일, 바닥 높이 보정, 층 번호, 충돌 크기, 활성/제외 상태와 항목 역할(가구, 내부 벽, 문 벽, 계단)이 저장됩니다. 다시 생성할 때 현재 Volume 크기로 XY를 복원하고 현재 층의 바닥을 다시 Trace합니다. 다른 크기의 방에도 같은 Preset을 적용할 수 있습니다.

다층 모드에서 저장한 Preset은 각 항목의 층 번호를 보존하므로 저장 당시의 층에만 복원됩니다. 기존 단층 Preset처럼 층 번호가 없는 항목은 감지된 모든 층에 한 번씩 반복 적용됩니다. 다층 Preset을 단층 옵션으로 생성하면 0층 항목과 기존 단층 항목만 생성합니다.

동일한 방에서 저장한 배치를 정확히 복원하는 것이 우선이면 `Check Preset Collision`을 끕니다. 켜면 현재 구조물과 충돌하는 Preset 항목은 생성하지 않습니다.

### 여러 Preset 한 번에 생성

`Selected Preset`은 새 Preset 저장과 기존 Preset 업데이트에 사용하는 단일 작업 대상입니다. 여러 Preset을 합쳐 생성할 때는 별도의 `Selected Presets (Batch)` 배열을 사용합니다.

1. `Selected Presets (Batch)` 배열에 생성할 Preset을 순서대로 등록합니다.
2. **등록 프리셋 전체 생성**을 누릅니다.
3. 기존 Preview를 한 번만 정리한 뒤 배열의 모든 활성 배치 항목을 현재 Volume에 순서대로 생성합니다.

`Check Preset Collision`이 켜져 있으면 먼저 생성된 Preset의 Actor도 다음 Preset의 충돌 검사에 포함됩니다. 끄면 저장된 배치를 그대로 겹쳐서 생성할 수 있습니다. 서로 다른 Preset에 같은 Stable ID가 있으면 두 번째 이후 ID만 배열 순서에 따라 결정적으로 재생성하여, 합쳐진 결과 안에서 ID가 중복되지 않게 합니다. 같은 Preset 에셋을 배열에 중복 등록한 경우에는 한 번만 생성합니다.

## 5. PCG Graph 사용

`/Game/InteriorPCG/PCG_InteriorGenerator`에는 C++ `Generate Editable Interior Actors` 노드가 들어 있습니다.

- `Graph Generation Mode = Random Entries`: 일반 Volume은 가구 배열을 생성합니다. Wall Volume은 `Generate Walls With Random Graph Generation`이 켜져 있으면 가구와 벽을 함께 생성합니다.
- `Graph Generation Mode = Selected Preset`: `Selected Presets (Batch)` 배열이 비어 있으면 단일 `Selected Preset`을 생성하고, 배열에 항목이 있으면 등록된 모든 Preset을 한 번에 생성합니다. 저장된 벽/문 역할도 복원합니다.

Details 버튼과 Graph는 같은 Runtime 생성 함수를 호출합니다. Preset 에셋 생성/저장, Undo Transaction, 패키지 Dirty 처리는 Editor 모듈에만 있습니다.

## 6. 현재 제한사항

- Volume Pitch/Roll은 지원 범위 밖이며 Yaw 기준으로 동작합니다.
- 다층 검사는 Volume의 동일한 XY 범위 안에서 수평 바닥 표면을 높이별로 군집화합니다. 층마다 평면 범위가 크게 다르면 샘플 수와 최소 커버리지를 조정해야 합니다.
- 충분히 넓은 옥상, 대형 플랫폼 또는 수평 구조물도 조건을 만족하면 바닥 층으로 감지될 수 있습니다.
- 생성된 계단 접근점 사이의 통로 외에는 층 의미, 층 이름, 엘리베이터와 층간 동선을 분석하지 않습니다.
- 외곽 벽은 지정한 `Wall Trace Channel`을 막고, 양쪽 수평 Trace가 모두 벽의 세로 면에 닿아야 합니다.
- 내부 칸막이는 Volume 로컬 X/Y 직선만 생성하며 L자, 곡선, 교차 연결, 방 의미 분석은 하지 않습니다.
- 연결 통로 검사는 Volume 로컬 X/Y의 L자 후보 두 개를 사용하는 기본 공간 검사이며 NavMesh 경로 탐색은 아닙니다.
- 문 클래스는 별도 문 의미 분석 결과가 아니라 “문이 포함된 벽 모듈”로 취급됩니다.
- 문/창문 자동 인식, 통로/내비게이션 분석, 네트워크 복제, 최종 인스턴싱 최적화는 포함하지 않습니다.
- Actor Class 인스턴스의 임의 컴포넌트 속성 전체를 Preset에 직렬화하지는 않습니다. 클래스와 Transform, PCG 메타데이터를 저장합니다.
- 충돌은 사용자가 지정한 박스 반크기와 월드 Blocking Collision을 이용하는 기본 검사입니다.
