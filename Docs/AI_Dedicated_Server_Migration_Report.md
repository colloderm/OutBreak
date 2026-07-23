# OutBreak AI Dedicated Server 전환 분석 보고서

작성일: 2026-07-18  
분석 기준: `Source/OutBreak/Public/AI` 선언부에서 시작하여 실제 구현, 참조 클래스, 네트워크/피해/GAS/스폰/애니메이션 예산 관련 코드까지 추적했다. 이번 작업에서는 프로젝트 소스 코드를 변경하지 않았고, 생성한 변경 사항은 이 Markdown 보고서뿐이다.

## 1. 요약

현재 `Source/OutBreak/Public/AI`와 대응 구현부의 AI 코드는 Dedicated Server 권위 모델로 완성되어 있지 않다. 서버 전용 빌드 타깃인 `Source/OutBreakServer.Target.cs`는 이미 존재하지만, AI 캐릭터/트래버설/피격/절단/래그돌 상태는 C++ 기준으로 명시적인 복제 상태나 RPC 설계가 없다.

가장 큰 위험은 `UEnemyMovementComponent`의 Traversal이 `PlayAnimMontage`, `MotionWarpingComponent`, `Montage_SetEndDelegate`에 강하게 묶여 있고, `AEnemyCharacter::TakeDamage`가 체력/사망/절단의 영구 상태를 복제하지 않는다는 점이다. Dedicated Server에서는 렌더링이 없고 Skeletal Mesh 애니메이션 업데이트가 제한될 수 있으므로, Montage 종료나 AnimInstance 평가를 서버 게임플레이 상태 종료의 유일한 기준으로 두면 NavLink 완료, 이동 모드 복구, 충돌 복구가 고착될 수 있다.

분석한 C++ 클래스 수는 20개다. Critical 문제는 4개, High 문제는 8개로 분류했다. 현실적 예상 공수는 기능적으로 안정적인 Dedicated Server 전환 기준 약 380시간, 63.3 개발일, 63.3 인일이다. 권장 전환 방안은 방안 B인 서버 권위 상태 머신형이다. 단, Phase 0~1에서는 방안 A의 최소 수정형을 일부 사용해 빠르게 Dedicated Server에서 일단 플레이 가능한 수준을 만든 뒤, Traversal/HitReact/Death/Dismemberment는 상태 복제 기반으로 정리하는 방식을 권장한다.

## 2. 분석 범위

주 분석 범위:

- `Source/OutBreak/Public/AI`
- `Source/OutBreak/Private/AI`

연관 분석으로 확장한 범위와 이유:

| 확장 경로 | 확장 이유 |
| --- | --- |
| `Source/OutBreak/Public/FlowField`, `Source/OutBreak/Private/FlowField` | `AEnemyCharacter`의 `LogModularAnimationProxy`와 Animation Budget Allocator 관련 동작을 확인하기 위해 추적했다. |
| `Source/OutBreak/Public/Character`, `Source/OutBreak/Private/Character` | 플레이어 캐릭터의 GAS, death, ragdoll, RepNotify 구현이 AI 전환 모델의 비교 기준이므로 확인했다. |
| `Source/OutBreak/Public/Ability`, `Source/OutBreak/Private/Ability` | 무기/근접/수류탄 피해가 AI에 도달하는 경로와 GAS 사용 여부를 확인하기 위해 추적했다. |
| `Source/OutBreak/Public/Weapon`, `Source/OutBreak/Private/Weapon` | `UOBGameplayAbility_RangedWeapon`, `UOBGameplayAbility_Melee`, `AOBGrenadeProjectile`이 피해와 이펙트를 처리하는 방식이 AI 피해 구조에 영향을 준다. |
| `Source/OutBreak/Public/Game`, `Source/OutBreak/Private/Game` | Dedicated Server 타깃, Expedition GameMode, SpawnZone, PlayerState 복제 구조와 비교하기 위해 확인했다. |
| `Config` | Dedicated Server 빌드/런타임 설정 흔적을 확인하기 위해 부분적으로 검색했다. |

코드에서 확인하지 못한 범위:

- Blueprint에 설정된 `bReplicates`, `Replicate Movement`, Behavior Tree, Blackboard, AI Perception 컴포넌트 설정은 C++ 텍스트 검색으로 확인할 수 없다.
- `Content`의 `.uasset` 애니메이션 Montage, Root Motion 설정, Motion Warping Notify State 구성은 바이너리 애셋이므로 이 보고서에서는 존재 가능성만 언급하고 동작을 단정하지 않는다.
- `UBaseGeneratedNavLinksProxy`의 실제 구현은 `Source/OutBreak`에서 찾지 못했다. `UEnemyGenNavLinksProxy`가 상속하고 `LinkProxyId`를 사용하므로 외부 모듈 또는 누락된 소스로 추정되며 확인이 필요하다.

## 3. 현재 AI 구조

확인한 AI 관련 C++ 구조는 다음과 같다.

| 클래스/타입 | 파일 | 현재 역할 |
| --- | --- | --- |
| `AEnemyCharacter` | `Source/OutBreak/Public/AI/EnemyCharacter.h`, `Source/OutBreak/Private/AI/EnemyCharacter.cpp` | `ACharacter` 기반 적 캐릭터. `UEnemyMovementComponent`, `UMotionWarpingComponent`, `UChildActorComponent`, `USkeletalMeshComponentBudgeted`를 사용한다. 피격 시 물리 반응과 부위 파괴를 직접 처리한다. |
| `AEnemyController` | `Source/OutBreak/Public/AI/EnemyController.h`, `Source/OutBreak/Private/AI/EnemyController.cpp` | `ADetourCrowdAIController` 기반 컨트롤러. 현재 생성자, `BeginPlay`, `Tick`만 있고 Behavior Tree/Perception 실행 코드는 없다. |
| `UEnemyMovementComponent` | `Source/OutBreak/Public/AI/Components/EnemyMovementComponent.h`, `Source/OutBreak/Private/AI/Components/EnemyMovementComponent.cpp` | `UCharacterMovementComponent` 파생. Generated NavLink 진입 시 Vault/ClimbUp/Drop Traversal을 시작하고 Montage/Motion Warping/Physics를 직접 제어한다. |
| `UEnemyGenNavLinksProxy` | `Source/OutBreak/Public/AI/Nav/EnemyGenNavLinksProxy.h`, `Source/OutBreak/Private/AI/Nav/EnemyGenNavLinksProxy.cpp` | Generated NavLink 진입을 받아 `UEnemyMovementComponent::StartNavLinkTraversal`을 호출한다. |
| `ETraversalType`, `ETraversalLinkType` | `Source/OutBreak/Public/AI/Struct/EnemyTraversalData.h` | Traversal 종류와 NavLink 종류 열거형. |
| `UEnemyBaseActorComponent` | `Source/OutBreak/Public/AI/Components/EnemyBaseActorComponent.h`, `Source/OutBreak/Private/AI/Components/EnemyBaseActorComponent.cpp` | `AEnemyCharacter` 참조 캐싱용 베이스 컴포넌트. 현재 매 틱 가능하지만 실질 로직은 없다. |
| `UEnemyStatusComponent` | `Source/OutBreak/Public/AI/Components/EnemyStatusComponent.h`, `Source/OutBreak/Private/AI/Components/EnemyStatusComponent.cpp` | 상태 컴포넌트로 보이나 현재 구현은 비어 있다. |
| `UEnemyLimbComponent` | `Source/OutBreak/Public/AI/Components/EnemyLimbComponent.h`, `Source/OutBreak/Private/AI/Components/EnemyLimbComponent.cpp` | Limb 컴포넌트로 보이나 현재 구현은 비어 있다. |
| `AModularSkeletalMeshActor` | `Source/OutBreak/Public/AI/System/ModularSkeletalMeshActor.h`, `Source/OutBreak/Private/AI/System/ModularSkeletalMeshActor.cpp` | `UChildActorComponent`로 붙는 보조 Skeletal Mesh Actor. `LeaderHead` Skeletal Mesh를 제공한다. |
| `UAnimationBudgetWorldSubsystem` | `Source/OutBreak/Public/FlowField/AnimationBudgetWorldSubsystem.h`, `Source/OutBreak/Private/FlowField/AnimationBudgetWorldSubsystem.cpp` | 게임 월드에서 Animation Budget Allocator를 활성화하되 `NM_DedicatedServer`에서는 즉시 반환한다. |

현재 C++에서 확인된 AI 의사결정 구조:

- `AEnemyController`에는 `RunBehaviorTree`, `UseBlackboard`, `UAIPerceptionComponent`, 타깃 선택, Aggro, Forget 시간 관련 코드가 없다.
- `UEnemyGenNavLinksProxy::OnLinkMoveStarted`는 `UPathFollowingComponent`에서 Pawn을 얻어 `UEnemyMovementComponent`로 Traversal 시작을 위임한다.
- `UEnemyMovementComponent::RequestDirectMove`와 `RequestPathMove`는 Traversal 중 PathFollowing 입력을 무시한다.

## 4. 현재 네트워크 구조

프로젝트 전체에는 네트워크 코드가 존재한다. 대표적으로 `AOBCharacterBase`, `AOBPlayerStateBase`, `UOBInventoryComponent`, `UOBEquipmentComponent`, `AOBWeaponBase`, `AOBGrenadeProjectile`, `UOBAttributeSetBase`는 `DOREPLIFETIME`, `ReplicatedUsing`, `HasAuthority`, `NetMulticast`를 사용한다.

AI C++ 범위에서는 다음이 확인된다.

| 항목 | 확인 결과 |
| --- | --- |
| `bReplicates` | `AEnemyCharacter`, `AEnemyController`, `AModularSkeletalMeshActor` C++ 생성자에서 명시 설정 없음. `ACharacter` 엔진 기본값 또는 Blueprint 설정에 의존하는지 확인 필요. |
| `SetReplicateMovement` | AI C++에서 명시 호출 없음. `UCharacterMovementComponent` 기본 네트워크 동기화 사용 여부는 Actor 복제 설정에 좌우된다. |
| `GetLifetimeReplicatedProps` | AI C++ 클래스에 없음. `TraversalType`, `bIsTraversingNavLink`, 피격/절단/사망 상태는 C++ 기준 복제되지 않는다. |
| `ReplicatedUsing`, `OnRep` | AI C++ 클래스에 없음. |
| `Server RPC`, `Client RPC`, `NetMulticast RPC` | AI C++ 클래스에 없음. |
| `HasAuthority`, `GetLocalRole`, `IsNetMode` | AI C++ 범위에서는 `UAnimationBudgetWorldSubsystem::OnWorldBeginPlay`의 `NM_DedicatedServer` 검사만 확인된다. |
| Dedicated Server Target | `Source/OutBreakServer.Target.cs`가 있고 `Type = TargetType.Server`로 설정되어 있다. |
| 모듈 의존성 | `Source/OutBreak/OutBreak.Build.cs`에 `AIModule`, `NavigationSystem`, `MotionWarping`, `PhysicsCore`, `AnimationBudgetAllocator`, `GameplayAbilities` 등이 포함되어 있다. |

비교 기준으로, 플레이어 측 `AOBCharacterBase`는 `bIsDead`, `bIsAiming`을 `ReplicatedUsing`으로 복제하고 `HandleDeath`에서 `HasAuthority()`를 검사한다. `UOBAttributeSetBase`는 `Health`, `MaxHealth`를 `DOREPLIFETIME_CONDITION_NOTIFY`로 복제한다. 이 패턴은 AI 전환에도 재사용할 수 있지만, 현재 `AEnemyCharacter`는 `IAbilitySystemInterface`를 구현하지 않고 ASC/AttributeSet을 갖지 않는다.

## 5. Dedicated Server 호환성 문제

| ID | 위험도 | 근거 | 문제 | 권장 권한/동기화 | 예상 공수 |
| --- | --- | --- | --- | --- | --- |
| C1 | Critical | `UEnemyMovementComponent::BeginTraversalVault`, `BegineTraversalClimbUp`, `OnMontageEnded` | Traversal 시작/종료가 Montage 재생과 종료 delegate에 의존한다. Dedicated Server에서 AnimInstance/Montage 평가가 제한되면 `EndParkour`, `FinishNavLinkTraversal`이 호출되지 않을 수 있다. | Traversal 판정/상태 전이는 Server Authority. `FEnemyTraversalNetState`를 ReplicatedUsing으로 복제하고 클라이언트는 Presentation만 수행. 서버 종료는 Montage delegate가 아니라 거리/시간/MovementMode 기반으로 분리. | 42~72시간 |
| C2 | Critical | `AEnemyCharacter::TakeDamage`, `UOBGameplayAbility_RangedWeapon::PerformServerWeaponTrace`, `UOBGameplayAbility_Melee::PerformMeleeTrace`, `AOBGrenadeProjectile::Explode` | AI 체력/사망 상태가 C++ 기준 존재하지 않는다. Ranged는 `ApplyPointDamage`로 AI 반응을 일으키지만 Health 감소가 없고, Melee/Grenade는 Target ASC가 없으면 AI에 피해가 적용되지 않는다. | 서버 전용 Damage 판정. `AEnemyCharacter`에 Health/Death 상태를 추가하거나 AI용 ASC/AttributeSet을 도입. Health/Death는 Replicated State, 피격 이펙트는 Gameplay Cue 또는 Unreliable Multicast. | 24~48시간 |
| C3 | Critical | `UEnemyMovementComponent::TickTraversalDrop`, `AEnemyCharacter::TakeDamage`, `HandleReactTimelineFinished` | Drop/Ragdoll/부분 물리 반응이 권한 검사 없이 Skeletal Mesh Physics와 Collision을 변경한다. 서버/클라이언트가 서로 다른 물리 결과와 충돌 상태를 가질 수 있다. | 서버는 게임플레이 위치/사망/Traversal 완료만 확정. 클라이언트는 복제된 Hit/Ragdoll 이벤트로 Cosmetic Physics. 장기 상태는 ReplicatedUsing. | 32~56시간 |
| C4 | Critical | `AEnemyCharacter::MeshPartDestruction`, `PhysicalMaterialProcess` | 절단 파츠 `AStaticMeshActor`를 Spawn하지만 복제 설정이 없고, Bone 숨김 상태도 복제되지 않는다. 늦게 들어온 클라이언트나 relevancy 재진입 클라이언트가 절단 상태를 복원할 수 없다. | 절단 판정은 서버 권위. 절단 부위 비트마스크, BoneName, 발생 Transform, Impulse를 Replicated State/Event로 분리. 파츠 Physics는 기본적으로 Client Cosmetic. | 40~72시간 |
| H1 | High | AI C++ 전체 검색 결과 | AI Actor/Component에 `GetLifetimeReplicatedProps`, `ReplicatedUsing`, RPC가 없다. Dedicated Server에서 필요한 상태를 네트워크로 복원할 근거가 없다. | `AEnemyCharacter` 또는 `UEnemyStatusComponent`를 상태 소유자로 정하고 RepNotify 추가. | 16~32시간 |
| H2 | High | `UEnemyMovementComponent::BeginTraversalVault`, `BegineTraversalClimbUp` | Warp Target 1/2/3이 현재 인스턴스에서 계산되며 복제되지 않는다. 클라이언트별 NavLink/위치 차이, 지연, 중도 참가 시 다른 결과가 가능하다. | 서버 계산 후 `FTransform` 배열 또는 시작/종료 Transform을 복제. `ServerWorldStartTime` 포함. | 16~32시간 |
| H3 | High | `AEnemyController` | Behavior Tree, Blackboard, Perception, Target, Aggro 코드가 C++에 없다. Blueprint에만 있으면 Dedicated Server 실행 여부와 권한 경계가 불명확하다. | AI 의사결정은 서버 전용. 클라이언트에 필요한 최소 표현 상태만 복제. | 12~24시간 |
| H4 | High | `AEnemyCharacter::ApplyAnimationBudgetSignificance`, `UAnimationBudgetWorldSubsystem::OnWorldBeginPlay` | Dedicated Server에서 Animation Budget Allocator는 비활성화되지만, AI gameplay가 Mesh/Montage/AnimInstance 이벤트에 의존한다. | 서버 gameplay 상태는 애니메이션 예산/렌더링 여부와 분리. Mesh Tick 옵션은 검증 항목으로만 유지. | 12~24시간 |
| H5 | High | `UEnemyMovementComponent::BeginParkour`, `EndParkour`, `TickTraversalDrop` | Capsule collision, Mesh collision, MovementMode 전환이 복제 상태 없이 직접 변경된다. | MovementMode는 CMC로 처리 가능한지 검증하고, Capsule/Mesh collision gameplay 상태는 RepNotify로 복제. | 12~24시간 |
| H6 | High | `AEnemyCharacter`, `AEnemyController`, `UEnemyBaseActorComponent`, `UEnemyStatusComponent`, `UEnemyLimbComponent`, `AModularSkeletalMeshActor` | 여러 Actor/Component가 기본 Tick 활성화 상태다. 100 AI 환경에서 서버 Tick 비용과 네트워크 보정 비용이 증가한다. | 실제 로직이 없는 Tick 비활성화, 상태 변화 기반 이벤트 처리, Significance/LOD 정책 분리. | 8~20시간 |
| H7 | High | `AOBExpeditionGameMode`, AI C++ 검색 결과 | 플레이어 스폰 구조는 있으나 AI Spawn/Death/Despawn/Object Pool C++ 구조가 확인되지 않는다. Dedicated Server에서 AI 수명주기와 relevancy 복구 기준이 없다. | GameMode/Subsystem 서버 권위 SpawnManager 또는 Pool 도입. Spawned/Dead/Despawned 상태 복제. | 16~40시간 |
| H8 | High | `AOBCharacterBase::Multicast_PlayFireMontage`, `AOBGrenadeProjectile::Multicast_OnExploded` | 플레이어 무기 연출은 Multicast 중심 예시가 있다. AI 영구 상태까지 Multicast로 처리하면 중도 참가/relevancy 복구가 실패한다. | 일회성 VFX/SFX는 Multicast 또는 Gameplay Cue, 영구 상태는 ReplicatedUsing. | 8~16시간 |
| M1 | Medium | `UEnemyMovementComponent::TickTraversalDrop` | 타이머 람다가 `[&]`로 캡처한다. 컴포넌트 파괴/재진입/다중 AI 상황에서 안전성 검증이 필요하다. | `FTimerDelegate`에 약한 객체 캡처 또는 멤버 함수 바인딩, `EndPlay`에서 타이머 정리. | 4~8시간 |
| M2 | Medium | `AEnemyCharacter::TakeDamage` | `if (!bIsHit) { "It's already a hit." }` 로그 조건이 의미와 반대로 보인다. 중복 피격 방지 의도가 구현되지 않았다. | 서버 권위 HitReact 상태와 쿨다운으로 중복 반응 제어. | 4~8시간 |
| M3 | Medium | `AModularSkeletalMeshActor` | 보조 Mesh Actor의 복제/Owner/Relevancy 정책이 없다. `LeaderHead`에 Bone 숨김이 적용되므로 네트워크 표시 차이가 생길 수 있다. | Actor 자체 복제보다 `AEnemyCharacter`의 절단 상태로 클라이언트 표현을 재구성. | 8~16시간 |
| M4 | Medium | `Source/OutBreakServer.Target.cs`, `Config/DefaultGame.ini` | Server Target은 있으나 기본 BuildTarget은 `OutBreak`로 보인다. 패키징/실행 프로파일은 별도 검증 필요. | Dedicated Server 빌드, cook, launch script, map travel, OnlineSubsystem 설정 검증. | 10~18시간 |

## 6. 클래스별 분석

### `AEnemyCharacter`

- 파일: `Source/OutBreak/Public/AI/EnemyCharacter.h`, `Source/OutBreak/Private/AI/EnemyCharacter.cpp`
- 주요 멤버: `MotionWarpingComponent`, `ChildActorComponent`, `ChildActorSkeletalMesh`, `ReactCurveFloat`, `ReactScale`, `SM_Arm_R`, `SM_Arm_L`, `SM_Leg_R`, `SM_Leg_L`, `PM_Head`, `PM_Torso`, `PM_Arm_R`, `PM_Arm_L`, `PM_Leg_R`, `PM_Leg_L`, `ReactTimeline`, `CacheBoneName`, `bIsHit`
- 현재 동작:
  - 생성자에서 `UEnemyMovementComponent`를 기본 CharacterMovement로 지정한다.
  - `USkeletalMeshComponentBudgeted`를 Mesh로 사용하고 `UMotionWarpingComponent`와 `UChildActorComponent`를 생성한다.
  - `BeginPlay`에서 Animation Budget 설정, ReduceWork delegate, `ReactTimeline`을 구성한다.
  - `Tick`에서 `ReactTimeline.TickTimeline`을 매 프레임 호출한다.
  - `TakeDamage`에서 `FPointDamageEvent`의 `HitInfo`를 읽고 Bone/PhysicalMaterial에 따라 물리 반응과 부위 파괴를 처리한다.
  - `MeshPartDestruction`에서 절단 파츠용 `AStaticMeshActor`를 Spawn하고 `ChildActorSkeletalMesh->HideBoneByName`을 호출한다.
- Dedicated Server 문제:
  - `TakeDamage`, `PhysicalMaterialProcess`, `MeshPartDestruction`, `HandleReactTimeline`, `HandleReactTimelineFinished`에 권한 검사가 없다.
  - AI Health, Death, Dead state, Despawn state가 없다. `Super::TakeDamage` 결과를 사용하지만 실제 체력 감소/사망 전이는 C++에서 확인되지 않는다.
  - Spawn한 `AStaticMeshActor`에 `bReplicates`, `SetReplicateMovement` 설정이 없고, 파츠 Actor 수가 증가하면 서버 물리/대역폭 비용이 커진다.
  - `ReactTimeline`은 렌더링 없는 서버에서도 Tick된다. 서버 gameplay 상태 복구가 Timeline 끝에 의존하면 위험하다.
- 권장 실행 권한:
  - 피해 판정, 체력 변경, 사망 판정, 절단 판정은 Server Authority.
  - HitReact, 파츠 이펙트, 피격 물리 연출은 Server Simulation + Client Presentation 또는 Client Cosmetic Only.
  - Health, Death, DismemberedMask, active hit reaction id는 Replicated State.
- 권장 동기화:
  - `CurrentHealth` 또는 AI ASC Attribute는 `ReplicatedUsing`.
  - `bIsDead`, `DeathTransform`, `DeathServerTime`은 `ReplicatedUsing`.
  - `DismemberedPartsMask`, `LastDismemberEvent`는 영구 상태와 일회성 이벤트를 분리.
  - `FEnemyHitReactionEvent { BoneName, HitLocation, HitNormal, Impulse, EventId }`는 Unreliable Multicast 또는 RepNotify 이벤트 카운터.
- 수정 난이도: 높음
- 예상 공수: 48~96시간

### `UEnemyMovementComponent`

- 파일: `Source/OutBreak/Public/AI/Components/EnemyMovementComponent.h`, `Source/OutBreak/Private/AI/Components/EnemyMovementComponent.cpp`
- 주요 멤버: `TraversalType`, `VaultSpan`, `VaultMinHeight`, `VaultMaxHeight`, `VaultMontage`, `MantleMontage`, `ClimbUpMontage`, `TraversalDestination`, `ActivePathFollowing`, `ActiveCustomLink`, `bIsTraversingNavLink`, `CacheMeshWorldLocation`, `bFallingStart`, `AfterDropToReturnHandle`, `Character`, `CapsuleComponent`, `SkeletalMeshComponent`
- 현재 동작:
  - `StartNavLinkTraversal`이 LinkType에 따라 `BeginTraversalVault`, `BegineTraversalClimbUp`, Drop 분기를 실행한다.
  - `BeginTraversalVault`와 `BegineTraversalClimbUp`은 Motion Warping Target 1/2/3을 계산하고 Montage를 재생한다.
  - Montage 종료 delegate가 `OnMontageEnded`를 호출하고, `OnMontageEnded`가 `EndParkour`와 `FinishNavLinkTraversal`을 호출한다.
  - Drop은 `TickTraversalDrop`에서 Mesh Physics, Capsule Collision, Timer를 직접 제어한다.
- Dedicated Server 문제:
  - Traversal 상태가 복제되지 않는다. `TraversalDestination`, `TraversalType`, `bIsTraversingNavLink`는 `UPROPERTY()`일 뿐 replication 설정이 없다.
  - Montage 종료 delegate가 서버 상태 종료 기준이다. Dedicated Server에서 AnimInstance가 없거나 Montage가 재생되지 않으면 `FinishUsingCustomLink`가 호출되지 않을 수 있다.
  - Warp Target 계산 결과와 `ServerStartTime`이 복제되지 않아 중도 참가/relevancy 재진입 시 현재 Traversal을 복원할 수 없다.
  - `BeginParkour`와 `EndParkour`가 Collision/MovementMode를 직접 바꾸지만 클라이언트 동기화 정책이 없다.
- 권장 실행 권한:
  - NavLink 선택, Traversal 가능 여부, Start/End는 Server Authority.
  - 클라이언트 Montage/MotionWarping은 복제 상태 기반 Presentation.
- 권장 동기화:
  - `FEnemyTraversalNetState`를 `AEnemyCharacter` 또는 `UEnemyMovementComponent`에 ReplicatedUsing으로 둔다.
  - 포함 데이터: `TraversalId`, `TraversalType`, `LinkType`, `StartTransform`, `EndTransform`, `WarpTargets`, `ServerStartTime`, `ExpectedDuration`, `bActive`.
  - 서버 종료는 `ExpectedDuration`, 거리, MovementMode, PathFollowing 상태로 판단하고 Montage end와 분리한다.
- 수정 난이도: 높음
- 예상 공수: 42~72시간

### `UEnemyGenNavLinksProxy`

- 파일: `Source/OutBreak/Public/AI/Nav/EnemyGenNavLinksProxy.h`, `Source/OutBreak/Private/AI/Nav/EnemyGenNavLinksProxy.cpp`
- 주요 멤버: `LinkTraversalType`
- 주요 함수: `OnLinkMoveStarted`, `GetActiveLinkEndpoints`
- 현재 동작:
  - `UPathFollowingComponent`에서 `AAIController`와 Pawn을 얻고, Pawn의 `UEnemyMovementComponent`에 Traversal 시작을 전달한다.
  - `GetActiveLinkEndpoints`가 현재 Path point와 `DestPoint`를 이용해 시작/종료 지점을 계산한다.
- Dedicated Server 문제:
  - `OnLinkMoveStarted`는 일반적으로 서버 AI PathFollowing에서 호출될 가능성이 높지만 코드상 권한 확인이 없다.
  - Link endpoint 결과가 Traversal 복제 상태로 저장되지 않는다.
  - `UBaseGeneratedNavLinksProxy` 구현이 현재 소스 검색 범위에서 확인되지 않아 LinkProxyId 수명/권한 정책이 불명확하다.
- 권장 실행 권한:
  - Server Authority.
- 권장 동기화:
  - 이 클래스는 직접 복제하지 않고, `UEnemyMovementComponent` 또는 `AEnemyCharacter`가 Traversal 상태를 복제한다.
- 수정 난이도: 중간
- 예상 공수: 8~16시간

### `AEnemyController`

- 파일: `Source/OutBreak/Public/AI/EnemyController.h`, `Source/OutBreak/Private/AI/EnemyController.cpp`
- 현재 동작:
  - `ADetourCrowdAIController` 파생이며 Tick만 활성화되어 있다.
  - Behavior Tree, Blackboard, AI Perception, Target, Aggro 코드는 확인되지 않는다.
- Dedicated Server 문제:
  - C++ 기준 AI 의사결정이 어디서 실행되는지 확인할 수 없다.
  - Blueprint에서 실행 중이라면 Dedicated Server에서도 동일하게 실행되는지, 클라이언트에서 불필요하게 실행되지 않는지 검증해야 한다.
  - Tick이 비어 있는데 활성화되어 있어 다수 AI 환경에서 불필요한 비용이 발생한다.
- 권장 실행 권한:
  - AIController, Behavior Tree, Blackboard, Perception, Target/Aggro는 Server Authority.
- 권장 동기화:
  - 내부 Target Actor는 기본적으로 복제하지 않는다. 공격 상태, 시선 방향, 목표 위치, 경고/전투 상태 등 클라이언트 표현에 필요한 최소값만 복제한다.
- 수정 난이도: 중간
- 예상 공수: 12~24시간

### `UEnemyBaseActorComponent`, `UEnemyStatusComponent`, `UEnemyLimbComponent`

- 파일:
  - `Source/OutBreak/Public/AI/Components/EnemyBaseActorComponent.h`
  - `Source/OutBreak/Private/AI/Components/EnemyBaseActorComponent.cpp`
  - `Source/OutBreak/Public/AI/Components/EnemyStatusComponent.h`
  - `Source/OutBreak/Private/AI/Components/EnemyStatusComponent.cpp`
  - `Source/OutBreak/Public/AI/Components/EnemyLimbComponent.h`
  - `Source/OutBreak/Private/AI/Components/EnemyLimbComponent.cpp`
- 현재 동작:
  - 세 컴포넌트 모두 Tick 가능 상태이며, `UEnemyBaseActorComponent`는 생성자에서 `EnemyCharacter = Cast<AEnemyCharacter>(GetOwner())`를 수행한다.
  - Status/Limb 컴포넌트는 현재 실질 로직이 없다.
- Dedicated Server 문제:
  - 생성자에서 `GetOwner()` 기반 캐싱은 컴포넌트 소유자 초기화 타이밍에 따라 안전하지 않을 수 있다. `BeginPlay` 또는 `OnRegister`에서 재검증하는 편이 낫다.
  - 상태/절단을 담당하기 좋은 컴포넌트 이름이 이미 있으나 replication 설정이 없다.
  - 빈 Tick은 AI 수가 늘 때 비용이 된다.
- 권장 실행 권한:
  - `UEnemyStatusComponent`: Health/Death/ActionState 소유, Server Authority 전이, Replicated State 복제.
  - `UEnemyLimbComponent`: Dismemberment 영구 상태 소유, Server Authority 판정, Replicated State 복제.
- 권장 동기화:
  - 컴포넌트 자체 복제는 필요할 때만 `SetIsReplicatedByDefault(true)` 사용. 단순 상태는 Owner Actor의 replicated property로 둘 수도 있다.
- 수정 난이도: 중간
- 예상 공수: 16~32시간

### `AModularSkeletalMeshActor`

- 파일: `Source/OutBreak/Public/AI/System/ModularSkeletalMeshActor.h`, `Source/OutBreak/Private/AI/System/ModularSkeletalMeshActor.cpp`
- 주요 멤버: `Root`, `LeaderHead`
- 현재 동작:
  - `AEnemyCharacter`의 `ChildActorComponent`가 생성하는 Actor로 보이며, `LeaderHead` Skeletal Mesh를 제공한다.
  - `AEnemyCharacter::GetChildActorSkeletalMesh`가 이 Actor를 `AModularSkeletalMeshActor`로 캐스팅해 `LeaderHead`를 반환한다.
- Dedicated Server 문제:
  - Actor 복제 설정이 없다.
  - `MeshPartDestruction`은 원본 `GetMesh()`가 아니라 `ChildActorSkeletalMesh`의 Bone을 숨긴다. 클라이언트에서 이 child actor가 동일하게 존재하고 같은 Bone 상태를 갖는지 보장되지 않는다.
  - Tick이 활성화되어 있으나 현재 로직은 없다.
- 권장 실행 권한:
  - Mesh Actor 자체는 Client Presentation 성격이 강하다.
- 권장 동기화:
  - `AModularSkeletalMeshActor`를 직접 복제하기보다 `AEnemyCharacter`의 `DismemberedPartsMask`를 복제하고 각 클라이언트가 `LeaderHead`의 Bone 표시를 재구성한다.
- 수정 난이도: 중간
- 예상 공수: 8~16시간

### `UAnimationBudgetWorldSubsystem`

- 파일: `Source/OutBreak/Public/FlowField/AnimationBudgetWorldSubsystem.h`, `Source/OutBreak/Private/FlowField/AnimationBudgetWorldSubsystem.cpp`
- 현재 동작:
  - `OnWorldBeginPlay`에서 `InWorld.GetNetMode() == NM_DedicatedServer`면 반환한다.
  - Dedicated Server가 아니면 `IAnimationBudgetAllocator::Get(&InWorld)` 후 allocator를 활성화한다.
- Dedicated Server 문제:
  - 이 코드는 렌더링 없는 서버에서 animation budget allocator를 켜지 않는 올바른 방향이다.
  - 그러나 `AEnemyCharacter`와 `UEnemyMovementComponent`가 gameplay 상태를 Montage/AnimInstance에 의존하면 allocator 비활성 여부와 별개로 Dedicated Server에서 문제가 생긴다.
- 권장 실행 권한:
  - Animation Budget은 Client Cosmetic/Presentation 최적화.
- 권장 동기화:
  - 없음. 단, 서버 gameplay 상태는 animation budget 결과에 의존하지 않아야 한다.
- 수정 난이도: 낮음
- 예상 공수: 2~4시간 검증

### 피해/GAS 연관 클래스

| 클래스 | 파일 | 확인 내용 | AI 전환 영향 |
| --- | --- | --- | --- |
| `UOBGameplayAbility_RangedWeapon` | `Source/OutBreak/Private/Ability/Abilities/OBGameplayAbility_RangedWeapon.cpp` | `FireOneShot`은 서버에서 `PerformServerWeaponTrace`를 호출한다. `PerformServerWeaponTrace`는 `UGameplayStatics::ApplyPointDamage`를 먼저 호출하고, Target ASC가 있으면 GameplayEffect를 적용한다. | AI는 `TakeDamage`를 통해 반응할 수 있지만 Health/GAS가 없으면 체력/사망이 진행되지 않는다. |
| `UOBGameplayAbility_Melee` | `Source/OutBreak/Private/Ability/Abilities/OBGameplay/OBGameplayAbility_Melee.cpp` | `OnHitWindow`에서 서버만 `PerformMeleeTrace`를 호출한다. 피해 적용은 Target ASC가 있을 때만 이루어진다. | `AEnemyCharacter`에 ASC가 없으면 C++ AI는 근접 피해를 받지 않는다. |
| `AOBGrenadeProjectile` | `Source/OutBreak/Private/Weapon/Projectile/OBGrenadeProjectile.cpp` | Actor는 `bReplicates = true`, `SetReplicateMovement(true)`. 폭발은 서버에서 실행되고 `Multicast_OnExploded`로 연출한다. 피해는 Target ASC가 있을 때만 적용된다. | AI ASC가 없으면 수류탄 피해를 받지 않는다. 폭발 연출은 일회성이라 중도 참가 복원 대상은 아니다. |
| `UOBAttributeSetBase` | `Source/OutBreak/Private/Ability/Attributes/OBAttributeSetBase.cpp` | `Health`, `MaxHealth`를 RepNotify로 복제하고 `PostGameplayEffectExecute`에서 `HandleDeath`를 호출한다. | AI에도 GAS를 붙일 경우 재사용 가능한 모델이다. 다만 PlayerState 소유 ASC 구조를 AI Character 소유 ASC로 바꿔야 한다. |
| `AOBCharacterBase` | `Source/OutBreak/Private/Character/OBCharacterBase.cpp` | `bIsDead`, `bIsAiming` 복제, `HandleDeath` 서버 권위, `OnRep_IsDead` 클라이언트 ragdoll 구조가 있다. | AI death/ragdoll 복제 모델의 좋은 참고 예시다. |

## 7. 로직별 실행 권한 분류

| 기능 | 분류 | 현재 코드 근거 | 권장 처리 |
| --- | --- | --- | --- |
| AI 의사결정 | Server Authority | C++에서 의사결정 코드 미확인 | `AEnemyController`/BT는 서버 전용 실행. 클라에는 표현 상태만 복제. |
| Behavior Tree 실행 | Server Authority | `RunBehaviorTree` 검색 결과 없음 | 서버 AIController에서만 실행. |
| Blackboard 상태 | Server Authority | Blackboard 코드 미확인 | 내부 상태는 서버 전용. 필요한 일부 상태만 Replicated State. |
| AI Perception | Server Authority | Perception 코드 미확인 | 서버 전용 감지. 클라에는 타깃 자체보다 공격 방향/상태 중심 복제. |
| 타깃 선택 | Server Authority | Target/Aggro 코드 미확인 | 서버 선택. Target Actor 전체 복제는 필요할 때만 Owner/Team 조건 검토. |
| Aggro 획득 및 해제 | Server Authority | 코드 미확인 | 서버 타이머/상태. UI/연출용 상태만 복제. |
| MoveTo 요청 | Server Authority | `UEnemyGenNavLinksProxy`가 `UPathFollowingComponent`에서 Pawn을 얻음 | 서버 AIController가 요청. CMC 이동 복제 사용. |
| NavMesh 경로 탐색 | Server Authority | `UNavigationSystemV1` 사용 | 서버 경로 탐색. 클라 경로는 필요 없음. |
| Generated NavLink 선택 | Server Authority | `UEnemyGenNavLinksProxy::OnLinkMoveStarted` | 서버 선택 결과를 Traversal state로 복제. |
| Traversal 시작 및 종료 | Server Simulation + Client Presentation | `StartNavLinkTraversal`, `FinishNavLinkTraversal` | 서버 상태 전이, 클라 OnRep로 Montage/MotionWarping. |
| Vault | Server Simulation + Client Presentation | `BeginTraversalVault` | 서버 WarpTarget 계산/복제, 클라 Montage 표현. |
| Mantle | Server Simulation + Client Presentation | Mantle Montage 멤버만 있고 구현 분기 없음 | 구현 시 Vault와 같은 상태 복제 모델. |
| Climb | Server Simulation + Client Presentation | `BegineTraversalClimbUp` | 서버 WarpTarget 계산/복제, 클라 Montage 표현. |
| Drop | Server Simulation + Client Presentation | `TickTraversalDrop` | 서버는 완료 위치/상태 확정, 클라 ragdoll/낙하 연출. |
| Motion Warping Target 계산 | Replicated State | WarpTarget 1/2/3 로컬 계산 | 서버 계산 후 Transform 복제. |
| Root Motion | Server Simulation + Client Presentation | Montage 기반 이동으로 추정, 애셋 설정 미확인 | 서버 CMC 권위 이동 검증. 클라는 상태 기반 재생. |
| Animation Montage 재생 | Client Cosmetic Only | `PlayAnimMontage`, `Multicast_PlayFireMontage` | gameplay 상태 기준이 아니라 표현. 시작 시각 보정 필요. |
| Montage Section | Replicated State | Section 사용 코드 미확인 | Section이 판정/이동에 영향 있으면 서버 상태로 복제. |
| Montage 종료 처리 | Server Authority | `OnMontageEnded`가 `EndParkour` 호출 | 서버 종료는 타이머/거리/상태 조건. Montage 종료는 클라 표현 종료. |
| CharacterMovement 상태 | Replicated State | `SetMovementMode`, CMC 파생 | 기본 CMC로 가능한 범위 검증. Traversal custom 상태는 별도 복제. |
| 위치와 회전 | Replicated State | `ACharacter`/CMC 기반 | 기본 Movement replication 우선. 순간이동 보정 최소화. |
| 충돌 상태 변경 | Replicated State | `SetCollisionEnabled`, `SetCollisionResponseToChannel` | gameplay 충돌은 서버 권위 + OnRep. |
| Capsule 활성화 및 비활성화 | Replicated State | `BeginParkour`, `EndParkour`, death 참고 | 서버 상태 기반으로 클라 복원. |
| Ragdoll 시작 및 종료 | Server Simulation + Client Presentation | `StartRagdoll`, `TickTraversalDrop` | 서버는 사망/완료 위치 확정, 클라 Cosmetic ragdoll. |
| 부분 Physics Reaction | Client Cosmetic Only | `SetAllBodiesBelowSimulatePhysics`, `AddImpulse` | 서버 hit event만 전송, 클라 로컬 물리. |
| 피격 판정 | Server Authority | `PerformServerWeaponTrace`, `OnHitWindow` | 서버 Trace/Overlap/HitResult 판정. |
| 데미지 계산 | Server Authority | GAS abilities는 서버에서 적용 | AI도 서버 전용 계산. |
| 체력 변경 | Replicated State | AI 없음, Player `UOBAttributeSetBase` 있음 | AI Health/Attribute RepNotify. |
| 사망 판정 | Server Authority | AI 없음, Player `HandleDeath` 있음 | 서버만 판정, `bIsDead` 복제. |
| 부위 절단 판정 | Server Authority | `PhysicalMaterialProcess` | 서버만 판정. |
| Bone 숨김 | Replicated State | `HideBoneByName` 직접 호출 | `DismemberedPartsMask` OnRep에서 재적용. |
| 절단 파츠 생성 | Server Simulation + Client Presentation | `SpawnActor<AStaticMeshActor>` | 서버는 상태/event, 클라는 cosmetic actor 생성. 필요 시 제한적으로 replicated actor. |
| 절단 파츠 Physics | Client Cosmetic Only | `SetSimulatePhysics(true)` | 서버 대량 physics 지양. Impulse event로 클라 재현. |
| Niagara 및 Particle | Client Cosmetic Only | Gameplay Cue/Multicast 사용 가능 | Gameplay Cue 또는 Unreliable Multicast. |
| Sound | Client Cosmetic Only | `Multicast_OnExploded`, Gameplay Cue | 일회성 이벤트. |
| Decal | Client Cosmetic Only | 직접 코드 미확인 | Hit event로 클라 생성. |
| Spawn | Server Authority | AI Spawn 코드 미확인, Player spawn은 GameMode | 서버 SpawnManager/Pool. Actor replication 또는 spawn state. |
| Despawn | Server Authority | AI Despawn 코드 미확인 | 서버 결정, `bPendingDespawn`/Destroy replication. |
| Object Pool | Server Authority | 코드 미확인 | 서버 pool state, 클라 visibility/activation 복제. |
| Timer | Server Authority | `AfterDropToReturnHandle`, session timer | gameplay timer는 서버. cosmetic timer는 클라. |
| Delegate | Server Authority | `Montage_SetEndDelegate`, ReduceWork delegate | gameplay delegate는 서버 상태와 분리. |
| Gameplay Event | Server Authority | GAS/Gameplay Cue 사용 | 판정 이벤트는 서버, 연출 이벤트는 Cue. |
| Gameplay Ability System Effect | Server Authority | `UOBAbilitySet`, weapon abilities | 서버 ASC에서 적용. Attribute는 Replicated State. |

## 8. 동기화가 필요한 상태와 이벤트

영구 복제 상태:

| 상태 | 소유 주체 | 복제 방식 | 이유 |
| --- | --- | --- | --- |
| `EnemyHealth` 또는 AI AttributeSet `Health` | `AEnemyCharacter` 또는 AI ASC | `ReplicatedUsing` 또는 GAS Attribute replication | 중도 참가/HP UI/사망 판정 복구. |
| `bIsDead` | `AEnemyCharacter`/`UEnemyStatusComponent` | `ReplicatedUsing = OnRep_IsDead` | 사망/충돌/시체 상태 복원. |
| `EnemyActionState` | `AEnemyCharacter`/`UEnemyStatusComponent` | `ReplicatedUsing = OnRep_ActionState` | Traversal, HitReact, Ragdoll, Dead 같은 상호 배타적 상태 복원. |
| `TraversalNetState` | `UEnemyMovementComponent` 또는 Owner Actor | `ReplicatedUsing = OnRep_TraversalState` | 늦게 들어온 클라이언트가 현재 Traversal을 재생/보간. |
| `DismemberedPartsMask` | `UEnemyLimbComponent` 또는 Owner Actor | `ReplicatedUsing = OnRep_DismemberedPartsMask` | Bone 숨김과 절단 상태 영구 복원. |
| `CapsuleCollisionState` | Owner Actor | `ReplicatedUsing` | Traversal/death 중 collision divergence 방지. |
| `DeathTransform` | Owner Actor | `ReplicatedUsing` | Cosmetic ragdoll 시작 기준과 시체 위치 보정. |

일회성 이벤트:

| 이벤트 | 전달 방식 | 포함 데이터 | 비고 |
| --- | --- | --- | --- |
| HitReact 시작 | Unreliable Multicast 또는 RepNotify event id | `BoneName`, `HitLocation`, `HitNormal`, `Impulse`, `EventId` | 손실되어도 Health/Death 영구 상태는 유지되어야 한다. |
| 절단 발생 연출 | RepNotify event id + 영구 mask | `BodyPart`, `BoneName`, `Transform`, `Impulse` | 단발 Multicast만으로는 부족하다. |
| 발사/폭발/탄착 VFX | Gameplay Cue 또는 Unreliable Multicast | `Location`, `Normal`, `PhysicalMaterial` | 중도 참가 복구 불필요. |
| Montage presentation 시작 | OnRep 상태 기반 | `Montage`, `Section`, `ServerStartTime`, `PlayRate` | 단순 Multicast보다 상태 복제 권장. |

클라이언트 전용 처리:

- Niagara, Particle, Sound, Decal
- 카메라 FOV, PostProcess, recoil
- Cosmetic ragdoll 세부 물리
- 절단 파츠의 장기 물리 시뮬레이션
- Animation Budget Significance

## 9. Traversal 및 Motion Warping 분석

현재 `UEnemyMovementComponent::StartNavLinkTraversal`은 Generated NavLink 진입 시점에 `LinkType`을 보고 Vault/ClimbUp/Drop을 선택한다. Vault와 ClimbUp은 `BeginParkour`로 `MOVE_Flying`과 Capsule `NoCollision`을 적용한 뒤, `MotionWarping->AddOrUpdateWarpTargetFromLocationAndRotation`을 3회 호출하고 Montage를 재생한다. 종료는 `AnimInstance->Montage_SetEndDelegate`에 연결된 `OnMontageEnded`에서 `EndParkour`를 호출하는 구조다.

Dedicated Server 관점의 문제:

- 서버가 Montage를 정상 재생하지 못하거나 AnimInstance가 없으면 `MontageDuration <= 0.f`가 될 수 있고 Traversal 시작/종료가 실패한다.
- 서버에서 Montage는 재생되더라도 애니메이션 평가와 Root Motion이 클라이언트와 동일하다고 보장할 수 없다.
- WarpTarget이 복제되지 않으므로 클라이언트가 독자적으로 계산하면 시작 위치, NavLink endpoint, floor projection, network smoothing 차이로 결과가 달라질 수 있다.
- `TraversalDestination`, `TraversalType`, `bIsTraversingNavLink`가 복제되지 않아 중도 참가와 relevancy 재진입이 불가능하다.
- `FinishUsingCustomLink`가 `FinishNavLinkTraversal`에서만 호출되므로, Montage 종료 손실 시 AI path following이 멈출 수 있다.

권장 구조:

```cpp
USTRUCT()
struct FEnemyTraversalNetState
{
	GENERATED_BODY()

	UPROPERTY()
	bool bActive = false;

	UPROPERTY()
	uint8 TraversalId = 0;

	UPROPERTY()
	ETraversalType TraversalType = ETraversalType::Walk;

	UPROPERTY()
	FTransform StartTransform;

	UPROPERTY()
	FTransform EndTransform;

	UPROPERTY()
	TArray<FTransform> WarpTargets;

	UPROPERTY()
	float ServerStartTime = 0.0f;

	UPROPERTY()
	float ExpectedDuration = 0.0f;
};
```

서버 함수 예시:

```cpp
void UEnemyMovementComponent::StartNavLinkTraversal(...)
{
	if (!Character || !Character->HasAuthority())
	{
		return;
	}

	// 서버에서 LinkType, 시작/종료, WarpTarget, 예상 시간 계산.
	// MovementMode/Collision 변경도 서버 상태 전이로 처리.
	// 계산된 FEnemyTraversalNetState를 Owner Actor의 ReplicatedUsing property에 저장.
}
```

클라이언트 함수 예시:

```cpp
void AEnemyCharacter::OnRep_TraversalNetState()
{
	// bActive면 복제된 WarpTargets와 ServerStartTime으로 MotionWarping/Montage presentation 시작.
	// bActive가 false면 Montage 중단, collision/movement presentation 복구.
}
```

이벤트 한 번만 보내는 방식과 상태 복제 방식 비교:

| 방식 | 장점 | 단점 | 판단 |
| --- | --- | --- | --- |
| 단발 Multicast `PlayTraversalMontage` | 구현이 빠르고 대역폭이 작다. | 패킷 손실, 중도 참가, relevancy 재진입 시 상태 복원이 안 된다. Montage 종료 손실 시 상태 불일치가 남는다. | 단기 프로토타입만 가능. |
| Replicated traversal state | 중도 참가와 relevancy 복구가 가능하다. 서버 보정과 클라 presentation을 분리할 수 있다. | 설계와 검증 공수가 크다. | 현재 프로젝트 권장 방식. |

## 10. Combat 및 Damage 분석

현재 플레이어 무기 피해 경로는 세 갈래다.

1. `UOBGameplayAbility_RangedWeapon::PerformServerWeaponTrace`
   - 서버에서 LineTrace를 수행한다.
   - HitActor가 있으면 `UGameplayStatics::ApplyPointDamage`를 호출한다.
   - 이후 Target ASC가 있으면 GameplayEffect를 적용한다.
   - AI는 `AEnemyCharacter::TakeDamage`를 통해 물리 반응을 받을 수 있지만, Health/GAS 사망은 진행되지 않는다.

2. `UOBGameplayAbility_Melee::PerformMeleeTrace`
   - 서버에서 Overlap을 수행한다.
   - `UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor)`가 성공할 때만 GameplayEffect를 적용한다.
   - `AEnemyCharacter`는 C++ 기준 ASC가 없으므로 근접 피해 대상이 되지 않는다.

3. `AOBGrenadeProjectile::Explode`
   - 서버에서 폭발 타이머와 Overlap을 수행한다.
   - Target ASC가 있을 때만 GameplayEffect를 적용한다.
   - `AEnemyCharacter`는 C++ 기준 ASC가 없으므로 수류탄 피해 대상이 되지 않는다.

권장 사항:

- AI 피해 모델을 하나로 통일해야 한다.
- 선택지 1: `AEnemyCharacter`에 단순 Health/Death replicated property를 추가하고 `TakeDamage`에서 서버 권위로 체력 감소/사망/절단 판정을 처리한다.
- 선택지 2: AI용 `UOBAbilitySystemComponent`와 `UOBAttributeSetBase`를 `AEnemyCharacter` 또는 별도 AI State Actor에 붙이고, 모든 무기 피해를 GAS로 통일한다.
- 현재 프로젝트는 이미 플레이어가 GAS를 사용하므로 장기적으로 선택지 2가 일관성이 높다. 단, AI가 수백 개면 ASC replication mode, Attribute replication 조건, GameplayCue 비용을 별도 최적화해야 한다.

## 11. Ragdoll 및 Physics 분석

현재 물리 관련 경로:

- `AEnemyCharacter::TakeDamage`
  - `GetCharacterMovement()->StopMovementImmediately()`
  - `GetMesh()->SetAllBodiesBelowSimulatePhysics(BoneName, true, true)`
  - `GetMesh()->AddImpulse(...)`
  - `ReactTimeline.PlayFromStart()`
  - `HandleReactTimelineFinished`에서 모든 물리 blend/simulation을 해제

- `UEnemyMovementComponent::TickTraversalDrop`
  - 낙하 중 Mesh `SetSimulatePhysics(true)`
  - Mesh collision을 `PhysicsOnly`로 변경
  - Capsule Pawn collision을 Ignore로 변경
  - 3초 타이머 후 Mesh/Capsule 상태를 복구하고 `FinishNavLinkTraversal`

- `AOBCharacterBase::StartRagdoll`
  - 플레이어 사망용 참고 구현. `bIsDead`가 RepNotify로 복제되고 클라에서도 `StartDeath`를 호출한다.

Dedicated Server에서 서버가 모든 ragdoll physics를 권위 있게 계산하고 복제하는 방식은 AI 20~100개에서 비용이 크다. 권장 모델은 다음과 같다.

| 상태 | 서버 처리 | 클라이언트 처리 |
| --- | --- | --- |
| HitReact | Hit 판정, Damage, Reaction event id 확정 | Bone/Impulse 기반 로컬 부분 물리 연출 |
| Drop Traversal | 시작/끝 위치, 완료 시간, NavLink 완료 확정 | 낙하/구르기/일어서기 Montage 또는 local ragdoll |
| Death Ragdoll | 사망 위치, 최종 capsule/collision, despawn timer 확정 | Cosmetic full ragdoll. 필요 시 일정 시간 후 서버 DeathTransform에 보정 |
| Dismembered Part | 절단 상태와 파츠 발생 Transform/Impulse 확정 | 파츠 Actor 로컬 생성 및 물리 |

서버에서 반드시 유지해야 하는 것은 gameplay collision과 AI path/nav 상태다. 시각적 ragdoll pose 전체를 복제하는 것은 권장하지 않는다.

## 12. Dismemberment 분석

현재 `AEnemyCharacter::PhysicalMaterialProcess`는 PhysicalMaterial에 따라 `Head` Bone을 숨기거나 `MeshPartDestruction`을 호출한다. `MeshPartDestruction`은 다음을 수행한다.

- `ChildActorSkeletalMesh->GetSocketTransform(BoneName)`으로 절단 파츠 Spawn 위치 계산
- `World->SpawnActor<AStaticMeshActor>`
- StaticMesh/Collision/Physics 설정
- `ChildActorSkeletalMesh->HideBoneByName(BoneName, PBO_Term)`

상태 분류:

| 항목 | 분류 | 현재 상태 | 권장 |
| --- | --- | --- | --- |
| 어떤 부위가 절단되는가 | 게임플레이 판정 | 서버/클라 구분 없음 | 서버 권위 |
| 절단 여부 | 영구 상태 | 복제 없음 | `DismemberedPartsMask` Replicated State |
| Bone 숨김 | 영구 표현 상태 | 직접 `HideBoneByName` | OnRep에서 재적용 |
| 절단 발생 위치/방향 | 일회성 연출 이벤트 | 로컬 계산 | 서버 event로 전달 |
| Impulse | 일회성 연출 이벤트 | 로컬 물리 | 서버 event로 전달, 클라 local physics |
| 절단 파츠 Actor | 연출 또는 gameplay object | non-replicated `AStaticMeshActor` | 기본은 client cosmetic, gameplay 충돌이 필요할 때만 서버 actor + 낮은 frequency/cull |
| 파츠 Physics | 클라이언트 로컬 Physics 결과 | 서버/클라 구분 없음 | Client Cosmetic Only |
| 중도 참가/relevancy 복구 | 영구 상태 복원 | 불가 | bitmask + OnRep reconstruction |

단발 Multicast만으로 절단을 처리하면 늦게 접속한 클라이언트에서 Bone이 다시 붙어 보인다. 절단은 영구 상태와 일회성 이벤트를 반드시 분리해야 한다.

## 13. 권장 네트워크 아키텍처

권장 아키텍처는 서버 권위 상태 머신형이다. 현재 코드에 이미 `UEnemyStatusComponent`, `UEnemyLimbComponent`, `UEnemyMovementComponent`가 나뉘어 있으므로, 완전히 새 Actor를 만들기보다 기존 컴포넌트의 책임을 명확히 하는 방식이 적합하다.

권장 소유 구조:

| 책임 | 권장 소유자 | 설명 |
| --- | --- | --- |
| Health, Death, ActionState | `UEnemyStatusComponent` 또는 `AEnemyCharacter` | 서버 전이와 RepNotify 담당. |
| Traversal state | `UEnemyMovementComponent`가 계산, `AEnemyCharacter`가 복제 | Component replication보다 Owner Actor property가 단순하다. |
| Limb/Dismember state | `UEnemyLimbComponent` 또는 `AEnemyCharacter` | `DismemberedPartsMask`, 마지막 절단 event 관리. |
| AI decision/target | `AEnemyController` | 서버 전용. 필요한 표현만 Owner Actor에 복제. |
| Cosmetic presentation | `AEnemyCharacter`와 AnimInstance/Blueprint | OnRep 상태를 받아 Montage/VFX/SFX 실행. |

권장 상태 예시:

```cpp
UENUM(BlueprintType)
enum class EEnemyNetworkActionState : uint8
{
	None,
	Moving,
	Traversal,
	Attacking,
	HitReact,
	Ragdoll,
	Dead
};
```

복제 데이터 예시:

```cpp
UPROPERTY(ReplicatedUsing = OnRep_ActionState)
EEnemyNetworkActionState ActionState = EEnemyNetworkActionState::None;

UPROPERTY(ReplicatedUsing = OnRep_TraversalNetState)
FEnemyTraversalNetState TraversalNetState;

UPROPERTY(ReplicatedUsing = OnRep_DismemberedParts)
uint32 DismemberedPartsMask = 0;

UPROPERTY(ReplicatedUsing = OnRep_IsDead)
bool bIsDead = false;
```

RPC 사용 원칙:

- `Server RPC`: 플레이어가 AI에 직접 명령을 보내는 구조가 아니라면 AI 내부에는 최소화한다. 클라이언트 입력은 기존 플레이어 Ability/Controller 경로에서 서버로 들어오고 AI는 서버 판정 결과만 받는다.
- `Client RPC`: 특정 소유 클라이언트에만 알려야 하는 AI 정보가 거의 없으므로 기본적으로 불필요하다.
- `NetMulticast`: 폭발, 피격 이펙트, 포효 사운드 같은 일회성 cosmetic에만 사용한다. 영구 상태에는 사용하지 않는다.
- `Replicated Property + RepNotify`: Traversal, Death, Dismemberment, Health, ActionState 같은 복원 가능한 상태에 사용한다.

## 14. 전환 방안 비교

### 방안 A: 최소 수정형

목표는 현재 Actor, `CharacterMovementComponent`, Montage 중심 구조를 최대한 유지하면서 Dedicated Server에서 일단 플레이 가능한 상태를 만드는 것이다.

수정 대상 클래스:

- `AEnemyCharacter`
- `UEnemyMovementComponent`
- `UEnemyStatusComponent`
- `UEnemyLimbComponent`
- `AEnemyController`
- `UEnemyGenNavLinksProxy`

추가할 Replicated Property:

- `bIsDead`
- `CurrentHealth`
- `TraversalType`
- `bIsTraversingNavLink`
- `TraversalDestination`
- `DismemberedPartsMask`
- `LastHitReactionEventId`

필요 RPC:

- `NetMulticast, Unreliable`: `Multicast_PlayHitReaction`, `Multicast_SpawnDismemberCosmetic`, `Multicast_PlayTraversalMontage` 정도로 제한.
- Server RPC는 AI 자체에는 기본적으로 추가하지 않는다.

권한 검사 위치:

- `AEnemyCharacter::TakeDamage`: 서버만 Health/Dismember 판정.
- `UEnemyMovementComponent::StartNavLinkTraversal`: 서버만 Traversal 시작.
- `UEnemyMovementComponent::FinishNavLinkTraversal`: 서버만 PathFollowing 완료.
- `UEnemyMovementComponent::TickTraversalDrop`: 서버 gameplay 상태와 클라 cosmetic 분리.

예상 공수:

- 90~150시간
- 15~25 개발일
- 15~25 인일

장점:

- 빠르게 Dedicated Server에서 AI 이동/피격/사망의 기본 형태를 확인할 수 있다.
- 기존 Montage와 MotionWarping 애셋을 크게 바꾸지 않는다.
- 플레이어 측 기존 GAS/weapon 구조를 많이 건드리지 않는다.

단점:

- 중도 참가와 relevancy 복구가 제한적이다.
- Traversal과 Death가 복잡해질수록 RepNotify와 Multicast가 섞여 유지보수가 어려워진다.
- 다수 AI 최적화와 예측 보정까지 가면 다시 구조 변경이 필요할 가능성이 높다.

확장 한계:

- 절단/래그돌/Traversal이 모두 단발 이벤트 중심이면 상태 복원이 어렵다.
- 100 AI 환경에서 replicated movement, physics, multicast 비용을 낮추기 어렵다.

### 방안 B: 서버 권위 상태 머신형

목표는 Traversal, Combat, Hit Reaction, Death, Dismemberment를 명시적인 서버 권위 상태로 관리하고, 클라이언트는 복제 상태를 기반으로 표현을 재구성하게 만드는 것이다.

상태 소유 주체:

- `AEnemyCharacter` 또는 `UEnemyStatusComponent`가 `ActionState`, `bIsDead`, Health를 소유한다.
- `UEnemyMovementComponent`는 서버에서 Traversal state를 계산하고 Owner Actor의 replicated state를 갱신한다.
- `UEnemyLimbComponent`는 절단 bitmask와 절단 event를 관리한다.

상태 전이 주체:

- `AEnemyController`: AI decision, target, MoveTo
- `UEnemyMovementComponent`: Traversal start/end
- `AEnemyCharacter`/`UEnemyStatusComponent`: damage, hit react, death
- `UEnemyLimbComponent`: dismemberment

복제 데이터:

- `EEnemyNetworkActionState`
- `FEnemyTraversalNetState`
- `FEnemyHitReactionNetEvent`
- `DismemberedPartsMask`
- `FEnemyDismemberNetEvent`
- `bIsDead`, `DeathTransform`, `DeathServerTime`
- 선택적으로 `CombatFacing`, `AttackId`, `AttackSection`

클라이언트 표현 방식:

- `OnRep_ActionState`: Montage/VFX/SFX 시작/종료
- `OnRep_TraversalNetState`: MotionWarping target 적용, Montage 시간 보정
- `OnRep_DismemberedPartsMask`: Bone 숨김 상태 재적용
- `OnRep_IsDead`: Capsule/Mesh presentation 전환, cosmetic ragdoll

중도 참가 및 relevancy 복구:

- Actor가 relevancy에 들어오면 replicated property snapshot으로 현재 ActionState, Traversal 진행률, 절단 상태, 사망 상태를 복원한다.
- 일회성 event를 놓쳐도 영구 상태가 맞으므로 시각적 복구가 가능하다.

예상 공수:

- 240~420시간
- 40~70 개발일
- 40~70 인일

장점:

- Dedicated Server, 중도 참가, relevancy 재진입에 강하다.
- Montage/AnimNotify 손실이 gameplay 고착으로 이어지지 않는다.
- 다수 AI 최적화에서 상태별 NetUpdateFrequency, dormancy, cull 정책을 적용하기 쉽다.

단점:

- 초기 설계와 테스트 공수가 크다.
- 기존 Blueprint/Montage/AnimNotify 의존 구조를 정리해야 할 수 있다.
- Root Motion 애셋 설정 검증이 필요하다.

결정:

현재 OutBreak 프로젝트에는 방안 B를 권장한다. 이유는 Traversal, Motion Warping, 부분 물리, Dismemberment가 모두 중도 참가와 상태 복구에 취약한 구조이기 때문이다. 단기적으로는 방안 A의 일부를 Phase 0~1에 적용해 플레이 가능한 기준선을 만든 뒤, Phase 2부터 방안 B 구조로 전환하는 절충이 현실적이다.

## 15. 공수 산정

가정 조건:

- AI 최대 수: 1차 검증 20개, 최적화 목표 100개
- 동시 접속자 수: 기본 2~4명, `AOBExpeditionGameMode::ResolveMaxPlayers` 폴백 기준 최대 12명 검토
- 네트워크 지연: 0ms, 100ms, 200ms
- 패킷 손실: 0%, 5%
- 기존 애셋 수정: 가능하다고 가정하되 Root Motion/Montage 수정은 별도 비용
- 애니메이션 수정: Montage section/notify 정리는 가능하다고 가정
- 엔진 소스 수정: 없음
- 클라이언트 예측: AI에는 적극 적용하지 않고, server state + client presentation 중심
- 1 개발일: 실제 구현 및 검증 6시간
- 1 인일: 개발자 1명이 1일 작업

| 항목 | 낙관적 시간/일/인일 | 현실적 시간/일/인일 | 비관적 시간/일/인일 |
| --- | --- | --- | --- |
| 코드 구조 분석 | 10h / 1.7일 / 1.7인일 | 16h / 2.7일 / 2.7인일 | 24h / 4.0일 / 4.0인일 |
| Authority 분리 | 18h / 3.0일 / 3.0인일 | 30h / 5.0일 / 5.0인일 | 48h / 8.0일 / 8.0인일 |
| Actor 및 Component 복제 설정 | 12h / 2.0일 / 2.0인일 | 20h / 3.3일 / 3.3인일 | 36h / 6.0일 / 6.0인일 |
| AI 이동 동기화 | 8h / 1.3일 / 1.3인일 | 14h / 2.3일 / 2.3인일 | 24h / 4.0일 / 4.0인일 |
| Traversal 동기화 | 24h / 4.0일 / 4.0인일 | 42h / 7.0일 / 7.0인일 | 72h / 12.0일 / 12.0인일 |
| Motion Warping 및 Root Motion 검증 | 12h / 2.0일 / 2.0인일 | 24h / 4.0일 / 4.0인일 | 40h / 6.7일 / 6.7인일 |
| Combat 및 Damage 동기화 | 12h / 2.0일 / 2.0인일 | 24h / 4.0일 / 4.0인일 | 42h / 7.0일 / 7.0인일 |
| Ragdoll 동기화 | 16h / 2.7일 / 2.7인일 | 32h / 5.3일 / 5.3인일 | 56h / 9.3일 / 9.3인일 |
| Dismemberment 동기화 | 20h / 3.3일 / 3.3인일 | 40h / 6.7일 / 6.7인일 | 72h / 12.0일 / 12.0인일 |
| Gameplay Ability System 검증 | 12h / 2.0일 / 2.0인일 | 20h / 3.3일 / 3.3인일 | 36h / 6.0일 / 6.0인일 |
| Dedicated Server 빌드 설정 | 6h / 1.0일 / 1.0인일 | 10h / 1.7일 / 1.7인일 | 18h / 3.0일 / 3.0인일 |
| 테스트 환경 구성 | 8h / 1.3일 / 1.3인일 | 16h / 2.7일 / 2.7인일 | 28h / 4.7일 / 4.7인일 |
| 버그 수정 및 안정화 | 18h / 3.0일 / 3.0인일 | 36h / 6.0일 / 6.0인일 | 72h / 12.0일 / 12.0인일 |
| 성능 프로파일링 | 12h / 2.0일 / 2.0인일 | 24h / 4.0일 / 4.0인일 | 48h / 8.0일 / 8.0인일 |
| 패킷 및 대역폭 최적화 | 16h / 2.7일 / 2.7인일 | 32h / 5.3일 / 5.3인일 | 64h / 10.7일 / 10.7인일 |
| 총합 | 204h / 34.0일 / 34.0인일 | 380h / 63.3일 / 63.3인일 | 680h / 113.3일 / 113.3인일 |

최종 범위:

| 목표 수준 | 예상 공수 | 포함 범위 |
| --- | --- | --- |
| Dedicated Server에서 일단 플레이 가능한 수준 | 90~150h / 15~25일 / 15~25인일 | AI Spawn 확인, 서버 MoveTo, 기본 피해/사망, 최소 Traversal replication, 주요 crash/deadlock 제거 |
| 기능적으로 안정적인 수준 | 240~420h / 40~70일 / 40~70인일 | 상태 머신, Traversal/MotionWarping 복구, Damage/GAS 통일, Death/Ragdoll/Dismemberment 복제, 중도 참가/relevancy 복구 |
| 다수 AI 환경에서 최적화된 수준 | 420~680h / 70~113.3일 / 70~113.3인일 | 100 AI, bandwidth 최적화, dormancy/cull/frequency tuning, physics 비용 축소, 장시간 soak test |

불확실성 원인:

- AI Blueprint의 Behavior Tree/Perception/replication 설정을 C++에서 확인할 수 없다.
- Root Motion, Montage Notify, Motion Warping notify 구성은 `.uasset` 내부 설정 확인이 필요하다.
- `UBaseGeneratedNavLinksProxy` 구현 위치가 현재 검색 범위에서 확인되지 않았다.
- 실제 AI 최대 수, 공격 빈도, 절단 빈도, 파츠 lifetime 정책이 아직 명확하지 않다.

## 16. 위험도와 기술 부채

Critical:

- Traversal 종료가 Montage end delegate에 의존한다.
- AI Health/Death replicated state가 없다.
- Drop/Ragdoll/Hit physics가 서버/클라 상태를 분리하지 않는다.
- Dismemberment가 영구 상태 복제 없이 Bone hide와 non-replicated actor spawn으로 처리된다.

High:

- AI C++에 복제 property/RPC/authority gate가 거의 없다.
- MotionWarping target과 traversal start time이 복제되지 않는다.
- AIController 의사결정/Perception/Target 구조가 C++에서 확인되지 않는다.
- AnimationBudget과 Mesh update가 gameplay state와 섞일 위험이 있다.
- Capsule/Mesh collision state가 복제되지 않는다.
- 다수 빈 Tick이 존재한다.
- AI Spawn/Despawn/ObjectPool 코드가 확인되지 않는다.
- 영구 상태에 Multicast를 사용할 위험이 있다.

Medium:

- Drop timer lambda 수명 안전성.
- `bIsHit` 중복 피격 조건의 의미 불일치.
- `AModularSkeletalMeshActor`의 네트워크/표현 책임 불명확.
- Dedicated Server 빌드/패키징 프로파일 검증 필요.

기술 부채:

- `UEnemyStatusComponent`, `UEnemyLimbComponent`가 이름상 책임은 있지만 비어 있다.
- AI와 플레이어 피해 모델이 분리되어 무기 종류별 AI 피해 결과가 다르다.
- 애니메이션/물리/게임플레이 상태 전이가 한 함수에 섞여 있다.
- 중도 참가와 Net Relevancy 재진입을 고려한 snapshot state가 없다.

## 17. 구현 단계

| Phase | 목표 | 수정 파일 | 주요 작업 | 완료 조건 | 예상 시간 | 선행 조건 | 위험 요소 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Phase 0: Dedicated Server 빌드 및 실행 확인 | 서버 타깃 빌드/실행, 맵 진입, AI 스폰 확인 | `Source/OutBreakServer.Target.cs`, Config, 실행 스크립트 | Server cook/build, 로그 카테고리, NetMode 로그 추가 | Dedicated Server 1대 + Client 1명 접속 | 10~18h | 서버 타깃 유지 | OnlineSubsystem/맵 travel 설정 |
| Phase 1: AI Spawn, 이동, 타깃, 데미지 서버 권위화 | 기본 AI gameplay 서버 실행 | `AEnemyCharacter`, `AEnemyController`, `UEnemyStatusComponent`, weapon abilities | Health/Death, authority gate, melee/grenade AI 피해 통일 | AI가 서버에서 이동/피격/사망하고 클라에 보임 | 40~80h | Phase 0 | Blueprint BT/Perception 확인 필요 |
| Phase 2: Traversal 및 Montage 동기화 | NavLink traversal 복제 | `UEnemyMovementComponent`, `UEnemyGenNavLinksProxy`, `AEnemyCharacter` | `FEnemyTraversalNetState`, OnRep presentation, server completion 분리 | 0/100/200ms에서 Vault/Climb/Drop이 완료되고 path following 재개 | 60~110h | Phase 1 | Root Motion/Montage asset 수정 가능성 |
| Phase 3: 사망, Ragdoll, Dismemberment 동기화 | death/dismember 복원 가능 상태 | `AEnemyCharacter`, `UEnemyLimbComponent`, `AModularSkeletalMeshActor` | death state, dismember mask, cosmetic part spawn, ragdoll policy | 중도 접속 클라가 사망/절단 상태를 복원 | 70~130h | Phase 1 | 파츠 physics 비용 |
| Phase 4: 중도 참가 및 Net Relevancy 상태 복구 | snapshot 기반 복원 | AI replicated properties, GameMode/Subsystem | relevancy 재진입 test, OnRep idempotency, duplicate event 방지 | 범위 이탈 후 재진입해도 상태 일치 | 30~60h | Phase 2~3 | OnRep 순서/초기화 타이밍 |
| Phase 5: 다수 AI 네트워크 최적화 | 20/100 AI 성능 안정화 | AI Actor/Component, Net settings | Tick 제거, NetUpdateFrequency, dormancy, cull, physics LOD | 100 AI에서 stat net/game/physics 기준 목표 달성 | 60~120h | Phase 4 | bandwidth와 server physics 병목 |
| Phase 6: 장시간 안정성 테스트 | soak test와 버그 안정화 | 전체 관련 파일 | 30~120분 soak, packet loss, reconnect, crash 로그 수집 | 치명적 deadlock/crash 없음 | 30~80h | Phase 5 | 재현 어려운 timing issue |

## 18. 테스트 계획

테스트 환경:

- Dedicated Server 1대
- 클라이언트 1명
- 클라이언트 2명 이상
- AI 1개, 20개, 100개
- 지연 0ms, 100ms, 200ms
- 패킷 손실 0%, 5%
- 클라이언트 중도 접속
- Net Relevancy 범위 이탈 후 재진입
- 서버에서 AI 사망
- Traversal 도중 연결 상태 악화
- Ragdoll 도중 중도 접속
- 절단 이후 중도 접속

사용 도구와 명령:

- Dedicated Server 빌드: `OutBreakServer.Target.cs` 기반 Development Server 빌드
- Multi-Process Play In Editor
- Unreal Insights
- Network Profiler
- `stat net`
- `stat game`
- `stat physics`
- Network Emulation: PIE network settings 또는 command line packet lag/loss
- 로그 카테고리 분리: AI authority, traversal, damage, dismemberment, relevancy
- Authority 및 NetMode 로그: `HasAuthority`, `GetLocalRole`, `GetNetMode`를 주요 상태 전이에 출력

검증 항목:

| 테스트 | 기대 결과 | 실패 판정 |
| --- | --- | --- |
| Dedicated Server + Client 1 + AI 1 | AI가 서버에서 스폰/이동/피격/사망하고 클라에 동일하게 보인다. | 클라에서만 AI가 움직이거나, 서버 로그에 상태 전이가 없다. |
| Client 2명 + AI 1 | 두 클라이언트가 같은 AI 위치, Health, Death, Dismember 상태를 본다. | 클라이언트별 사망/절단/위치가 다르다. |
| AI 20 + 100ms | MoveTo와 Traversal이 완료되고 path following이 재개된다. | NavLink에서 멈추거나 server correction으로 순간이동한다. |
| AI 100 + 0ms | `stat game`, `stat net`, `stat physics`가 목표 예산 내 유지된다. | 서버 frame time 급증, bandwidth 급증, physics time 급증. |
| 200ms + 5% loss Traversal | 클라 presentation은 지연 보정되며 서버 상태는 완료된다. | Montage가 stuck, capsule collision이 복구되지 않음. |
| 중도 접속 중 Traversal | 클라가 현재 Traversal state를 복원하거나 완료 상태로 보정한다. | AI가 T-pose/잘못된 위치/붙은 capsule로 보인다. |
| Relevancy 이탈 후 재진입 | 절단/사망/Traversal 상태가 snapshot으로 재구성된다. | 절단된 팔이 다시 보이거나 죽은 AI가 살아 보인다. |
| Ragdoll 도중 중도 접속 | Death state와 death transform 기준으로 cosmetic ragdoll 또는 dead pose가 보인다. | 살아있는 collision 또는 다른 위치의 시체가 보인다. |
| 절단 이후 중도 접속 | `DismemberedPartsMask`에 따라 Bone hide가 재적용된다. | 절단 상태가 복원되지 않는다. |
| Melee/Grenade vs AI | AI Health가 서버에서 감소하고 모든 클라에 복제된다. | ASC 없음으로 피해가 적용되지 않는다. |
| Animation Budget off/on | 서버 gameplay 상태가 animation budget에 영향 받지 않는다. | ReducedWork 상태에서 traversal/death 종료가 손실된다. |

## 19. 최종 권고안

OutBreak의 AI Dedicated Server 전환은 방안 B, 서버 권위 상태 머신형을 기준으로 진행하는 것이 맞다. 현재 AI 코드는 Movement/Animation/Physics/Destruction이 한 흐름에 섞여 있고, 영구 상태 복제가 없기 때문에 최소 수정형만으로는 중도 참가, relevancy 재진입, 다수 AI 최적화에서 다시 막힐 가능성이 높다.

우선순위는 다음과 같다.

1. Dedicated Server 실행과 AI Spawn/MoveTo 확인.
2. AI 피해 모델 통일. `AEnemyCharacter`에 Health/Death를 추가하거나 AI ASC를 도입한다.
3. `UEnemyMovementComponent` Traversal을 서버 상태 전이와 클라이언트 Montage 표현으로 분리한다.
4. Dismemberment를 `DismemberedPartsMask` 영구 상태와 cosmetic 파츠 이벤트로 분리한다.
5. Ragdoll/partial physics는 클라이언트 cosmetic 중심으로 전환한다.
6. 20 AI 검증 후 100 AI 기준으로 Tick, physics, NetUpdateFrequency, dormancy, cull을 최적화한다.

현실적인 개발 전략은 Phase 0~1에서 일단 플레이 가능한 Dedicated Server 기준선을 만들고, Phase 2~4에서 상태 복제 기반으로 안정성을 확보한 뒤, Phase 5~6에서 다수 AI 성능과 장시간 안정성을 잡는 것이다.

## 20. 확인이 필요한 미결 사항

- `AEnemyCharacter` Blueprint에서 `bReplicates`, `Replicate Movement`, `NetUpdateFrequency`, `NetCullDistanceSquared`, `NetDormancy`가 어떻게 설정되어 있는지 확인해야 한다.
- AI Behavior Tree, Blackboard, AI Perception이 Blueprint/Asset에서 구성되어 있는지 확인해야 한다.
- `VaultMontage`, `ClimbUpMontage`, `MantleMontage`의 Root Motion, Montage Section, AnimNotify, Motion Warping Notify 설정을 에디터에서 확인해야 한다.
- `UBaseGeneratedNavLinksProxy` 구현 위치와 LinkProxyId 수명 정책을 확인해야 한다.
- AI가 GAS를 사용할지, 단순 Health replicated property를 사용할지 설계 결정을 내려야 한다.
- Melee/Grenade가 AI에 피해를 주어야 한다면 `AEnemyCharacter`가 ASC를 제공하지 않는 현 구조를 반드시 수정해야 한다.
- 절단 파츠가 gameplay collision을 가져야 하는지, 순수 cosmetic인지 결정해야 한다.
- 시체/ragdoll이 서버에서 일정 시간 후 despawn되는지, object pool로 돌아가는지 수명 정책을 정해야 한다.
- 목표 동시 AI 수, 절단 빈도, 평균 전투 빈도, 파츠 lifetime이 확정되어야 최종 bandwidth 예산을 산정할 수 있다.
- Dedicated Server 패키징과 OnlineSubsystemSteam 설정은 실제 서버 실행으로 검증해야 한다.
