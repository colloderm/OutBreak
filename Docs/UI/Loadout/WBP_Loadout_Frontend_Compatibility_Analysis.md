# WBP_Loadout Frontend Compatibility Analysis

Analysis Status: Partial
Implementation Performed: No
Asset Modified: No
Source Modified: No

> Partial 사유: Unreal Editor 5.7 Commandlet 및 Python으로 대상 WidgetBlueprint 로드와 생성 클래스 로드는 성공했으나, 현재 노출된 Python API에서 `WidgetBlueprint.WidgetTree`, `ParentClass`, `Bindings`, `UbergraphPages`가 protected 또는 비노출 상태라 상세 Widget Tree, Is Variable, Graph node, Property Binding, Animation 목록을 확정 추출하지 못했다. 따라서 이 문서는 C++ 계약, Asset Registry 의존/참조 관계, 에디터 로드 결과, 그리고 보조적인 문자열 존재 확인을 근거로 작성하며, 상세 Designer 속성은 구현 전 에디터 UI 또는 별도 C++ Editor Utility로 재검증해야 한다.

## 1. 분석 범위

대상 신규 프론트 에셋:

`C:\Users\Admin\Documents\Unreal Projects\OutBreak\Content\UI\Lobby\Loadout\WBP_Loadout.uasset`

Unreal 경로:

`/Game/UI/Lobby/Loadout/WBP_Loadout`

비교 대상 기존 로비/로드아웃 에셋:

- `/Game/UI/Lobby/WBP_Loadout`
- `/Game/UI/Lobby/WBP_Lobby`
- `/Game/UI/Lobby/WBP_WeaponSelect`
- `/Game/UI/Lobby/WBP_WeaponEntry`

이번 분석에서는 `.uasset`, C++, Blueprint Graph, Config를 수정하지 않았다. 에디터 자동화는 읽기 전용 로드 및 참조 조회 목적으로만 실행했다.

## 2. 조사한 파일 및 에셋

조사한 C++ 파일:

- `Source/OutBreak/Public/UI/Widgets/Lobby/OBLoadoutWidget.h`
- `Source/OutBreak/Private/UI/Widgets/Lobby/OBLoadoutWidget.cpp`
- `Source/OutBreak/Public/UI/Widgets/Lobby/OBLobbyWidget.h`
- `Source/OutBreak/Private/UI/Widgets/Lobby/OBLobbyWidget.cpp`
- `Source/OutBreak/Public/UI/Widgets/Lobby/OBWeaponSelectWidget.h`
- `Source/OutBreak/Private/UI/Widgets/Lobby/OBWeaponSelectWidget.cpp`
- `Source/OutBreak/Public/UI/Widgets/Lobby/OBWeaponEntryWidget.h`
- `Source/OutBreak/Private/UI/Widgets/Lobby/OBWeaponEntryWidget.cpp`
- `Source/OutBreak/Public/LoadOut/OBLoadoutTypes.h`
- `Source/OutBreak/Public/LoadOut/OBLoadoutSubsystem.h`
- `Source/OutBreak/Private/LoadOut/OBLoadoutSubsystem.cpp`
- `Source/OutBreak/Public/Player/State/OBPlayerStateBase.h`
- `Source/OutBreak/Private/Player/State/OBPlayerStateBase.cpp`
- `Source/OutBreak/Public/Player/Controller/OBPlayerController.h`
- `Source/OutBreak/Private/Player/Controller/OBPlayerController.cpp`
- `Source/OutBreak/Public/Inventory/Components/OBInventoryComponent.h`
- `Source/OutBreak/Private/Inventory/Components/OBInventoryComponent.cpp`
- `Source/OutBreak/Public/Equipment/Components/OBEquipmentComponent.h`
- `Source/OutBreak/Private/Equipment/Components/OBEquipmentComponent.cpp`
- `Source/OutBreak/Public/Weapon/Data/OBWeaponData.h`
- `Source/OutBreak/Public/Weapon/Data/OBWeaponCatalog.h`
- `Source/OutBreak/Public/SaveGame/OBSaveGame.h`
- `Source/OutBreak/Public/UI/Widgets/Home/OBHomeHubWidget.h`
- `Source/OutBreak/Private/UI/Widgets/Home/OBHomeHubWidget.cpp`

조사한 에셋 및 참조 관계:

- 신규 `/Game/UI/Lobby/Loadout/WBP_Loadout`: Editor 로드 성공, Blueprint class `/Game/UI/Lobby/Loadout/WBP_Loadout.WBP_Loadout_C` 로드 성공. Asset Registry 의존성은 `/Script/SlateCore`, `/Script/UMGEditor`, `/Script/UMG`만 확인됨. 참조자는 없음.
- 기존 `/Game/UI/Lobby/WBP_Loadout`: `/Script/OutBreak` 의존성 있음. `/Game/UI/Lobby/WBP_Lobby`가 참조함.
- `/Game/UI/Lobby/WBP_Lobby`: `/Game/GameAbilitySystem/DataAssets/DA_WeaponCatalog`, 기존 `/Game/UI/Lobby/WBP_Loadout`, `/Game/UI/Lobby/WBP_WeaponSelect`를 참조함. `/Game/Map/L_LobbyMap`, `/Game/UI/Home/WBP_HomeHub`가 참조함.
- `/Game/UI/Lobby/WBP_WeaponSelect`: `/Game/UI/Lobby/WBP_WeaponEntry`를 참조함.
- `/Game/UI/Lobby/WBP_WeaponEntry`: `/Script/OutBreak` 의존성 있음.

보조 문자열 확인:

- 신규 `/Game/UI/Lobby/Loadout/WBP_Loadout` 바이너리 문자열 테이블에는 `BTN_CategoryPrimary`, `BTN_PrimaryLoadoutCard`, `IMG_PrimaryWeaponPreview`, `TXT_PrimaryWeaponName`, `PB_Damage`, `TXT_AmmoCount` 등 지시문에 제시된 신규 이름들이 존재한다.
- 기존 `/Game/UI/Lobby/WBP_Loadout` 바이너리 문자열 테이블에는 `IconPrimary`, `NamePrimary`, `IconSecondary`, `NameSecondary`, `IconMelee`, `NameMelee`, `StatName`, `BarDamage`, `BarFireRate`, `BarAccuracy`, `BarRecoil`, `BarMobility`, `AmmoText`가 존재한다.
- 이 문자열 확인은 최종 구조 판정의 단독 근거로 사용하지 않았고, 에디터 로드/참조 관계 및 C++ 계약 보조 자료로만 사용했다.

## 3. 기존 Loadout 아키텍처

기존 로드아웃 UI는 `WBP_Lobby`가 두 하위 위젯을 소유하는 구조다.

- `UOBLobbyWidget`
  - `WeaponSelect`: `UOBWeaponSelectWidget`, `BindWidget`
  - `Loadout`: `UOBLoadoutWidget`, `BindWidget`
  - `NativeConstruct()`에서 `WeaponSelect->BuildList(WeaponCatalog)` 호출
  - `WeaponSelect->OnWeaponChosen`을 `HandleWeaponChosen()`에 연결
  - 0.3초 타이머로 `RefreshDynamic()` 호출
- `UOBWeaponSelectWidget`
  - `PrimaryBox`, `SecondaryBox`, `MeleeBox`: `UScrollBox`, `BindWidget`
  - `EntryWidgetClass`: 무기 항목 위젯 클래스
  - `BuildList()`가 `UOBWeaponCatalog::AvailableWeapons`를 순회해 `UOBWeaponEntryWidget`을 생성하고 슬롯별 ScrollBox에 배치
  - Entry 클릭 이벤트를 `OnWeaponChosen`으로 브로드캐스트
- `UOBWeaponEntryWidget`
  - `RootButton`: `UButton`, 클릭 이벤트 시작점
  - `IconImage`: `UImage`
  - `NameText`: `UTextBlock`
  - `CheckImage`: `UImage`, 선택 체크 표시
- `UOBLoadoutWidget`
  - 선택된 무기 배열을 받아 주/보조/근접 슬롯 이름과 아이콘을 갱신
  - 선택된 무기 하나의 상세 능력치를 갱신

데이터 및 저장 흐름:

- `UOBWeaponCatalog::AvailableWeapons`: 로비에서 선택 가능한 무기 목록
- `AOBWeaponBase::GetWeaponData()`: CDO에서 `UOBWeaponData`를 얻는 통로
- `UOBWeaponData`: 표시명, 아이콘, 슬롯, 카테고리, 데미지, RPM, 탄창/예비탄, 반동, 탄퍼짐 등 제공
- `UOBLoadoutSubsystem`: GameInstance 수명 로컬 로드아웃 저장소. `SetWeapon()` 호출 시 `OBPlayerProfile` SaveGame 슬롯에 즉시 저장
- `AOBPlayerController::Server_SetWeaponSlot()`: 로비 선택을 서버 PlayerState에 반영
- `AOBPlayerStateBase::SelectedWeapons`: 서버/클라 로비 선택 상태
- `AOBCharacterBase::PossessedBy()`: Expedition 진입 시 PlayerState 선택 무기 또는 PawnData 기본 무기를 Inventory에 추가
- `UOBInventoryComponent`: 실제 무기 슬롯, ActiveSlot, 탄약 풀, 장착 요청 처리
- `UOBEquipmentComponent`: 실제 `AOBWeaponBase` 스폰/장착 및 현재 무기 복제

## 4. 기존 UI 바인딩 계약

`UOBLoadoutWidget`이 요구하는 `BindWidget` 계약:

| 기존 변수명 | 타입 | 선언 위치 | 필수 여부 | 읽기/쓰기 | 갱신 시점 | 데이터 원본 | 실패 영향 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `IconPrimary` | `UImage` | `OBLoadoutWidget.h` | 필수 `BindWidget` | `SetBrushFromTexture()` | `RefreshLoadout()` | `UOBWeaponData::WeaponIcon` | BP Compile BindWidget 오류 또는 런타임 표시 누락 |
| `NamePrimary` | `UTextBlock` | `OBLoadoutWidget.h` | 필수 | `SetText()` | `RefreshLoadout()` | `UOBWeaponData::DisplayName` | BP Compile 오류 또는 이름 미표시 |
| `IconSecondary` | `UImage` | `OBLoadoutWidget.h` | 필수 | `SetBrushFromTexture()` | `RefreshLoadout()` | `WeaponIcon` | 동일 |
| `NameSecondary` | `UTextBlock` | `OBLoadoutWidget.h` | 필수 | `SetText()` | `RefreshLoadout()` | `DisplayName` | 동일 |
| `IconMelee` | `UImage` | `OBLoadoutWidget.h` | 필수 | `SetBrushFromTexture()` | `RefreshLoadout()` | `WeaponIcon` | 동일 |
| `NameMelee` | `UTextBlock` | `OBLoadoutWidget.h` | 필수 | `SetText()` | `RefreshLoadout()` | `DisplayName` | 동일 |
| `StatName` | `UTextBlock` | `OBLoadoutWidget.h` | 필수 | `SetText()` | `ShowStats()` | `DisplayName` | 상세 이름 미표시 |
| `BarDamage` | `UProgressBar` | `OBLoadoutWidget.h` | 필수 | `SetPercent()` | `ShowStats()` | `BaseDamage / MaxDamage` | 상세 능력치 미표시 |
| `BarFireRate` | `UProgressBar` | `OBLoadoutWidget.h` | 필수 | `SetPercent()` | `ShowStats()` | `RoundsPerMinute / MaxRPM` | 동일 |
| `BarAccuracy` | `UProgressBar` | `OBLoadoutWidget.h` | 필수 | `SetPercent()` | `ShowStats()` | `1 - BaseSpreadDegrees / MaxSpread` | 동일 |
| `BarRecoil` | `UProgressBar` | `OBLoadoutWidget.h` | 필수 | `SetPercent()` | `ShowStats()` | `VerticalRecoil + HorizontalRecoil` | 동일 |
| `BarMobility` | `UProgressBar` | `OBLoadoutWidget.h` | 필수 | `SetPercent(0.5f)` | `ShowStats()` | 현재 하드코딩 | 항상 0.5 표시 |
| `AmmoText` | `UTextBlock` | `OBLoadoutWidget.h` | 필수 | `SetText()` | `ShowStats()` | `MagazineSize`, `MaxReserveAmmo` | 탄약 텍스트 미표시 |

`UOBLobbyWidget` 계약:

| 기존 변수명 | 타입 | 선언 위치 | 필수 여부 | 읽기/쓰기 | 이벤트 | 실패 영향 |
| --- | --- | --- | --- | --- | --- | --- |
| `WeaponSelect` | `UOBWeaponSelectWidget` | `OBLobbyWidget.h` | 필수 `BindWidget` | `BuildList()`, `RefreshChecks()` 호출 | `OnWeaponChosen` 구독 | 무기 목록/선택 동작 불가 |
| `Loadout` | `UOBLoadoutWidget` | `OBLobbyWidget.h` | 필수 `BindWidget` | `ShowStats()`, `RefreshLoadout()` 호출 | 없음 | 로드아웃 표시/상세 갱신 불가 |

`UOBWeaponSelectWidget` 계약:

| 기존 변수명 | 타입 | 선언 위치 | 필수 여부 | 읽기/쓰기 | 이벤트 | 실패 영향 |
| --- | --- | --- | --- | --- | --- | --- |
| `PrimaryBox` | `UScrollBox` | `OBWeaponSelectWidget.h` | 필수 | `ClearChildren()`, `AddChild()` | 없음 | 주무기 항목 표시 불가 |
| `SecondaryBox` | `UScrollBox` | `OBWeaponSelectWidget.h` | 필수 | `ClearChildren()`, `AddChild()` | 없음 | 보조무기 항목 표시 불가 |
| `MeleeBox` | `UScrollBox` | `OBWeaponSelectWidget.h` | 필수 | `ClearChildren()`, `AddChild()` | 없음 | 근접무기 항목 표시 불가 |

`UOBWeaponEntryWidget` 계약:

| 기존 변수명 | 타입 | 선언 위치 | 필수 여부 | 읽기/쓰기 | 이벤트 | 실패 영향 |
| --- | --- | --- | --- | --- | --- | --- |
| `RootButton` | `UButton` | `OBWeaponEntryWidget.h` | 필수 | 클릭 이벤트 연결 | `OnClicked` | 무기 선택 불가 |
| `IconImage` | `UImage` | `OBWeaponEntryWidget.h` | 필수 | `SetBrushFromTexture()` | 없음 | 목록 아이콘 미표시 |
| `NameText` | `UTextBlock` | `OBWeaponEntryWidget.h` | 필수 | `SetText()` | 없음 | 목록 이름 미표시 |
| `CheckImage` | `UImage` | `OBWeaponEntryWidget.h` | 필수 | `SetVisibility()` | 없음 | 선택 체크 미표시 |

## 5. 현재 WBP_Loadout 구조

에디터 자동화로 확인된 사항:

- `/Game/UI/Lobby/Loadout/WBP_Loadout` 로드 성공
- `/Game/UI/Lobby/Loadout/WBP_Loadout.WBP_Loadout_C` 클래스 로드 성공
- 참조자 없음
- `/Script/OutBreak` 의존성 없음

확정하지 못한 사항:

- 정확한 부모 클래스 이름
- 전체 Widget Tree
- Widget별 클래스, Slot, Visibility, Is Enabled, Hit Test, Z Order
- Is Variable 여부
- Blueprint Graph 노드
- 연결된 이벤트
- Property Binding
- Widget Animation
- Named Slot

부모 클래스 판정:

- 기존 `/Game/UI/Lobby/WBP_Loadout`은 `/Script/OutBreak` 의존성이 있으므로 `UOBLoadoutWidget` 기반으로 판단된다.
- 신규 `/Game/UI/Lobby/Loadout/WBP_Loadout`은 `/Script/OutBreak` 의존성이 없어 현재 상태로는 `UOBLoadoutWidget` 기반이라고 볼 수 없다.
- 따라서 신규 에셋을 기존 로직에 연결하려면 구현 전 에디터에서 부모 클래스를 반드시 확인하고, 기존 C++ 계약을 쓰려면 `UOBLoadoutWidget` 기반이어야 한다.

후보 신규 위젯 이름의 존재 여부:

신규 에셋 문자열 테이블에서 다음 후보 이름들이 확인되었다. 단, 타입과 Is Variable은 미확정이다.

- `BTN_CategoryPrimary`, `BTN_CategorySecondary`, `BTN_CategoryMelee`
- `BTN_PrimaryLoadoutCard`, `BTN_SecondaryLoadoutCard`, `BTN_MeleeLoadoutCard`
- `IMG_PrimaryWeaponPreview`, `TXT_PrimaryWeaponName`, `TXT_PrimaryWeaponType`, `TXT_PrimaryWeaponDesc`, `IMG_PrimaryWeaponClassIcon`
- `IMG_SecondaryWeaponPreview`, `TXT_SecondaryWeaponName`, `TXT_SecondaryWeaponType`, `TXT_SecondaryWeaponDesc`, `IMG_SecondaryWeaponClassIcon`
- `IMG_MeleeWeaponPreview`, `TXT_MeleeWeaponName`, `TXT_MeleeWeaponType`, `TXT_MeleeWeaponDesc`, `IMG_MeleeWeaponClassIcon`
- `TXT_SelectedWeaponName`, `PB_Damage`, `PB_FireRate`, `PB_Accuracy`, `PB_Recoil`, `PB_Mobility`, `IMG_AmmoPreview`, `TXT_AmmoCount`

## 6. 기존 구조와 신규 프론트 차이

가장 중요한 차이:

1. 신규 에셋은 현재 기존 `WBP_Lobby`에서 참조되지 않는다. 기존 UI는 `/Game/UI/Lobby/WBP_Loadout`을 사용한다.
2. 신규 에셋은 `/Script/OutBreak` 의존성이 없어 기존 `UOBLoadoutWidget` 부모/계약이 적용된 상태로 확인되지 않는다.
3. 신규 위젯 이름은 기존 `BindWidget` 이름과 다르다. 예: 기존 `IconPrimary` 대 신규 `IMG_PrimaryWeaponPreview`.
4. 기존 `UOBLoadoutWidget`에는 버튼 입력 계약이 없다. 신규 `BTN_PrimaryLoadoutCard`, `BTN_CategoryPrimary` 등은 기존 C++가 처리하지 않는다.
5. 기존 무기 목록/선택은 별도 `UOBWeaponSelectWidget`과 `UOBWeaponEntryWidget`이 담당한다. 신규 `WBP_Loadout`이 카테고리 버튼과 카드 선택까지 포함한다면 기존 구조와 역할이 합쳐진 상태다.
6. 신규 `TXT_*WeaponType`, `TXT_*WeaponDesc`, `IMG_*ClassIcon`, `IMG_AmmoPreview`는 기존 `UOBLoadoutWidget`이 갱신하지 않는다.

## 7. 전체 바인딩 호환성 매트릭스

| 기능 | 기존 로직 변수 | 기존 타입 | 신규 위젯 후보 | 신규 타입 후보 | 현재 호환 여부 | 프론트 수정안 | 로직 수정 필요 여부 | 위험도 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 주무기 이름 | `NamePrimary` | `UTextBlock` | `TXT_PrimaryWeaponName` | `UTextBlock` 추정 | 이름 불일치 | 신규 위젯명을 `NamePrimary`로 변경하거나 `NamePrimary` 래퍼 TextBlock 추가 | 프론트만 가능 | 중 |
| 주무기 이미지 | `IconPrimary` | `UImage` | `IMG_PrimaryWeaponPreview` | `UImage` 추정 | 이름 불일치 | `IconPrimary` 이름 제공 | 프론트만 가능 | 중 |
| 보조무기 이름 | `NameSecondary` | `UTextBlock` | `TXT_SecondaryWeaponName` | `UTextBlock` 추정 | 이름 불일치 | `NameSecondary` 이름 제공 | 프론트만 가능 | 중 |
| 보조무기 이미지 | `IconSecondary` | `UImage` | `IMG_SecondaryWeaponPreview` | `UImage` 추정 | 이름 불일치 | `IconSecondary` 이름 제공 | 프론트만 가능 | 중 |
| 근접무기 이름 | `NameMelee` | `UTextBlock` | `TXT_MeleeWeaponName` | `UTextBlock` 추정 | 이름 불일치 | `NameMelee` 이름 제공 | 프론트만 가능 | 중 |
| 근접무기 이미지 | `IconMelee` | `UImage` | `IMG_MeleeWeaponPreview` | `UImage` 추정 | 이름 불일치 | `IconMelee` 이름 제공 | 프론트만 가능 | 중 |
| 선택 무기 이름 | `StatName` | `UTextBlock` | `TXT_SelectedWeaponName` | `UTextBlock` 추정 | 이름 불일치 | `StatName` 이름 제공 | 프론트만 가능 | 중 |
| 데미지 | `BarDamage` | `UProgressBar` | `PB_Damage` | `UProgressBar` 추정 | 이름 불일치 | `BarDamage` 이름 제공 | 프론트만 가능 | 중 |
| 연사력 | `BarFireRate` | `UProgressBar` | `PB_FireRate` | `UProgressBar` 추정 | 이름 불일치 | `BarFireRate` 이름 제공 | 프론트만 가능 | 중 |
| 정확도 | `BarAccuracy` | `UProgressBar` | `PB_Accuracy` | `UProgressBar` 추정 | 이름 불일치 | `BarAccuracy` 이름 제공 | 프론트만 가능 | 중 |
| 반동 | `BarRecoil` | `UProgressBar` | `PB_Recoil` | `UProgressBar` 추정 | 이름 불일치 | `BarRecoil` 이름 제공 | 프론트만 가능 | 중 |
| 기동성 | `BarMobility` | `UProgressBar` | `PB_Mobility` | `UProgressBar` 추정 | 이름 불일치, 값 하드코딩 | `BarMobility` 이름 제공. 값은 기존처럼 0.5 | 데이터 개선은 C++/Data 필요 | 중 |
| 탄약 | `AmmoText` | `UTextBlock` | `TXT_AmmoCount` | `UTextBlock` 추정 | 이름 불일치 | `AmmoText` 이름 제공 | 프론트만 가능 | 중 |
| 슬롯 선택 카드 | 없음 | 없음 | `BTN_*LoadoutCard` | `UButton` 추정 | 기존 계약 없음 | 장식/표시용이면 이벤트 연결하지 않음 | 클릭 기능 원하면 C++/BP 필요 | 중 |
| 카테고리 선택 | `PrimaryBox`/`SecondaryBox`/`MeleeBox`는 WeaponSelect에 있음 | `UScrollBox` | `BTN_Category*` | `UButton` 추정 | 구조 불일치 | 신규 버튼은 표시 전용으로 두거나 WeaponSelect 쪽 구조를 유지 | 실제 필터 기능 원하면 어댑터 필요 | 높음 |
| 장착 적용 | Entry 클릭 -> `OnWeaponChosen` | Delegate | 카드/버튼 후보 | `UButton` | 직접 호환 없음 | 기존 `WBP_WeaponEntry` 흐름 유지 | 새 버튼으로 장착 원하면 C++/BP 필요 | 높음 |
| 무기 목록 | `PrimaryBox`, `SecondaryBox`, `MeleeBox` | `UScrollBox` | 신규 Loadout 안 목록 후보 미확정 | 미확정 | 미확정 | 기존 `WBP_WeaponSelect` 별도 유지 권장 | 통합 UI면 어댑터 필요 | 높음 |
| 목록 항목 클릭 | `RootButton` | `UButton` | 신규 카드 버튼 | `UButton` 추정 | 의미 다름 | `WBP_WeaponEntry` 유지 | 통합 카드 클릭이면 수정 필요 | 높음 |
| 선택 체크 표시 | `CheckImage` | `UImage` | 신규 선택 Border 후보 없음 | 미확정 | 미확정 | 기존 CheckImage 유지 또는 신규 선택 표시를 `CheckImage` 호환 위젯으로 제공 | 프론트만 가능 | 중 |

## 8. 상호작용 및 데이터 흐름

현재 실제 흐름:

1. `UOBLobbyWidget::NativeConstruct()`
   - `WeaponSelect->BuildList(WeaponCatalog)` 호출
   - `WeaponSelect->OnWeaponChosen`를 `HandleWeaponChosen()`에 연결
   - 0.3초 타이머로 `RefreshDynamic()` 실행

2. `UOBWeaponSelectWidget::BuildList()`
   - `UOBWeaponCatalog::AvailableWeapons` 순회
   - `EntryWidgetClass`로 `UOBWeaponEntryWidget` 생성
   - `Entry->Setup(W)` 호출
   - `PrimaryBox`, `SecondaryBox`, `MeleeBox` 중 슬롯에 맞는 ScrollBox에 추가

3. `UOBWeaponEntryWidget::Setup()`
   - 무기 CDO에서 `UOBWeaponData` 획득
   - `NameText`에 `DisplayName` 설정
   - `IconImage`에 `WeaponIcon` 설정
   - `CheckImage` 숨김

4. 사용자 무기 항목 클릭
   - `RootButton->OnClicked`
   - `UOBWeaponEntryWidget::HandleClicked()`
   - `OnEntryClicked.Broadcast(WeaponClass, SlotType)`
   - `UOBWeaponSelectWidget::HandleEntryClicked()`
   - `OnWeaponChosen.Broadcast(WeaponClass, WeaponSlot)`

5. `UOBLobbyWidget::HandleWeaponChosen()`
   - `UOBLoadoutSubsystem::SetWeapon(WeaponSlot, WeaponClass)`로 로컬 저장
   - `AOBPlayerController::Server_SetWeaponSlot()`로 서버 PlayerState 반영
   - `Loadout->ShowStats(WeaponClass)`로 상세 표시

6. `AOBPlayerStateBase::SetWeaponForSlot()`
   - 같은 슬롯의 기존 `SelectedWeapons` 제거
   - 새 무기 추가
   - `OnLobbyStateChanged.Broadcast()`

7. `UOBLobbyWidget::RefreshDynamic()`
   - `WeaponSelect->RefreshChecks(PS->GetSelectedWeapons())`
   - `Loadout->RefreshLoadout(PS->GetSelectedWeapons())`

8. Expedition 진입 또는 Pawn possession
   - `AOBPlayerController::BeginPlay()`가 GameInstance Loadout을 `Server_ApplyLoadout()`로 서버에 push
   - `AOBCharacterBase::PossessedBy()`가 `SelectedWeapons`를 Inventory에 추가
   - `UOBInventoryComponent::EquipDefaultSlot()`가 실제 장착 시작

상태 구분:

| 상태 | 현재 소유 객체 | 현재 코드상 의미 |
| --- | --- | --- |
| 현재 카테고리 | 명시 상태 없음 | 기존 구조는 3개 ScrollBox를 모두 구성함. 카테고리 버튼 필터 상태 없음 |
| 현재 미리보기 중인 무기 | 명시 상태 없음 | Entry 클릭 시 곧바로 선택/저장됨. hover preview 없음 |
| 현재 선택된 무기 | `AOBPlayerStateBase::SelectedWeapons`, `UOBLoadoutSubsystem::CurrentLoadout` | 로비 선택 결과 |
| 실제로 장착된 무기 | `UOBInventoryComponent::ActiveSlot`, `UOBEquipmentComponent::CurrentWeapon` | Expedition/게임플레이 장착 상태 |
| 장착 요청 중인 무기 | `UOBInventoryComponent::bSwitching` | 슬롯 교체 중 차단 상태 |
| 실패 후 복원 상태 | 별도 없음 | 현재 로비 선택 실패 UI 없음. 서버 RPC 실패 처리도 UI에 연결되어 있지 않음 |

## 9. 프론트 수정 전략 비교

### 전략 A: 신규 프론트의 위젯 이름과 타입을 기존 계약에 맞춤

내용:

- 신규 프론트가 기존 `UOBLoadoutWidget`의 `BindWidget` 이름과 타입을 그대로 제공한다.
- 예: `TXT_PrimaryWeaponName`을 `NamePrimary`로 변경하거나, 디자인 내부에 `NamePrimary`라는 `UTextBlock`을 별도로 둔다.
- `PB_Damage`는 `BarDamage`, `TXT_AmmoCount`는 `AmmoText` 등으로 맞춘다.

평가:

- 수정 범위: 중간. 이름/Is Variable/부모 클래스/소유 관계 수정 필요
- 디자인 보존: 이름 변경만이면 대부분 보존 가능
- Graph 참조 파손 가능성: 신규 에셋 내부 Graph가 위젯 이름을 참조한다면 이름 변경 시 파손 가능. 현재 Graph는 추출 불가
- 기존 로직 무수정 가능성: 기본 슬롯/상세 표시만은 가능
- 장기 유지보수성: 기존 C++ 계약을 명확히 따르므로 가장 단순

제약:

- 신규 에셋이 현재 `UOBLoadoutWidget` 기반인지 확인되지 않았다. `/Script/OutBreak` 의존성이 없으므로 현재 상태 그대로는 불가하다.
- 신규 `/Game/UI/Lobby/Loadout/WBP_Loadout`은 `WBP_Lobby`가 참조하지 않는다. 참조 교체 또는 기존 경로에 디자인 반영이 필요하다.

### 전략 B: 신규 프론트 이름을 유지하고 C++ 바인딩 계층만 조정

내용:

- `UOBLoadoutWidget`의 `BindWidget` 멤버를 `TXT_PrimaryWeaponName`, `IMG_PrimaryWeaponPreview`, `PB_Damage` 등으로 변경한다.

평가:

- 수정 범위: C++ 변경 필요
- 디자인 보존: 좋음
- 기존 로직 무수정 가능성: 낮음
- 유지보수성: UI 이름이 C++에 다시 강결합됨

권장도:

- 현재 단계에서는 낮다. 기존 계약이 단순하고 이름 변경/래퍼로 해결 가능한 표시 위젯이 많다.

### 전략 C: 프론트 어댑터 또는 Presenter 계층 추가

내용:

- 기존 Loadout 데이터와 신규 복합 UI 사이에 표시용 ViewData/Presenter를 둔다.
- 카테고리 버튼, 카드 선택, 상세 표시, 설명/타입/클래스 아이콘을 신규 구조에 분배한다.

평가:

- 수정 범위: 높음
- 디자인 보존: 높음
- 기존 로직 무수정 가능성: 일부 유지 가능하나 연결 계층은 새로 필요
- 유지보수성: 향후 UI 교체 가능성이 높다면 좋음

권장 조건:

- 신규 `WBP_Loadout`이 기존 `WBP_WeaponSelect`까지 대체하는 단일 화면이 되어야 하는 경우
- 카테고리 필터와 카드 클릭을 실제 상호작용으로 구현해야 하는 경우
- 추가 표시 데이터가 `UOBWeaponData`에 없는 경우

### 전략 D: 구형 이름을 가진 호환용 래퍼 위젯 추가

내용:

- 신규 디자인 위젯 이름을 유지하면서, 기존 이름의 숨김/투명/동일 위치 래퍼 위젯을 추가한다.
- 예: `TXT_PrimaryWeaponName` 주변에 기존 이름 `NamePrimary`를 가진 실제 TextBlock을 두거나, 반대로 기존 이름을 데이터 출력 위젯에 부여한다.

평가:

- 수정 범위: 중간
- 디자인 보존: 높음
- 기존 로직 무수정 가능성: 높음
- 유지보수성: 임시 호환 패치로는 유효하지만 장기적으로 중복 위젯 관리 위험

권장도:

- 위젯 이름 변경이 신규 Graph/디자인 참조를 깨는 경우 임시 호환책으로 권장한다.

## 10. 권장 방법론

권장안: 전략 A를 기본으로 하되, 이름 변경이 신규 Graph를 깨는 위젯은 전략 D 래퍼를 제한적으로 사용한다.

구현 승인 시 우선순위:

1. 신규 디자인을 실제 사용 경로에 연결한다.
   - 기존 `WBP_Lobby`의 `Loadout` 하위 위젯은 현재 `/Game/UI/Lobby/WBP_Loadout`을 참조한다.
   - 신규 `/Game/UI/Lobby/Loadout/WBP_Loadout`을 쓰려면 `WBP_Lobby`의 `Loadout` 자식 위젯 클래스가 신규 에셋을 참조해야 한다.
   - 또는 기존 `/Game/UI/Lobby/WBP_Loadout` 안에 신규 디자인을 반영해 기존 참조 경로를 유지한다.

2. 실제 Loadout 표시 위젯은 `UOBLoadoutWidget` 기반이어야 한다.
   - 현재 신규 에셋은 `/Script/OutBreak` 의존성이 없어 `UOBLoadoutWidget` 기반으로 확인되지 않는다.
   - 기존 C++를 무수정으로 쓰려면 부모 클래스/생성 클래스가 `UOBLoadoutWidget` 계약을 가져야 한다.

3. 기존 `BindWidget` 이름과 타입을 제공한다.
   - `IconPrimary`, `NamePrimary`, `IconSecondary`, `NameSecondary`, `IconMelee`, `NameMelee`, `StatName`, `BarDamage`, `BarFireRate`, `BarAccuracy`, `BarRecoil`, `BarMobility`, `AmmoText`
   - 모두 `Is Variable` 활성화 필요

4. 신규 장식/추가 정보 위젯은 기존 장착 로직에 억지로 연결하지 않는다.
   - `TXT_*WeaponType`, `TXT_*WeaponDesc`, `IMG_*ClassIcon`, `IMG_AmmoPreview`는 현재 `UOBLoadoutWidget`이 갱신하지 않는다.
   - 필요한 경우 별도 ViewData 또는 소규모 표시 함수 추가로 분리한다.

5. 카테고리 버튼과 카드 클릭은 기존 선택/장착 흐름과 혼동하지 않는다.
   - 기존 로비 선택은 `WBP_WeaponEntry.RootButton` 클릭으로 처리된다.
   - 실제 장착은 게임플레이 Inventory/Equipment에서 처리된다.
   - `BTN_*LoadoutCard` 클릭을 장착으로 연결하면 로비 선택과 실제 장착 상태가 섞인다.

## 11. Widget별 정확한 수정 계획

아래 표는 구현 승인 시의 계획이다. 현재 분석에서는 수행하지 않았다.

| 현재 Widget | 수행할 작업 | 변경 후 이름 | 변경 후 타입 | Is Variable | 연결 이벤트 | 갱신 데이터 | 비고 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `BTN_PrimaryLoadoutCard` | 변경 불필요 또는 선택 표시 컨테이너로 유지 | 그대로 | `UButton` 또는 카드 컨테이너 | 필요 시만 | 연결하지 않음 | 없음 | 기존 `UOBLoadoutWidget`은 슬롯 카드 클릭 처리 없음 |
| `BTN_SecondaryLoadoutCard` | 변경 불필요 | 그대로 | `UButton` 또는 카드 컨테이너 | 필요 시만 | 연결하지 않음 | 없음 | 동일 |
| `BTN_MeleeLoadoutCard` | 변경 불필요 | 그대로 | `UButton` 또는 카드 컨테이너 | 필요 시만 | 연결하지 않음 | 없음 | 동일 |
| `IMG_PrimaryWeaponPreview` | 이름 변경 또는 래퍼 연결 | `IconPrimary` | `UImage` | Enabled | 없음 | `WeaponIcon` | 기존 C++가 `SetBrushFromTexture()` 호출 |
| `TXT_PrimaryWeaponName` | 이름 변경 또는 래퍼 연결 | `NamePrimary` | `UTextBlock` | Enabled | 없음 | `DisplayName` | 빈 슬롯은 `"미선택"` |
| `IMG_SecondaryWeaponPreview` | 이름 변경 또는 래퍼 연결 | `IconSecondary` | `UImage` | Enabled | 없음 | `WeaponIcon` |  |
| `TXT_SecondaryWeaponName` | 이름 변경 또는 래퍼 연결 | `NameSecondary` | `UTextBlock` | Enabled | 없음 | `DisplayName` |  |
| `IMG_MeleeWeaponPreview` | 이름 변경 또는 래퍼 연결 | `IconMelee` | `UImage` | Enabled | 없음 | `WeaponIcon` | 근접도 `WeaponIcon` 사용 가능 |
| `TXT_MeleeWeaponName` | 이름 변경 또는 래퍼 연결 | `NameMelee` | `UTextBlock` | Enabled | 없음 | `DisplayName` |  |
| `TXT_SelectedWeaponName` | 이름 변경 또는 래퍼 연결 | `StatName` | `UTextBlock` | Enabled | 없음 | `DisplayName` | `ShowStats()` 시 갱신 |
| `PB_Damage` | 이름 변경 또는 래퍼 연결 | `BarDamage` | `UProgressBar` | Enabled | 없음 | `BaseDamage` | `MaxDamage` 정규화 |
| `PB_FireRate` | 이름 변경 또는 래퍼 연결 | `BarFireRate` | `UProgressBar` | Enabled | 없음 | `RoundsPerMinute` |  |
| `PB_Accuracy` | 이름 변경 또는 래퍼 연결 | `BarAccuracy` | `UProgressBar` | Enabled | 없음 | `BaseSpreadDegrees` 역정규화 |  |
| `PB_Recoil` | 이름 변경 또는 래퍼 연결 | `BarRecoil` | `UProgressBar` | Enabled | 없음 | 수직+수평 반동 |  |
| `PB_Mobility` | 이름 변경 또는 래퍼 연결 | `BarMobility` | `UProgressBar` | Enabled | 없음 | 현재 0.5 고정 | 데이터 필드 없음 |
| `TXT_AmmoCount` | 이름 변경 또는 래퍼 연결 | `AmmoText` | `UTextBlock` | Enabled | 없음 | `MagazineSize / MaxReserveAmmo` | 근접무기 기본 처리 필요 |
| `IMG_AmmoPreview` | 변경 불필요 또는 표시 전용 유지 | 그대로 | `UImage` | 필요 시 | 없음 | 없음 | 기존 데이터에 전용 Ammo preview 없음 |
| `BTN_CategoryPrimary` | 표시 전용 또는 별도 어댑터 | 그대로 | `UButton` | 필요 시 | 기존 로직에는 연결하지 않음 | 없음 | 실제 필터 기능은 C++/BP 추가 필요 |
| `BTN_CategorySecondary` | 표시 전용 또는 별도 어댑터 | 그대로 | `UButton` | 필요 시 | 기존 로직에는 연결하지 않음 | 없음 |  |
| `BTN_CategoryMelee` | 표시 전용 또는 별도 어댑터 | 그대로 | `UButton` | 필요 시 | 기존 로직에는 연결하지 않음 | 없음 |  |
| `TXT_PrimaryWeaponType` | 새 데이터 출력 | 그대로 | `UTextBlock` | Enabled if updated | 없음 | `WeaponCategory` 또는 `WeaponType` | 기존 C++ 갱신 없음 |
| `TXT_PrimaryWeaponDesc` | 기본 텍스트 또는 새 데이터 출력 | 그대로 | `UTextBlock` | Enabled if updated | 없음 | 데이터 없음 | `UOBWeaponData`에 description 필드 없음 |
| `IMG_PrimaryWeaponClassIcon` | 표시 전용 | 그대로 | `UImage` | 필요 시 | 없음 | 데이터 없음 | 카테고리별 아이콘 데이터 없음 |
| `TXT_SecondaryWeaponType` | 새 데이터 출력 | 그대로 | `UTextBlock` | Enabled if updated | 없음 | `WeaponCategory` 또는 `WeaponType` |  |
| `TXT_SecondaryWeaponDesc` | 기본 텍스트 또는 새 데이터 출력 | 그대로 | `UTextBlock` | Enabled if updated | 없음 | 데이터 없음 |  |
| `IMG_SecondaryWeaponClassIcon` | 표시 전용 | 그대로 | `UImage` | 필요 시 | 없음 | 데이터 없음 |  |
| `TXT_MeleeWeaponType` | 새 데이터 출력 | 그대로 | `UTextBlock` | Enabled if updated | 없음 | `WeaponType`/`WeaponCategory` |  |
| `TXT_MeleeWeaponDesc` | 기본 텍스트 또는 새 데이터 출력 | 그대로 | `UTextBlock` | Enabled if updated | 없음 | 데이터 없음 |  |
| `IMG_MeleeWeaponClassIcon` | 표시 전용 | 그대로 | `UImage` | 필요 시 | 없음 | 데이터 없음 |  |

## 12. 최소 C++ 수정 필요 여부

프론트만으로 해결 가능한 항목:

- 기존 `UOBLoadoutWidget`이 갱신하는 이름, 아이콘, 상세 이름, 능력치 바, 탄약 텍스트
- 기존 이름과 타입을 신규 디자인에 제공하는 작업
- 표시 전용 신규 위젯을 기본 텍스트/장식 상태로 유지하는 작업

프론트 수정과 소규모 C++ 수정이 함께 필요한 항목:

- 신규 이름을 유지하면서 `UOBLoadoutWidget` 바인딩 멤버를 `TXT_*`, `IMG_*`, `PB_*`로 바꾸는 경우
- `TXT_*WeaponType`을 `WeaponType`/`WeaponCategory`에서 채우는 경우
- 근접무기 탄약 표시를 `N/A`, `-`, 빈 텍스트 등으로 분기하는 경우
- `BTN_Category*`를 실제 필터 버튼으로 사용하는 경우

어댑터 계층이 필요한 항목:

- 신규 `WBP_Loadout`이 기존 `WBP_WeaponSelect`까지 통합해 무기 목록, 카테고리, 장착 슬롯 카드, 상세 패널을 모두 하나의 복합 위젯에서 처리하려는 경우
- 선택 상태, 미리보기 상태, 저장된 로드아웃 상태를 분리해야 하는 경우
- 하나의 데이터가 여러 신규 위젯에 분배되고 기존 C++가 그 구조를 알지 못하는 경우

기존 로직 자체의 결함 또는 미구현:

- 로비 UI 갱신이 `OnLobbyStateChanged` 직접 구독이 아니라 0.3초 타이머 기반이다.
- `BarMobility`는 실제 데이터가 아니라 0.5로 고정된다.
- `WeaponDescription`, 카테고리 아이콘, 탄약 프리뷰 이미지 데이터가 `UOBWeaponData`에 없다.
- 장착 실패/서버 RPC 실패를 UI로 복원하는 흐름이 없다.
- 로비 선택과 실제 장착 상태가 별도의 시스템인데, 프론트가 이를 명확히 표시하지 않으면 상태 혼동이 생긴다.

## 13. 위험 요소

1. 신규 에셋이 사용 경로에 연결되어 있지 않음
   - 참조자가 없으므로 현재 런타임 UI에 등장하지 않는다.

2. 부모 클래스 불일치 가능성
   - 신규 에셋은 `/Script/OutBreak` 의존성이 없어 `UOBLoadoutWidget` 기반으로 확인되지 않는다.
   - 기존 C++ 계약은 부모가 `UOBLoadoutWidget`일 때만 의미가 있다.

3. 이름 불일치
   - 기존 `BindWidget`은 `IconPrimary`, `NamePrimary`, `BarDamage` 등을 요구한다.
   - 신규 이름은 `IMG_PrimaryWeaponPreview`, `TXT_PrimaryWeaponName`, `PB_Damage` 형태다.

4. 타입 불일치 가능성
   - 상세 Widget Tree를 추출하지 못했으므로 `PB_Damage`가 실제 `UProgressBar`인지, `BTN_*`가 실제 `UButton`인지 구현 전 검증이 필요하다.

5. `Is Variable` 미확정
   - 이름과 타입이 맞아도 `Is Variable`이 꺼져 있으면 `BindWidget` 바인딩이 실패한다.

6. 신규 버튼 이벤트가 기존 흐름에 없음
   - 카테고리 버튼과 슬롯 카드 버튼은 현재 기존 C++에서 처리하지 않는다.

7. 신규 표시 데이터 부족
   - 설명, 클래스 아이콘, 탄약 프리뷰 이미지에 대응하는 데이터 필드가 현재 확인되지 않았다.

8. 기존 타이머 갱신 방식
   - 로비 상태 변경 이벤트가 존재하지만 UI가 직접 구독하지 않고 타이머로 갱신한다. 기능은 동작할 수 있으나 장기적으로 중복/지연 갱신 위험이 있다.

## 14. 롤백 계획

구현 승인 후 수정 시 권장 롤백 절차:

1. `WBP_Loadout` 원본 백업 또는 Source Control 상태 확인
2. 기존 `/Game/UI/Lobby/WBP_Loadout`, 신규 `/Game/UI/Lobby/Loadout/WBP_Loadout`, `/Game/UI/Lobby/WBP_Lobby` 참조 목록 저장
3. 부모 클래스, Widget Tree, Graph 스크린샷 또는 JSON 덤프 저장
4. 위젯 이름 변경 전 Graph/Designer 참조 검색
5. 프론트 변경
6. Blueprint Compile
7. `BindWidget` 오류 확인
8. 저장 후 에디터 재실행
9. PIE 테스트
10. Reference Viewer로 참조 손상 확인

롤백 기준:

- Blueprint Compile에서 `BindWidget` 누락 또는 타입 오류 발생
- `WBP_Lobby`의 `Loadout` 바인딩이 null
- 무기 선택 시 `ShowStats()` 또는 `RefreshLoadout()`이 호출되지 않음
- 기존 `/Game/UI/Lobby/WBP_Loadout` 참조가 깨짐

## 15. 검증 및 테스트 계획

필수 검증:

- 신규 또는 갱신 대상 `WBP_Loadout` Blueprint Compile 성공
- 부모 클래스가 `UOBLoadoutWidget`인지 확인
- `IconPrimary`, `NamePrimary`, `IconSecondary`, `NameSecondary`, `IconMelee`, `NameMelee`, `StatName`, `BarDamage`, `BarFireRate`, `BarAccuracy`, `BarRecoil`, `BarMobility`, `AmmoText`가 정확한 타입으로 존재
- 모든 필수 바인딩의 `Is Variable` 활성화
- `WBP_Lobby`의 `Loadout` 자식 위젯이 `UOBLoadoutWidget` 타입으로 바인딩
- `WBP_Lobby`의 `WeaponSelect` 자식 위젯이 `UOBWeaponSelectWidget` 타입으로 바인딩
- `WBP_WeaponSelect`의 `PrimaryBox`, `SecondaryBox`, `MeleeBox`가 `UScrollBox`
- `WBP_WeaponEntry`의 `RootButton`, `IconImage`, `NameText`, `CheckImage`가 정확한 타입
- `NativeConstruct()` 중 null 접근 없음
- 주무기/보조무기/근접무기 이름 표시
- 주무기/보조무기/근접무기 이미지 갱신
- 상세 데미지/연사력/정확도/반동/기동성 갱신
- 탄약 텍스트 갱신
- 근접무기 탄약 표시 기본 상태
- 빈 슬롯 `"미선택"` 표시
- 무기 항목 클릭 시 로컬 SaveGame 저장 및 서버 PlayerState 반영
- `RefreshChecks()` 선택 체크 표시
- `RefreshLoadout()` 슬롯 카드 갱신
- 위젯 재오픈 시 중복 이벤트 바인딩 없음
- Lobby 복귀 후 `UOBLoadoutSubsystem::LoadFromDisk()` 결과 복원
- 해상도/DPI 변경 후 기능 유지
- 키보드/마우스/게임패드 포커스 확인
- 장식 이미지가 입력을 가로채지 않음

멀티플레이/서버 권위 검증:

- 클라이언트 선택 후 `Server_SetWeaponSlot()`가 서버 PlayerState를 갱신
- `SelectedWeapons` 복제 후 클라이언트 `RefreshDynamic()` 결과 확인
- Expedition 진입 시 `Server_ApplyLoadout()`가 GameInstance 저장 선택을 서버에 push
- `AOBCharacterBase::PossessedBy()`에서 Inventory에 선택 무기 추가
- `UOBInventoryComponent::EquipDefaultSlot()`가 실제 장착 시작
- 실제 장착 상태와 로비 선택 상태가 UI에서 혼동되지 않음

## 16. 구현 순서

권장 구현 순서:

1. Source Control 상태 확인
2. 기존 `/Game/UI/Lobby/WBP_Loadout`과 신규 `/Game/UI/Lobby/Loadout/WBP_Loadout`의 목적 결정
   - 기존 경로 유지형: 기존 에셋에 신규 디자인 반영
   - 신규 경로 사용형: `WBP_Lobby.Loadout` 참조를 신규 에셋으로 변경
3. 신규 디자인을 사용할 에셋의 부모 클래스가 `UOBLoadoutWidget`인지 확인
4. 기존 `BindWidget` 계약 이름/타입/Is Variable 제공
5. 신규 표시 전용 Widget은 기존 로직과 분리
6. Blueprint Compile
7. `WBP_Lobby`에서 `Loadout` 바인딩 확인
8. 로비에서 WeaponSelect 목록 생성 확인
9. 무기 항목 클릭 및 상세 표시 확인
10. 저장/복제/재오픈 확인
11. 추가 기능이 필요할 때만 별도 Presenter 또는 소규모 C++ 표시 함수 추가

구현하지 말아야 할 순서:

- 신규 버튼을 곧바로 실제 장착 요청에 연결
- 기존 `UOBLoadoutSubsystem` 저장 모델을 UI 편의를 위해 변경
- 설명/아이콘 데이터가 없는데 임의 데이터 필드를 가정
- 모든 신규 위젯을 `BindWidgetOptional`로 바꿔 오류를 숨김

## 17. 최종 결론

현재 신규 `/Game/UI/Lobby/Loadout/WBP_Loadout`은 기존 Loadout 런타임 흐름에 바로 호환되지 않는다.

가장 큰 이유는 다음 세 가지다.

1. 기존 `WBP_Lobby`가 신규 에셋을 참조하지 않는다.
2. 신규 에셋은 `/Script/OutBreak` 의존성이 없어 `UOBLoadoutWidget` 기반으로 확인되지 않는다.
3. 신규 위젯 이름은 기존 `BindWidget` 계약과 다르다.

기존 C++ 로직을 유지하려면 먼저 실제 사용될 Loadout Widget이 `UOBLoadoutWidget` 기반인지 확인하고, 기존 필수 위젯 이름과 타입을 프론트에서 제공해야 한다. 기본 슬롯 표시와 상세 능력치 표시만 목표라면 C++ 수정 없이 프론트 수정으로 해결할 수 있다. 반대로 신규 카테고리 버튼, 카드 클릭, 설명/클래스 아이콘, 탄약 프리뷰까지 실제 기능으로 쓰려면 기존 `WBP_WeaponSelect` 구조와 충돌하므로 Presenter/어댑터 또는 소규모 C++ 표시 계층을 별도로 설계해야 한다.

이번 분석 단계에서는 에셋, 소스, Config를 수정하지 않았다.
