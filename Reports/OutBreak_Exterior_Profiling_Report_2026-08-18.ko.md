# OutBreak_Exterior 레벨 프로파일링 보고서

- 측정일: 2026-08-18 (Asia/Seoul)
- 대상: `/Game/Maps/OutBreak_Exterior`
- 엔진: Unreal Engine 5.7.4, Development WindowsEditor
- RHI: Direct3D 12, Shader Model 6, 하드웨어 레이 트레이싱 비활성
- 시스템: Intel Core i5-13600K, NVIDIA GeForce RTX 4070 SUPER 12GB, RAM 32GB
- 성능 조건: 1920×1080, Screen Percentage 100%, Dynamic Resolution/VSync/프레임 제한 비활성, 오프스크린 Standalone

## 1. 결론

현재 레벨은 **성능 이전에 안정성 때문에 출시 가능 판정을 내릴 수 없다.** 헬기 삽입 경로로 실제 맵 상공을 이동하는 동안 AI 스포너가 스트리밍되어 풀 워밍업을 시작하면, 적 캐릭터 초기화에서 동일한 접근 위반이 두 번 재현됐다.

크래시 전 유효 구간만 보면 이 PC의 1080p 기준으로 60 FPS는 여유 있게 충족했다. 워밍업 300프레임을 제외한 1,039프레임은 평균 10.279 ms(97.3 FPS), 1% low 63.6 FPS였다. 다만 적 AI가 정상 생성·전투하지 못한 상태이고 Development Editor 빌드 측정이므로, 이것은 **헬기 상공/삽입 UI/스트리밍의 부분 기준선**이지 실제 플레이 성능 인증값이 아니다.

우선순위는 다음과 같다.

1. P0: AI 풀 워밍업 크래시와 적 데이터 누락을 수정한다.
2. P0: 수정 후 AI 활성 상태로 동일한 1,800프레임 캡처를 정상 종료한다.
3. P1: 초기 PSO 미스와 헬기 이동 경로의 World Partition 스트리밍 지연을 줄인다.
4. P1: 오래된 Landmass/Water 외부 액터 참조와 엔진 버전 없는 에셋을 정리한다.
5. P2: 높은 Editor 메모리, 렌더 타깃 풀, Lumen Mesh SDF 오브젝트 수와 하드 레퍼런스를 점검한다.

## 2. 측정 시나리오와 유효성

| 시나리오 | 결과 | 보고서 사용 범위 |
|---|---:|---|
| 기본 URL, 1920×1080, 1,800프레임 | 정상 종료했으나 PlayerStart가 없어 카메라가 Z=-65,738까지 낙하 | FPS 판정에서 제외. 기본 프로파일 진입 경로 오류 확인에만 사용 |
| `HelicopterInsertion=1`, 1920×1080 | 헬기 탑승·ViewTarget 설정·상공 이동 성공, CSV 1,339프레임 후 크래시 | 크래시 전 1,039 steady 프레임을 부분 기준선으로 사용 |
| AI hard cap 실행 한정 오버라이드 재시도 | 외부 설정 오버라이드가 개별 스포너 등록 경로를 차단하지 못했고 같은 크래시 재현 | 크래시 재현성 확인에 사용 |
| 진단 캡처 + `profilegpu`/`memreport -full` | 기본 경로에서 정상 생성. 실제 렌더 해상도 888×500 | GPU 이벤트 상대 순위와 메모리 구성 참고용. 1080p 절대 시간으로 사용 금지 |

유효 헬기 캡처의 카메라는 Z=21,462에서 유지됐고, X=-89,451~-79,115, Y=-60,600~-24,977 구간을 약 3,500 cm/s로 이동했다. 로그에서도 헬기 생성, 플레이어 좌석 배치, 헬기 ViewTarget 적용을 확인했다. 자동으로 삽입 월드맵 UI가 열린 상태였다.

## 3. 1080p 크래시 전 성능

### 3.1 Steady 구간

첫 300프레임을 로딩/PSO 워밍업으로 제외했다.

| 지표 | 결과 |
|---|---:|
| 분석 프레임 | 1,039 |
| 평균 프레임 타임 / FPS | 10.279 ms / 97.3 FPS |
| 중앙값 | 9.864 ms |
| P95 | 13.497 ms |
| P99 / 환산 1% low | 15.719 ms / 63.6 FPS |
| 최대 프레임 타임 | 45.060 ms |
| 16.67 ms 초과 | 7프레임 (0.67%) |
| 33.33 ms 초과 | 1프레임 (0.10%) |
| 평균 Game Thread | 9.922 ms |
| 평균 Render Thread | 7.521 ms |
| 평균 GPU | 8.151 ms |
| 평균 RHI Draw Calls | 212 |
| 평균 Primitives Drawn | 149,888 |
| 평균 GPU Scene Instances | 8,963 |

프레임별 가장 긴 구간은 Game Thread 672프레임, Render Thread 321프레임, GPU 46프레임이었다. 이 샘플에서는 **Game Thread 우세의 CPU 병목**이다. P95/P99도 GT 13.513/15.600 ms, RT 10.823/11.954 ms, GPU 8.844/9.093 ms로 같은 방향이다.

60 FPS 예산(16.67 ms)은 크래시 전 부분 샘플에서 대체로 충족한다. 반면 120 FPS 예산(8.33 ms)은 평균 프레임 타임과 GT/GPU 모두 충족하지 못한다.

### 3.2 초기 300프레임

| 지표 | 결과 |
|---|---:|
| 평균 프레임 타임 | 14.711 ms |
| P95 / P99 | 27.506 / 57.113 ms |
| 16.67 ms 초과 | 31프레임 |
| 33.33 ms 초과 | 10프레임 |
| Graphics PSO misses | 149 |
| Compute PSO misses | 151 |

300프레임 이후에는 두 PSO miss 카운터가 모두 0이었다. 텍스처 Desired Data Loaded도 steady 구간에서 100%였다. 따라서 시작 직후 히치의 핵심 후보는 최초 셰이더/PSO 생성과 World Partition 셀·에셋 활성화다. 로그에는 D3D12 PSO 생성 100 ms 대기와 `Good -> Slow -> Critical` 스트리밍 상태 전환이 기록됐다.

## 4. P0 크래시 분석

두 헬기 실행 모두 `EXCEPTION_ACCESS_VIOLATION reading address 0x68`로 종료했다. 핵심 호출 경로는 다음과 같다.

```text
UEnemyPhysicalComponent::BeginPlay()                    EnemyPhysicalComponent.cpp:47
AEnemyCharacter::BeginPlay()                            EnemyCharacter.cpp:169
UZombieDirectorWorldSubsystem::CreateEnemy()             ZombieDirectorWorldSubsystem.cpp:844
UZombieDirectorWorldSubsystem::WarmPoolBucket()          ZombieDirectorWorldSubsystem.cpp:809
UZombieDirectorWorldSubsystem::WarmPoolForSpawner()      ZombieDirectorWorldSubsystem.cpp:782
UZombieDirectorWorldSubsystem::RegisterSpawner()         ZombieDirectorWorldSubsystem.cpp:210
```

현재 소스에서 `UEnemyPhysicalComponent::BeginPlay()`는 부모 컴포넌트가 복사한 `EnemyAsset`을 검사하지 않고 `GetPhysicalReact()` 및 `PhysicalReact->ReactCurveFloat`를 사용한다. 주소 0x68과 해당 행을 함께 보면 **스폰된 EnemyClass의 `EnemyAsset`이 null인 상태에서 구조체 멤버를 역참조한 것**으로 추정된다.

`AEnemyCharacter::InitializeAsset()`에도 유효성 ensure가 있지만, 실패 시 그 함수만 반환하고 `AEnemyCharacter::BeginPlay()` 및 컴포넌트 초기화를 중단시키지는 않는다. 같은 스폰 과정에서 다음 문제도 관찰됐다.

- `LeaderHead`와 `Torso`의 skeleton이 일치하지 않는 Modular Rig ensure
- `FEnemyPhysicalReact::ReactScale`가 기본 초기화되지 않았다는 시작 시 오류
- AI 풀 생성이 World Partition 스트리밍 중 스포너 등록에서 동기적으로 실행됨

권장 수정 및 합격 기준:

1. 모든 `EnemySpawnProfile -> EnemyClass`가 유효한 `EnemyAsset`을 가지는지 에디터 검증/커맨드릿 검증을 추가한다.
2. `InitializeAsset()` 실패를 bool/명시적 실패 결과로 전파하고, 실패한 적은 나머지 BeginPlay·컴포넌트 초기화·풀 등록을 수행하지 않게 한다.
3. `UEnemyBaseActorComponent`와 `UEnemyPhysicalComponent::BeginPlay()`에서 소유자와 `EnemyAsset`을 방어적으로 검사한다.
4. `FEnemyPhysicalReact::ReactScale`에 의도한 기본값을 선언한다.
5. Modular Female Rig의 leader/follower skeletal asset을 같은 ref skeleton으로 맞춘다.
6. 수정 후 헬기 삽입 + AI 활성 + 1,800프레임 캡처가 오류 코드 0으로 끝나야 한다. 이후 실제 전투/다수 AI 시나리오를 별도로 측정한다.

## 5. 로딩과 World Partition

| 항목 | 시간 |
|---|---:|
| `LoadMap(/Game/Maps/OutBreak_Exterior)` | 18.919 s |
| World Partition `GenerateStreaming` | 4.12 s |
| World Partition initialize | 4.38 s |

이 수치는 Development WindowsEditor의 콜드/부분 캐시 실행이라 패키지 빌드 로딩 시간과 같지 않다. 그래도 World Partition 초기화가 LoadMap의 큰 비중을 차지하며, 헬기가 3,500 cm/s로 움직일 때 스트리밍 상태가 Critical까지 내려간 점은 실제 이동 경로를 기준으로 재검증해야 한다.

권장 사항:

- 헬기 궤도 전체를 커버하는 streaming source의 반경·우선순위·target state를 검토한다.
- 궤도 진행 방향 셀과 삽입 후보 구역을 선행 로드하고, HLOD가 실제 런타임 셀에서 사용되는지 확인한다.
- PSO precache 또는 수집된 PSO 캐시를 시작 구간에 적용한다.
- 패키징된 Test 빌드에서 콜드 부팅, 두 번째 부팅, 헬기 한 바퀴의 로딩/히치를 각각 다시 잰다.

## 6. GPU 진단

1080p CSV의 평균 GPU는 8.151 ms였다. 세부 GPU 이벤트는 별도 888×500 진단 스냅샷이므로 절대 시간 대신 상대 순위만 참고한다.

| 이벤트 | 시간 | Graphics frame 비중 |
|---|---:|---:|
| PostProcessing | 0.539 ms | 12.8% |
| VolumetricCloud | 0.488 ms | 11.6% |
| RenderVirtualShadowMaps (Nanite) | 0.420 ms | 10.0% |
| Nanite VisBuffer | 0.339 ms | 8.1% |
| BasePass | 0.241 ms | 5.7% |
| SingleLayerWater | 0.177 ms | 4.2% |
| ComputeVolumetricFog | 0.150 ms | 3.6% |

스냅샷 전체는 4.198 ms, 476 draws, 562 dispatches, 1,036,337 primitives였다. 비동기 compute 쪽에서는 Lumen Screen Probe Gather가 0.333 ms로 compute queue의 39.3%였다.

Lumen scene에는 Mesh SDF object 14,743개, primitive group 13,228개, merged primitive/instance 0개가 기록됐다. surface cache 물리 페이지 사용률은 낮았으므로 캐시 용량 부족보다는 작은 폴리지/소품이 Mesh SDF와 Lumen scene에 과도하게 참가하는지 먼저 확인하는 편이 좋다.

GPU 최적화 우선 후보:

- Volumetric Cloud/Fog 품질 스케일별 비교 캡처
- VSM의 폴리지 shadow invalidation 및 non-Nanite caster 검토
- 작은 폴리지·원거리 소품의 Lumen Mesh SDF 참여 제외 후보 선정
- Post Process 및 Single Layer Water를 1080p `profilegpu`로 재측정

## 7. 메모리

유효 헬기 CSV steady 구간:

| 항목 | 결과 |
|---|---:|
| GPU local memory | 평균 4,416.7 MB / budget 11,231 MB |
| RenderTargetPoolUsed | 평균 1,942.1 MB, 관측 최대 약 1,970.4 MB |
| Transient memory | 평균 639.4 MB |
| Editor process physical memory | 평균 8,812.6 MB, 전체 캡처 최대 약 9,027 MB |
| Texture streaming | 100% desired data loaded, 평균 약 275 MB |
| Non-streaming mips | 평균 약 216 MB |

VRAM budget 여유와 텍스처 풀 상태는 양호하지만, 약 1.94 GB의 렌더 타깃 풀과 약 640 MB transient 사용은 Lumen/VSM/Water가 포함된 1080p 장면치고도 추적 가치가 있다. 이것이 동시 실사용인지 풀 예약/재사용 영역인지 RenderDoc/Insights 또는 패키지 빌드 RHI 메모리로 구분해야 한다.

`memreport -full`은 Editor process physical 9.35 GB를 기록했다. 상위 메모리 단서에는 다음이 포함된다.

- `RT_WorldMap_Exterior` 32 MB + `T_WorldMap_City` 32 MB
- `T_GameAnimationSample_Logo` 약 18 MB
- 여러 프로토타입 캐릭터 skeletal mesh가 동시에 resident
- Texture2D RHI 약 280 MB, UAV texture 약 757 MB
- Static mesh 전체 약 69 MB로 상대적으로 작음

월드맵 렌더 타깃/소스 텍스처의 중복 필요성을 확인하고, 샘플 로고 및 사용하지 않는 캐릭터가 GameMode/HUD/데이터 자산의 하드 레퍼런스로 로드되는지 Reference Viewer와 Asset Audit로 확인한다.

## 8. 에셋·마이그레이션 상태

진단 로그 행 기준으로 다음이 누적됐다. 동일 원인의 반복 로그가 포함되므로 고유 에셋 수와 같지는 않다.

- 엔진 버전이 비어 있는 에셋 경고 403행
- Blueprint error 508행: 대부분 UE Experimental Landmass 에디터 Blueprint
- `CreateExport: Failed to load Outer` 124행
- OutBreak_Exterior의 `Landscape_WaterBrushManager_0` 및 `LandmassBrushManager` 클래스/외부 액터 참조 불일치
- UE 5.7 Water 플러그인에 존재하지 않는 `/Water/Materials/WaterSurface/LODs/M_WaterRLOD` 의존성
- 일부 Manny pose asset이 원본 애니메이션보다 오래됨

우선 Water/Landmass manager 외부 액터를 현재 UE 5.7 플러그인 구조에 맞게 재생성하거나 제거하고, 관련 레벨·레벨 인스턴스를 resave해야 한다. 그 뒤 전체 에셋 resave/fixup 및 패키징 검증으로 빈 엔진 버전과 누락 import를 정리한다.

## 9. 재측정 체크리스트

- [ ] AI 풀 워밍업 크래시와 Modular Rig ensure 제거
- [ ] 헬기 삽입 1,800프레임 이상 정상 종료
- [ ] AI 0 / 20 / 60 / 목표 최대 개체수별 CSV 비교
- [ ] 전투, Niagara 혈흔, 무기 발사, 네트워크 복제 포함
- [ ] 1080p와 목표 해상도에서 `profilegpu` 재캡처
- [ ] PSO 캐시 적용 전/후 첫 300프레임 비교
- [ ] 패키징 Test 빌드의 콜드/웜 LoadMap 시간 측정
- [ ] World Partition 헬기 궤도 셀의 Slow/Critical 전환 제거
- [ ] Water/Landmass 로드 오류 0건 확인

## 10. 산출물

- 유효 부분 CSV: `Saved/Profiling/CSV/OutBreakExterior_Helicopter_Performance_20260818.csv`
- 유효 부분/크래시 로그: `Saved/Logs/OutBreakExterior_Helicopter_Performance_20260818.log`
- 크래시 재현 로그: `Saved/Logs/OutBreakExterior_Helicopter_RenderOnly_20260818.log`
- 진단 CSV: `Saved/Profiling/CSV/OutBreakExterior_Diagnostics_20260818.csv`
- 진단 로그: `Saved/Logs/OutBreakExterior_Diagnostics_20260818.log`
- 메모리 보고서: `Saved/Profiling/MemReports/OutBreak_Exterior-WindowsEditor-08.18-11.01.58/Pid16532_OutBreak_Exterior-WindowsEditor-18-11.01.58.memreport`
- 재분석 스크립트: `Scripts/analyze_unreal_csv_profile.py`

## 11. 한계

이번 결과는 한 대의 PC, Development WindowsEditor, 오프스크린 Standalone, 단일 플레이어의 헬기 삽입 시작 구간을 측정했다. 실제 적 생성이 크래시로 차단됐고 전투·다수 AI·멀티플레이·패키징 빌드는 포함하지 못했다. 따라서 97.3 FPS는 크래시 전 부분 기준선으로만 사용하며, P0 수정 후의 Test/Shipping 유사 빌드 재측정을 최종 성능 판정으로 삼아야 한다.
