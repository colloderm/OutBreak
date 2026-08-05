# Interior PCG

`InteriorPCG`는 모듈러 메쉬 이름이 아니라 의미와 관계를 먼저 생성하는 UE 5.7용 건물/실내 절차 생성 플러그인입니다. 건물 규칙과 실제 에셋 목록이 분리되어 있어 같은 규칙에 주택, 오피스, 병원, 공장용 에셋 세트를 교체할 수 있습니다.

## 구현 범위

- 직사각형 그리드 기반 다층 건물과 1층/반복층 전용 방 분류
- 모든 층에서 같은 셀을 쓰는 계단, 엘리베이터, 설비 샤프트
- `Identical`, `SeededPerFloor`, `PatternCycle` 반복층 변형
- 바닥, 계단 홀, 선택적 천장, 지붕, 외벽, 전용 코너, 내벽, 문, 창문, 수직 코어와 지붕 장식 의미 신호
- `Ground`, `Middle`, `Top`, `Roof` 층 구간별 모듈 선택과 같은 위치의 복합 메시 레이어
- 주 출입구와 위치를 지정할 수 있는 복수 보조 출입구
- 방-복도 직접 연결과 문/창문 접근 영역 예약
- 중앙/벽/참조 가구/문/창문 앵커와 Look-at 방향 계산
- 필수/선택 멤버가 있는 기능 가구 세트와 세트 단위 롤백
- `BuildingSeed -> FloorSeed -> RoomSeed -> DetailSeed` 결정론적 시드 계층
- 정적 반복 메쉬의 HISM 출력과 상호작용 Actor 분리 출력
- 네이티브 PCG 그래프 노드 및 Blueprint 생성 API

## 데이터 에셋 구성

콘텐츠 브라우저에서 `Miscellaneous > Data Asset`을 선택해 다음 클래스를 만듭니다.

1. `InteriorPCGBuildingRuleSet`: 셀 크기, 층 높이, 복도, 방 크기, 층 변형, 수직 코어, 방 타입 가중치
2. `InteriorPCGInteriorRuleSet`: 기능 가구 세트, 동선 폭, 문/창문 접근 깊이, 배치 재시도와 디테일 밀도
3. `InteriorPCGBuildingModuleSet`: 구조 의미별 모듈러 메쉬 또는 Actor variant
4. `InteriorPCGPropSet`: 가구 의미별 에셋, 앵커, 방향, 참조 대상과 여유 공간
5. `InteriorPCGGenerationProfile`: 위 네 에셋을 하나로 묶는 실행 프로필

`BuildingModuleSet`이 없어도 구조 신호는 생성됩니다. `InteriorPropSet`의 variant가 비어 있어도 의미 가구 신호는 유지되므로, 규칙을 먼저 검증하고 에셋을 나중에 연결할 수 있습니다.

## 에셋 축과 피벗 규약

- 구조 벽/문/창문은 로컬 X축 방향으로 한 셀 길이를 차지한다고 가정합니다.
- 가구의 로컬 +X축은 앞쪽입니다. Look-at 계산은 +X축을 대상으로 향하게 만듭니다.
- 바닥 배치 가구와 구조 모듈의 피벗은 바닥 높이에 두는 것을 권장합니다.
- 팩의 축이나 피벗이 다르면 각 `AssetVariant.PlacementOffset`에서 보정합니다.
- `NominalSize`는 PCG point bounds와 검증용 크기입니다. 셀 크기에 맞는 모듈을 등록해야 합니다.

## 사용 방법 A: Generator Actor

1. 레벨에 `InteriorPCGGeneratorActor`를 배치합니다.
2. `Profile`, `Footprint`, `NumFloors`, `BuildingSeed`를 지정합니다.
3. Details 패널에서 `Generate`를 실행합니다.
4. 반복 가능한 정적 메쉬는 메쉬별 HISM으로 묶이고, `ActorClass` variant는 자식 Actor로 생성됩니다.
5. 다시 생성하기 전에 기존 출력은 자동으로 정리됩니다. `Clear Generated`로 수동 정리할 수도 있습니다.

런타임이나 Blueprint에서는 `Generate`를 호출하고 `LastResult` 또는 `OnGenerated`를 사용합니다.

## 사용 방법 B: PCG Graph

PCG Graph에 `Generate Building + Interior Layout` 노드를 추가하고 `Profile`과 생성 옵션을 설정합니다. 노드는 세 출력 핀을 제공합니다.

| 핀 | 내용 |
|---|---|
| `Structure` | 바닥, 천장, 벽, 문, 창문, 수직 코어 placement point |
| `Interior` | 기능 가구, 디테일, 상호작용 Actor placement point |
| `Rooms` | 방 중심, bounds, 타입, 면적, 연결 상태 point |

주요 metadata attribute는 다음과 같습니다.

| Attribute | 형식 | 용도 |
|---|---|---|
| `InteriorPCG.AssetPath` | Soft Object Path | Static Mesh Spawner의 메쉬 selector |
| `InteriorPCG.ActorClassPath` | Soft Object Path | Spawn Actor 분기 |
| `InteriorPCG.ModuleType` | Name | 구조 의미 필터 |
| `InteriorPCG.PropType` | Name | 가구 의미 필터 |
| `InteriorPCG.RoomType` | Name | 방 타입 필터 |
| `InteriorPCG.AnchorType` | Name | 배치 근거 디버깅 |
| `InteriorPCG.FloorBand` | Name | Ground/Middle/Top/Roof 에셋 분기 |
| `InteriorPCG.FloorIndex` | Integer | 층별 분기 |
| `InteriorPCG.RoomID` | Integer | 방별 파티션 |
| `InteriorPCG.SetID` | Name | 기능 가구 세트 추적 |
| `InteriorPCG.AllowInstancing` | Boolean | HISM/Actor 출력 분기 |
| `InteriorPCG.Interactive` | Boolean | 상호작용 Actor 분기 |

Static Mesh Spawner가 `InteriorPCG.AssetPath`를 읽도록 selector를 설정하고, `ActorClassPath`가 유효한 포인트는 Spawn Actor 쪽으로 분기합니다.

## Blueprint API

- `InteriorPCGGenerationLibrary.Generate`: UObject를 생성하지 않는 결정론적 배치 계산
- `InteriorPCGGenerationLibrary.ValidateProfile`: 필수 규칙과 잘못된 참조 검사
- `InteriorPCGGenerationLibrary.MakeSeedBundle`: 하위 PCG 그래프와 동일한 시드 파생

`FInteriorPCGGenerationResult`에는 방, 문/창문 포털, 최종 placement, 경고와 `LayoutHash`가 들어 있습니다.

## 검증

에디터 자동화 테스트 이름은 `InteriorPCG.Generation`입니다. 명령행 예시는 다음과 같습니다.

```powershell
UnrealEditor-Cmd.exe OutBreak.uproject /Engine/Maps/Entry -unattended -nullrhi `
  "-ExecCmds=Automation RunTests InteriorPCG" "-TestExit=Automation Test Queue Empty"
```

현재 솔버의 건물 외곽은 직사각형 셀 footprint입니다. 비정형 폴리곤이나 기존 벽 spline을 입력하려면 별도 footprint rasterizer를 앞단에 추가하고 같은 의미 신호/에셋 해석 단계를 재사용하는 방식으로 확장할 수 있습니다.

Post-Apocalypse Building Bundle의 `BP_P_Building`/`BP_Building_LT1` 분석과 `/Game/DevB/PCG` 예제의 설정 근거는 [LT1 예제 상세 가이드](Docs/PostABundle_LT1_Example_Guide.md)를 참고하십시오.
