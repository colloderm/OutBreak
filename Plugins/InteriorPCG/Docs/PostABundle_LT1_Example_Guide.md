# PostABundle LT1 기반 Interior PCG 예제 상세 가이드

## 결론

현재 `InteriorPCG`는 `BP_P_Building`을 그대로 복제하는 도구라기보다, 그 블루프린트가 사용하는 **모듈 조립 문법을 데이터 기반으로 재생성하는 시스템**으로 적합합니다. 분석 중 확인된 LT1의 층별 파사드, 계단 홀, 복합 메시와 복수 출입구를 지원하도록 런타임을 보강했고, 요청한 다섯 데이터 에셋에는 실행 가능한 예제를 기록했습니다.

다만 결과가 원본 블루프린트와 픽셀 단위로 같지는 않습니다. 원본의 면별 수동 슬롯, 동적 머티리얼 랜덤화, 보도, 덩굴/산란 장식까지 완전히 복제하려면 별도의 파사드 패턴 및 머티리얼 규칙이 더 필요합니다.

## 분석 대상

- 부모: `/Game/PostABundle/Blueprints/Buildings/Parent/BP_P_Building`
- LT1 자식: `/Game/PostABundle/Blueprints/Buildings/BP_Building_LT1`
- 변환 전 붙여넣기: `BP_Building_LT1_C` 액터 하나를 직렬화한 텍스트
- 변환 후 붙여넣기: 결과를 개별 `StaticMeshActor`로 풀어낸 텍스트
- 감사 로그: `Saved/Logs/InteriorPCGReferenceSetup.log`

## `BP_P_Building` 구조 분석

에디터가 실제 `.uasset`을 로드한 결과, 부모 블루프린트는 `Actor` 직계 자식이며 SCS 노드 28개, 함수 그래프 21개, Construction Script용 ubergraph 1개를 갖습니다. 핵심 SCS 구성은 다음과 같습니다.

| 역할 | 실제 구성 |
|---|---|
| 반복 파사드 | `WindowMeshes`, `TopLevelWindowMeshes`, `CornerMeshes`, `TopLevelCornerMeshes`, `GroundLevelCornerMeshes`, `WallMeshes`, `TopLevelWallMeshes`, `WindowDecoMeshes` HISM |
| 외곽/방향 | `MainBBox`, 네 방향 `FaceBBox`, `RoofFaceBBox`, `FloorFaceBBox` Box Component |
| 부가 구조 | 보도용 HISM 3개, 기둥용 HISM 2개, `InteriorCheck` |
| 디버깅 | Front/Back/Left/Right Text Render |

부모 기본값에서 `FloorHeight=500cm`, `Wide=4`, `Long=3`, `FloorAmount=2`가 확인됩니다. LT1 에셋 CDO는 `FloorAmount=10`, `Wide=4`, `Long=3`이지만, 붙여넣은 **레벨 인스턴스가 이를 `FloorAmount=4`, `Wide=2`, `StaticMeshVersion=True`, `ConstructFloor=True`로 덮어씁니다.** 따라서 이번 레퍼런스의 실제 크기는 에셋 기본값이 아니라 인스턴스 오버라이드를 기준으로 판단해야 합니다.

관련 직접 종속성에는 다음이 포함됩니다.

- 데이터 구조: `F_Door_Data`, `F_FloorData`, `F_Scatter_Data`, `E_BuildingFace`
- 상호작용 자식: `BP_LT1_Door1`, `BP_Stairs_Steps1_2XX`
- LT1 구조 메시: Floor, Floor_Hole, Roof1, ground/middle/top wall·corner·window 패밀리
- 보조 레이어: `SM_LT1_Wall_C_L1_W`, `SM_LT1_Window1_W`
- 부가물: `SM_Vent_Roof_Set1`, 보도·기둥·덩굴 에셋

이 구조는 원본 시스템도 이미 “층/면/역할별 모듈 + HISM + 상호작용 ChildActor” 방식임을 보여 줍니다. `InteriorPCG`의 데이터 에셋 분리 모델과 잘 맞는 이유입니다.

## 두 텍스트가 같은 결과인지

결론은 **같은 건물 결과가 맞습니다.** 단, 첫 번째 텍스트는 하나의 병합 메시가 아니라 개별 정적 메시 액터로 확장된 결과입니다.

### 변환 전 텍스트

- 액터 1개: `BP_Building_LT1_C`
- 부모 생성 클래스: `BP_P_Building`
- 직렬화 오브젝트 213개
- Static Mesh Component 65개, HISM 20개, Child Actor Component 5개
- Child Actor: 계단 3개, 문 2개
- 주요 인스턴스 값: `FloorAmount=4`, `Wide=2`, `SidewalkLong=3`, Seed `555011868`
- `MainBBox`: 약 `1800 × 2700 × 2000cm`

### 변환 후 텍스트

- 125개 액터가 모두 `StaticMeshActor`
- 고유 메시 18종
- 바닥 피벗은 X/Y 방향 900cm 간격의 2×3 격자
- 층 Z는 0, 500, 1000, 1500cm, 지붕은 2000cm
- 계단 비행 3개가 네 층 사이를 연결
- LT1 Ground/Middle/Top 코너 및 창 패밀리가 같은 위치 관계로 존재
- `Wall_C_L1 + Wall_C_L1_W`, `Window1 + Window1_W`가 동일 트랜스폼에 겹침
- 두 개의 ground doorway가 서로 반대쪽 서/동 파사드에 위치

개수가 1:1이 아닌 이유는 변환 전의 HISM 인스턴스와 ChildActor 내부 메시가 변환 후에 실제 `StaticMeshActor`들로 물질화되기 때문입니다. 예를 들어 변환 전 계단은 ChildActor 3개지만, 변환 후에는 step middle/side 조각 수십 개로 나타납니다.

## 분석으로 확인한 LT1 생성 규약

| 항목 | 값 | 근거 |
|---|---:|---|
| 셀 크기 | 900cm | Floor/Roof 실제 bounds와 변환 후 피벗 간격 |
| 층고 | 500cm | 부모 프로퍼티 및 Z 간격 |
| 레퍼런스 격자 | 2×3 | 6개 바닥 위치 |
| 층 수 | 4 | 인스턴스 `FloorAmount=4` |
| 높이 | 2000cm | 4 × 500, MainBBox와 roof Z |
| 계단 | 3개 비행 | 인접 층 쌍마다 하나 |
| 코너 | 층당 4개 | Ground/Middle/Top 전용 패밀리 |
| 천장 | 별도 타일 없음 | 다음 층 Floor가 구조 슬래브 역할 |
| 지붕 장식 | Vent 1개 | 변환 후 고유 roof vent |
| 외부 문 | 2개 | 서쪽 1개, 동쪽 1개 |

## 시스템 적합성 평가와 보강 내용

처음 구현 상태는 일반적인 4×4 이상 건물에는 적합했지만 이 LT1 레퍼런스에는 부족한 점이 있었습니다. 이번 작업에서 다음을 보강했습니다.

| 기존 간극 | 적용한 해결책 | 이유 |
|---|---|---|
| 2×3 footprint 거부 | 최소 크기를 2×2로 낮추고 소형 코어 우회 링 생략 | 실제 레퍼런스가 2×3이며, 작은 격자에서 우회 링이 모든 방 셀을 소비했음 |
| 모든 층에 같은 파사드 | `Ground/Middle/Top/Roof` FloorBand 추가 | LT1이 ground, repeat, top 전용 메시를 사용 |
| 코너를 일반 외벽으로 처리 | `ExteriorCorner`와 corner span 예약 추가 | 코너 메시가 900cm 양쪽 벽을 함께 점유 |
| 계단 셀에도 일반 바닥 | `FloorOpening` 추가 | 상층마다 `SM_LT1_Floor_Hole` 필요 |
| 별도 천장 중복 | `bGenerateCeilingTiles=false` 지원 | LT1은 다음 층 floor가 슬래브/천장 역할 |
| 지붕/벤트 의미 없음 | `Roof`, `RoofDecoration`, count 추가 | roof 6개와 vent 1개 재현 |
| `_W` 메시 손실 | Variant에 `AdditionalStaticMeshes` 추가 | 한 semantic placement에서 베이스+보조 레이어를 함께 출력 |
| 문 메시 또는 Actor 중 하나만 출력 | 메시와 Actor를 함께 유지하고 `bInteractive` 분리 | Doorway 구조와 실제 문 액터를 동시에 유지 |
| 출입구 한 개 고정 중앙 | 주 출입구 위치 + 보조 출입구 배열 추가 | 레퍼런스의 서/동 ground door 2개 반영 |
| PCG에서 레이어/상호작용 정보 손실 | `FloorBand`, `Interactive` metadata 및 레이어별 point 출력 | PCG Graph에서도 직접 Generator Actor와 같은 해석 가능 |

따라서 현재 시스템은 **LT1 스타일의 모듈러 건물과 의미 기반 실내 생성에는 적합**합니다. 반면 원본 `BP_P_Building`의 모든 시각 변형을 똑같이 재현하는 완전 대체품은 아직 아닙니다.

### 최초 설계 보고서 대비 판정

첨부된 「범용 모듈러 건물 및 실내 인테리어 PCG 시스템 설계 보고서」 전체 요구를 기준으로 보면 현재 구현은 **작동하는 기반/MVP에는 적합하지만 최종 프로덕션 완료 상태는 아닙니다.** 범위를 구분하면 다음과 같습니다.

| 상태 | 항목 |
|---|---|
| 구현 | 규칙과 에셋의 분리, 수직 코어 고정, 1층/반복층 방 가중치, 세 가지 반복층 모드, Seed 계층, 의미 placement, 기능 가구 세트, 필수 세트 롤백, 문/창 접근 예약, HISM/Actor 분리 |
| 부분 구현 | 방 분할은 직사각 그리드와 직선 복도 중심, 이동 가능성은 예약 사각형과 직접 복도 연결로 검사, 전방 축은 Variant별 enum 대신 로컬 +X 규약과 PlacementOffset으로 보정, 반복층은 결정론적으로 다시 계산하지만 결과 캐시는 없음 |
| 미구현 | 비정형 footprint/spline, 모듈 인접 조건과 ConnectionType/RequiredLength, 생성 후 NavMesh 경로 검증, 책상 상판·선반 같은 배치 표면, 점유/파손/스타일 상태, 머티리얼 Variant, 작은 프롭용 전용 단계, 반복층 평면 캐시 |

즉 이 상태에서 범용성의 방향은 맞고 LT1 예제도 실행되지만, 보고서의 모든 품질 목표를 충족했다고 판단하면 안 됩니다. 위 미구현 항목은 에셋 팩을 늘리거나 실제 플레이 레벨에 투입하기 전에 우선순위를 정해 확장해야 합니다.

## 생성한 다섯 데이터 에셋

### 1. `DA_BuidlingModuleSet`

경로의 `Buidling` 철자는 요청한 기존 에셋명을 그대로 유지했습니다.

| Semantic | FloorBand | 에셋/처리 |
|---|---|---|
| Floor | Any | `SM_LT1_Floor` |
| FloorOpening | Any | `SM_LT1_Floor_Hole` |
| Roof | Roof | `SM_LT1_Roof1` |
| ExteriorCorner | Ground | `SM_LT1_Wall_C_B_L1` |
| ExteriorCorner | Middle | `SM_LT1_Wall_C_L1` + `SM_LT1_Wall_C_L1_W` |
| ExteriorCorner | Top | `SM_LT1_Wall_C_T_L1` |
| ExteriorWall | Ground/Middle/Top | `Wall_B1` / `Wall1` / `Wall_T1` |
| InteriorWall | Any | `SM_LT1_Wall1` 예제 fallback |
| Window | Ground/Middle | `Window1` + `Window1_W` |
| Window | Top | `Window_T1` |
| Door | Ground | `Doorway1` + `BP_LT1_Door1`, interactive |
| Door | Middle/Top | `Doorway1` 정적 구조 |
| Stair | Any | `BP_Stairs_Steps1_2XX` Actor |
| RoofDecoration | Roof | `SM_Vent_Roof_Set1` |

메시 bounds를 직접 읽어 `NominalSize`와 `PlacementOffset`을 기록했습니다. Floor는 실제 bounds가 `900×900cm`이고 피벗이 모서리에 있어 XY 중심 보정이 필요합니다. 코너는 원본이 외곽 모서리 피벗을 사용하므로 XY 보정을 하지 않았습니다. 파사드 메시에는 레퍼런스의 `1.0025` 균일 스케일을 적용해 틈을 줄였습니다.

### 2. `DA_BuildingRule`

| 설정 | 값 | 이유 |
|---|---:|---|
| CellSize / FloorHeight | 900 / 500cm | 레퍼런스 실측 |
| CorridorWidth | 1 cell | 2×3 소형 footprint에서 가용 방 유지 |
| RoomLength | 1–2 cells | 소형/확장 footprint 모두 대응 |
| RepeatFloorVariation | Identical | 원본 반복층 파사드 문법 유지 |
| Main Stair | 1×1, normalized 0.5/0.5 | 동일 코어 셀을 모든 층에서 공유 |
| Main Entrance | East, 0.5 | 동쪽 중앙 doorway |
| Additional Entrance | West, 0.0 | 서쪽 하단 doorway |
| Ceiling / Roof | false / true | 중복 천장 방지, roof 생성 |
| RoofDecorationCount | 1 | vent 하나 |
| Dedicated Corners | true, span 1 | 코너 메시와 일반 edge 중복 방지 |
| Window chance / spacing | 0.6 / 1 | 작은 파사드에서도 창 생성 가능 |

1층은 Living/Kitchen/Storage, 반복층은 Bedroom/Living/Storage 가중치를 사용합니다. 원본 건물 자체는 실내 용도를 정의하지 않으므로, 이 부분은 PostABundle의 주택 가구군과 잘 맞는 **예시 설계 선택**입니다.

### 3. `DA_PropSet`

- Table: Livingroom/Kitchen table, 중앙 앵커
- Sofa: Couch 1/2, 벽 앵커, Television 참조
- Television: New/Old, 벽 앵커, 방 중심을 바라봄
- Bed: 2X1/4X1, Bedroom 벽 앵커
- Shelf: Shelf 1/2, Storage/Living/Bedroom 벽 앵커
- Cabinet: Kitchen Desk 2X, Kitchen/Storage 벽 앵커
- Chair: Basic/Office Chair, Table 참조 앵커
- Decoration: Painting 1/2, 벽 앵커

각 `FootprintSize`는 메시 bounds에서 계산했고, 소파·침대·수납장의 전면 동선 여유를 더 크게 지정했습니다. 따라서 단순 랜덤 산포가 아니라 문 접근, 주 동선, 다른 가구와의 여유 공간 검사를 통과해야 배치됩니다.

### 4. `DA_InteriorRule`

| 기능 세트 | 필수 | 선택 |
|---|---|---|
| `LT1_LivingSet` | TV, Sofa | Table, Painting |
| `LT1_KitchenSet` | Cabinet | Table, Chair 2개 |
| `LT1_BedroomSet` | Bed | Shelf, Painting |
| `LT1_StorageSet` | Shelf | Cabinet |

주 동선 폭 100cm, 문 접근 깊이 120cm, 창 접근 깊이 60cm, 배치 재시도 24회, 디테일 확률 0.3으로 설정했습니다. 필수 멤버가 하나라도 들어가지 않으면 세트 전체를 롤백하므로 기능이 깨진 반쪽 가구 묶음이 남지 않습니다.

### 5. `DA_GenerationProfile`

위 네 에셋을 다음과 같이 연결했습니다.

- BuildingRules → `DA_BuildingRule`
- InteriorRules → `DA_InteriorRule`
- BuildingModules → `DA_BuidlingModuleSet`
- InteriorProps → `DA_PropSet`

## 사용 방법

### 레퍼런스 크기로 Generator Actor 사용

1. C++ 구조체가 변경되었으므로 이미 열려 있던 에디터는 한 번 완전히 재시작합니다. 외부 저장 알림이 나타나면 디스크의 새 버전을 다시 로드합니다.
2. 레벨에 `InteriorPCGGeneratorActor`를 배치합니다.
3. Profile을 `/Game/DevB/PCG/DA_GenerationProfile`로 지정합니다.
4. Footprint를 `X=1800, Y=2700`, NumFloors를 `4`, BuildingSeed를 `555011868`로 설정합니다.
5. 원본 변환 결과 위에 비교 배치하려면 액터 위치를 대략 `(20460, -10710, 0)`에 둡니다. 이는 네 외곽 코너의 중심입니다.
6. Details의 `Generate`를 실행합니다.

정적 반복 메시와 추가 레이어는 메시별 HISM으로 묶입니다. Ground Door는 doorway 메시와 ChildActor가 같이 생성됩니다. Stair는 원본 계단 Blueprint Actor를 유지합니다.

### PCG Graph 사용

1. `Generate Building + Interior Layout` 노드를 추가합니다.
2. 같은 Profile과 Generation Options를 지정합니다.
3. `Structure` 출력은 `InteriorPCG.AssetPath`를 읽는 Static Mesh Spawner로 연결합니다.
4. `InteriorPCG.ActorClassPath`가 유효한 포인트는 Spawn Actor 분기로 보냅니다.
5. `InteriorPCG.Interactive`, `FloorBand`, `ModuleType`, `AllowInstancing`으로 필요한 출력을 필터링합니다.
6. 추가 메시 레이어는 별도 point로 이미 펼쳐집니다. ActorClassPath는 첫 레이어에만 기록되므로 Actor가 중복 생성되지 않습니다.

## 검증 결과

레퍼런스 옵션 `1800×2700cm`, 4층, Seed `555011868`으로 데이터 에셋을 다시 로드해 생성한 결과는 다음과 같습니다.

- 성공, 오류 0, 경고 0
- 방 7개, 포털 9개, 전체 placement 94개
- Interior placement 16개
- 구조 핵심: Floor 21, FloorOpening 3, Roof 6, ExteriorCorner 16, Stair 3
- 동/서 외부 출입구 2개
- 결정론적 LayoutHash: `1319153355`

자동화 테스트 네 개도 모두 통과합니다.

- `InteriorPCG.Generation.Determinism`
- `InteriorPCG.Generation.PostABundleLT1Structure`
- `InteriorPCG.Generation.SemanticInteriorWithoutMeshes`
- `InteriorPCG.Generation.VerticalCoreAndRepeatFloor`

## 남은 차이와 권장 확장

1. **머티리얼**: 원본은 MID를 다수 생성해 벽/바닥 색과 재질을 바꿉니다. 현재 Variant는 메시 중심입니다. `MaterialVariantSet`과 PCG material override attribute를 추가하는 것이 좋습니다.
2. **정확한 면 패턴**: 현재 창문은 확률/간격 규칙입니다. 원본처럼 면마다 정확한 doorway/window 배열을 재현하려면 `FacadePattern` 데이터 에셋을 추가해야 합니다.
3. **보도와 외부 산란**: Sidewalk HISM, ivy, pillar, floor scatter는 이번 다섯 에셋의 범위에서 제외했습니다. 별도 ExteriorDetailSet으로 분리하는 편이 유지보수에 유리합니다.
4. **비정형 건물**: 솔버 외곽은 직사각형 셀입니다. L자/다각형은 footprint mask 또는 spline rasterizer가 필요합니다.
5. **비주얼 튜닝**: 계단/문 Blueprint의 자체 피벗과 forward axis는 실제 레벨에서 한 번 확인하고 Variant PlacementOffset/Yaw를 미세 조정해야 합니다.

즉, 이번 예제는 LT1의 구조 문법과 실내 기능을 검증하는 **작동하는 기준 프로필**입니다. 원본 블루프린트의 시각적 스냅샷을 그대로 굳히는 용도라면 변환 후 StaticMeshActor 묶음이 더 직접적이고, 시드·크기·용도에 따라 계속 재생성하려면 이 PCG 프로필이 더 적합합니다.
