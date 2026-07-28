# `M_Foliage` 실제 게임 사용 및 비용 감사 보고서

- 대상: `/Script/Engine.Material'/Game/PostABundle/Materials/MasterMaterials/M_Foliage.M_Foliage'`
- 조사 기준: UE 5.7.4, 2026-07-28 현재 워크스페이스
- 조사 방식: UE Asset Registry 하드/소프트/관리/검색 참조 그래프, Material/Material Instance 파라미터, 그래프 노드, 패키징 설정을 읽기 전용으로 조사

## 1. 결론

현재 패키징 설정과 자산 참조 그래프 기준으로 `M_Foliage` 및 그 자식 Material Instance 16개는 실제 게임 Cook 루트에서 도달하지 않는다. 따라서 현재 Shipping 빌드에서 이 계열이 차지하는 GPU 실행 시간, 런타임 메모리, 패키지 용량은 사실상 0으로 판단한다.

가장 효과적인 최적화는 머티리얼 그래프를 즉시 뜯어고치는 것이 아니라 다음 두 가지다.

1. 이 계열이 게임 Cook에 들어오지 않는 상태를 CI 또는 Cook 감사로 유지한다.
2. 향후 실제 게임에 채택할 때만 목적별 경량 머티리얼로 분리하고, 사용하지 않는 Wetness·Gameplay Bending·Advanced Wind를 정적 스위치로 끈다.

현재 그래프를 단순화해도 Shipping FPS나 메모리는 줄지 않는다. 반면 콘텐츠 팩 샘플 맵과 수백 개 배치 자산의 외형을 깨뜨릴 위험은 있다.

## 2. 실제 사용 경로 추적

### 2.1 Cook 루트

프로젝트는 `bCookAll=False`이며 다음 5개 맵만 `MapsToCook`에 명시되어 있다.

- `/Game/Map/L_Transition`
- `/Game/Map/L_MainMap`
- `/Game/Map/L_LobbyMap`
- `/Game/Map/L_TestMap`
- `/Game/Map/L_HomeMap`

항상 Cook되는 디렉터리는 `/NNEDenoiser`, `/Game/GameAbilitySystem` 두 곳뿐이다. `/Game/PostABundle`은 포함되지 않는다.

UE Asset Registry로 위 5개 맵에서 정방향 의존성 폐쇄를 계산한 결과는 다음과 같다.

| 검사 | 탐색된 `/Game` 패키지 | `M_Foliage` 또는 자식 MI 도달 | 탐색 중단 |
|---|---:|---:|---:|
| 하드 패키지 참조 | 3,725 | 0 / 17 | 아니오 |
| 하드·소프트·관리·검색 참조 전체 | 4,065 | 0 / 17 | 아니오 |

`Config`, `Source`, `Docs`, `Documentation`에도 대상 경로의 문자열 참조가 없다. 런타임에서 문자열을 조립해 `LoadObject`하는 비정형 경로까지 절대적으로 배제할 수는 없지만, 소스 검색과 Asset Registry 모두에서 근거가 발견되지 않았다.

### 2.2 콘텐츠 팩 내부 사용

대상은 미사용/고아 자산은 아니다. 콘텐츠 팩 내부에서는 다음과 같이 적극적으로 사용된다.

- 직계 자식 MI: 8개
- 전체 자식 MI: 16개
- MI의 직접 Static Mesh 참조: 44개
- 외부 액터/Blueprint 계열 직접 참조: `BP_WoodenFence_C` 168개, `BP_Highway1_C` 4개
- 하드 참조가 이어지는 `.umap`: 3개
  - `/Game/PostABundle/Maps/Showcase`
  - `/Game/PostABundle/Geometry/MAAS/LevelInstances/L_Build_AP1_Inside2`
  - `/Game/PostABundle/Geometry/MAAS/LevelInstances/L_Build_AP1_Inside3`

이 3개 맵은 현재 Cook 목록에 없다. 즉 “콘텐츠 팩 데모/제작 자산에는 필요하지만 실제 게임 빌드에는 필요하지 않은” 상태다.

## 3. 비용 구조

### 3.1 머티리얼 기본 속성

| 항목 | 값 | 비용 의미 |
|---|---|---|
| Domain | Surface | 일반 표면 머티리얼 |
| Blend | Masked | 알파 테스트와 foliage overdraw 비용 |
| Shading Model | Two Sided Foliage | 양면 조명 및 subsurface 계열 비용 |
| Two Sided | True | 뒷면까지 래스터/셰이딩 |
| Material Attributes | True | 통합 attribute 그래프 |
| Dithered LOD Transition | True | LOD 전환 시 디더 비용 |
| Opacity Mask Clip | 0.3333 | Masked 경계값 |
| WPO | 연결됨 | 바람/상호작용을 위한 vertex 비용 |

이 조합은 실제 렌더링된다면 저가 머티리얼이 아니다. 특히 Masked + Two Sided + WPO는 식생에서 픽셀 overdraw와 vertex 작업을 동시에 발생시킨다. 다만 현재 게임 Cook에는 들어오지 않으므로 이 비용은 콘텐츠 팩 샘플을 열거나 편집할 때만 발생한다.

### 3.2 그래프 규모

- 메인 그래프 계산 노드: 139개, 코멘트 16개
- Scalar Parameter: 65개
- Vector Parameter: 11개
- Texture Parameter: 7개
- Static Switch Parameter: 33개
- 16개 MI가 실제로 만드는 정적 스위치 조합: 11개
- 직접 호출 Material Function:
  - `MF_AdvancedWind`: 175개 노드
  - `MF_Wetness`: 84개 노드
  - `SimpleGrassWind`: 64개 노드
  - `MF_CheaperNormalBlend`: 7개 노드

33개 스위치가 이론적으로 `2^33` 셰이더를 만드는 것은 아니다. 현재 저장된 MI가 요구하는 유효 조합은 11개다. 각 조합은 다시 Nanite, Static Mesh, Instanced Static Mesh, Spline Mesh 및 렌더 패스에 맞는 셰이더를 요구할 수 있다.

### 3.3 텍스처 및 소스 자산 규모

| 범위 | 소스 파일 크기 |
|---|---:|
| `M_Foliage.uasset` | 103,788 B |
| 자식 MI 16개 합계 | 337,848 B |
| 프로젝트 소유 함수 3개 합계 | 208,613 B |
| 기본 텍스처 3개 합계 | 9,060,731 B |
| MI 유효 텍스처를 포함한 관찰 계열 65패키지 | 163,424,706 B (155.85 MiB) |

마지막 수치는 “이 머티리얼 계열에서 보이는 소스 자산 집합”이며, 독점 소유 용량이나 실제 Cook 용량이 아니다. 텍스처와 함수가 다른 머티리얼에서도 사용되므로 155.85 MiB 전체를 삭제 가능한 용량으로 해석하면 안 된다. 현재 Cook에서는 대상 계열 자체가 도달되지 않으므로 Shipping 절감 가능량도 155.85 MiB가 아니라 현재 기준 0이다.

## 4. 줄일 수 있는 부분

### P0 — 현재 Cook 비포함 상태를 보장

효과가 가장 크고 위험이 가장 낮다.

- CI에서 5개 Cook 맵의 Asset Registry 의존성에 `M_Foliage` 또는 16개 MI가 나타나면 실패하도록 검사한다.
- `/Game/PostABundle` 전체를 `DirectoriesToNeverCook`로 막는 방식은 사용하지 않는다. `MF_Wetness`, `MF_CheaperNormalBlend`, `MF_AdvancedWind`는 다른 머티리얼에서도 공유되고, 특히 `/Game/Materials/M_Landscape`가 앞의 두 함수를 참조한다.
- 콘텐츠 팩을 정리할 경우 공용 함수와 MPC를 먼저 프로젝트 소유 경로로 옮긴 후, 식생 전용 자산만 격리한다.

현재 상태에서는 이 조치만으로 Shipping 런타임 비용 0을 유지할 수 있다.

### P1 — 실제 게임에 채택할 때 Wetness를 우선 판단

16개 MI 모두 `UseWetness?=True`다. `MF_Wetness`는 84개 노드, 3개의 Texture Sample, Time/World Position/Normal 연산과 `MPC_Weather`를 사용한다. `MPC_Weather` 기본값도 `WetnessLevel=0.8`, `WaterDrops=1.0`이므로 기본적으로 시각 효과가 꺼져 있는 경로가 아니다.

실제 게임 식생에 젖음/물방울 표현이 필요 없다면 production MI에서 `UseWetness?=False`로 고정하는 것이 가장 명확한 픽셀 비용 절감 후보다. 정적 스위치이므로 해당 함수 경로가 셰이더에서 제거된다. 콘텐츠 팩의 공유 `MF_Wetness` 자체는 다른 18개 머티리얼 계열이 참조하므로 삭제하지 않는다.

### P1 — Gameplay Bending 사용 여부 확정

`CharacterAffectFoliage?`는 16개 중 5개 MI에서만 켜져 있다. 이 경로는 `MPC_PlayerInteraction`의 `InfluencerPosition`, `EffectRadius`, `Strenght`와 Distance 연산을 통해 WPO를 수정한다.

`MPC_PlayerInteraction`을 직접 참조하는 제어 자산은 콘텐츠 팩의 `BP_ExamplePlayer`, `BP_P_VehiclePawn`뿐이며 현재 게임 Cook 경로에 대상 foliage가 없다. production 플레이어가 이 MPC를 갱신하지 않는다면 모든 production MI에서 이 스위치를 끄는 것이 맞다. 그러면 불필요한 per-vertex 거리/상호작용 계산을 제거할 수 있다.

### P1 — Advanced Wind는 나무 전용으로 한정

`UseAdvancedWind?`는 Pivot Painter 나무 계열 6개 MI에서만 켜져 있다. 함수는 175개 노드와 Pivot Painter 텍스처를 사용하며 1~4단계 회전 애니메이션을 지원한다. 일반 풀/관목에는 Simple Grass Wind 경로만 유지하고, Advanced Wind는 나무 전용 마스터로 분리하는 편이 안전하다.

분리는 현재 정적 스위치가 이미 수행하는 런타임 가지 제거보다도 셰이더 키·파라미터 관리·Cook 의존성을 단순화하는 데 의미가 있다.

### P2 — 항상 꺼진 11개 기능과 죽은 override 정리

현재 16개 MI에서 항상 False인 스위치는 다음과 같다.

- `UseDetailNormal?`
- `UseStaticDrops`
- `UseVerticalDrops`
- `UseTriplanarMapping`
- `UseVertexPaintedAO? (BLUE)`
- `Manually Control Pivot Painter 2 Foliage Animation Time`
- `Expose Previous Frame Time`
- `Animate Level 1 Normals`
- `Animate Level 2 Normals`
- `Animate Level 3 Normals`
- `Animate Level 4 Normals`

Static Switch이므로 이 경로들의 GPU 연산은 이미 컴파일 단계에서 제거된다. 그래프에서 지운다고 현재 GPU 시간이 더 줄지는 않는다. 절감 대상은 불필요한 파라미터·셰이더 키 복잡도·하드 자산 참조다.

특히 `UseDetailNormal?=False`인데도 9개 MI가 `DetailNormalT`를 override하며, 대부분 `T_Water_N`을 하드 참조한다. 해당 texture override와 `DetailNormalMultiplier`, `DetailTiling` override를 제거하면 시각 결과를 바꾸지 않고 MI 의존성과 데이터 노이즈를 줄일 수 있다. 단, `T_Water_N`은 공용 텍스처이므로 override 제거만으로 파일 자체를 삭제할 수 있다는 뜻은 아니다.

항상 True인 `UseWetness?`, `UseRoughnessT`도 이 16개 MI만 지원할 전용 마스터라면 분기 대신 True 경로로 고정할 수 있다. 다만 `UseWetness?`는 실제 비용이 있는 기능이므로 먼저 유지 여부를 결정해야 한다.

### P3 — Usage Flag 정리

현재 켜진 주요 Usage Flag는 Static Mesh, Instanced Static Mesh, Spline Mesh, Nanite, Static Lighting이다.

- `Used With Nanite`: 유지. 직접 참조된 Static Mesh 44개가 모두 Nanite Enabled다.
- `Used With Instanced Static Meshes`: foliage 배치 방식상 유지하는 편이 안전하다.
- `Used With Spline Meshes`: 라이트폴 spline mesh와 fence/highway Blueprint 참조가 있어 검증 없이 끄지 않는다.
- `Used With Static Lighting`: 프로젝트가 `r.AllowStaticLighting=False`이므로 게임에는 불필요하다. 플래그를 끄는 것은 안전한 정리 후보지만, 전역 설정이 이미 static-lighting 셰이더를 억제하므로 실제 절감 폭은 작거나 0일 수 있다.
- `Cast Ray Traced Shadows`: 프로젝트가 `r.RayTracing=False`이므로 현재 런타임 비용이 없다.

## 5. 권장 실행안

### 이 콘텐츠 팩 식생을 게임에서 사용하지 않을 경우

1. 머티리얼은 수정하지 않는다.
2. Cook 의존성 검사만 추가해 비포함 상태를 보장한다.
3. 에디터 저장소 용량까지 줄여야 할 때만 `M_Foliage` 전용 MI·mesh·texture 묶음을 별도 보관소로 이동한다.
4. 공용 함수와 MPC는 다른 프로젝트 머티리얼 참조를 먼저 분리하지 않는 한 삭제하거나 `/Game/PostABundle` 전체와 함께 제외하지 않는다.

### 일부 식생을 게임에 도입할 경우

1. 사용할 mesh/MI만 프로젝트 소유 경로로 이관한다.
2. 목적별로 `Simple Foliage`, `Interactive Foliage`, `Pivot Painter Tree` 3개 마스터로 분리한다.
3. Wetness가 필요 없으면 가장 먼저 제거한다.
4. Gameplay Bending은 MPC 갱신 주체가 있는 경우에만 켠다.
5. 항상 꺼진 스위치와 Detail Normal override를 제거한다.
6. 변경 전후를 `Material Stats`, Shader Compile 통계, ProfileGPU 및 실제 Cook 크기로 측정한다.

## 6. 검증 기준과 한계

- 본 조사는 현재 Asset Registry와 작업 트리의 자산을 기준으로 했다.
- 실제 Cook 산출물(`Saved/Cooked`)이 없어 플랫폼별 cooked byte와 Shader Library 크기는 측정하지 않았다.
- UE 5.7.4 Python의 `GetInputsForMaterialExpression`이 이 그래프의 일부 입력에서 null dereference로 commandlet을 종료시키는 문제가 있어, 개별 노드의 active/disconnected 판정은 보고서 근거로 사용하지 않았다. 대신 출력 연결, 정적 스위치 유효값, 함수/텍스처/참조 그래프를 사용했다.
- 최종 삭제 또는 master 분리는 별도 작업으로 수행하고, 5개 Cook 맵 smoke test와 패키지 실행 검증을 통과해야 한다.

## 7. 최종 판정

| 항목 | 판정 |
|---|---|
| 현재 실제 게임 필요성 | 없음 — 지정 Cook 루트에서 도달하지 않음 |
| 현재 Shipping GPU/메모리 비용 | 사실상 0 |
| 현재 Shipping 용량 비용 | 사실상 0 |
| 에디터/콘텐츠 팩 비용 | 있음 — 139-node master, 11개 MI permutation, 대규모 팩 참조 |
| 즉시 권장 변경 | Cook 비포함 상태 자동 검증 |
| 그래프 최적화 시점 | 실제 게임 도입이 결정된 후 |
| 가장 큰 잠재 GPU 절감 | Wetness 제거, 불필요한 Gameplay Bending 제거, Advanced Wind의 나무 전용화 |
| 가장 안전한 데이터 정리 | 항상 꺼진 Detail Normal의 9개 MI override 제거 |
