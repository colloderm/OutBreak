# 인벤토리 시스템 통합 개발 진행 보고서

- 작성일: 2026-08-04
- 기준 브랜치: `ZombieSystem`
- 분석 기준 커밋: `68152840` (`Merge branch 'KJH' into ZombieSystem`)
- 구현 기준일: 2026-08-04
- 대상 모듈: 플레이어 인벤토리, 장비/무기, 월드 아이템, 루팅 컨테이너, 소비 아이템 HUD
- 문서 목적: 중복 컴포넌트·자료형·저장소·API를 하나의 런타임 인벤토리 모델로 통합하기 위한 구현 기준과 진행 순서를 확정한다.

## 1. 결론

최종 플레이어 인벤토리 소유자는 `UPlayerInventoryComponent` 하나로 통일한다.

`UOBInventoryComponent`가 보유한 무기 슬롯, 탄약 풀, 소모품 카운트는 모두 신형 컴포넌트와 중복되므로 소비자를 먼저 이관한 뒤 클래스와 관련 자료형을 삭제한다. `UOBEquipmentComponent`는 인벤토리 저장소가 아니라 실제 무기/장비 액터의 생성·부착·해제 담당으로 남긴다.

아이템 데이터는 다음 두 계층만 허용한다.

1. 정적 정의: `FGameplayTag -> FOBItemDefinitionRow`
2. 런타임 상태: 태그, 수량, 인스턴스 GUID, 탄창 상태를 가진 인벤토리 아이템 인스턴스

`FName`, Item DataAsset 포인터, 별도 메타데이터 테이블, 카테고리 복사본은 다시 도입하지 않는다.

외부 루팅 보관함은 `AOBLootContainer`가 직접 소유하고, 플레이어 컴포넌트의 `InventoryContrainerArray`는 제거한다. 루팅 UI는 플레이어 인벤토리와 월드 컨테이너를 동시에 표시하되, 서버 RPC를 통해 두 소유자 사이에서 아이템을 원자적으로 이관한다.

## 2. 현재 진행 상태

| 영역 | 상태 | 현재 결과 | 남은 작업 |
|---|---|---|---|
| 아이템 식별자 | 완료 | `FGameplayTag`로 통일 | Blueprint와 DataTable 유효성 검사 자동화 |
| 정적 아이템 정의 | 대부분 완료 | `DT_Items`의 `FOBItemDefinitionRow` 사용 | 원본 CSV 버전 관리, Reimport 시 캐시 갱신 |
| 신형 슬롯 인벤토리 | 구현 완료·검증 필요 | 가방, 장비, 퀵슬롯, 탄창, 드랍/재획득, 서버 권한 복제 | 멀티플레이 회귀 테스트 |
| 구형 인벤토리 이관 | 부분 완료 | 무기·탄약·소모 어빌리티 일부가 신형 사용 | HUD와 사망 루팅 이관, 구형 컴포넌트 삭제 |
| 월드 아이템 | 부분 완료 | GUID·탄창·배낭 내용물 보존 코드 존재 | 공통 상호작용 연결, 중복 자료형 제거 |
| 루팅 컨테이너 | 부분 완료 | 서버 추첨과 `Contents` 복제 구현 | 가져가기/전체 가져가기 RPC와 UI 연결 |
| 플레이어 사망 루팅 | 미완료 | 컨테이너 스폰은 구현 | 신형 인벤토리에서 실제 인스턴스 추출 및 원본 차감 |
| 문서/데이터 소스 | 미완료 | 이전 설계 문서와 현재 코드가 불일치 | 본 문서를 기준 문서로 교체, CSV/JSON 추가 |

## 3. 현재 중복 구조와 문제

### 3.1 플레이어 인벤토리 컴포넌트 중복

`AOBCharacterBase`는 다음 두 컴포넌트를 동시에 생성한다.

- `UOBInventoryComponent`: 구형 무기 슬롯, 별도 탄약 풀, 태그별 소모품 카운트
- `UPlayerInventoryComponent`: 슬롯 기반 가방, 장비 슬롯, 퀵슬롯, 탄약 아이템, 인스턴스 GUID, 탄창 보존

시작 소모품은 두 컴포넌트에 동시에 지급된다. 실제 소비 어빌리티는 신형 컴포넌트에서 차감하지만 소비품 HUD는 구형 컴포넌트를 읽으므로, 사용 후 HUD 수량과 실제 수량이 달라질 수 있다.

시작 무기는 신형 컴포넌트에만 지급되지만 `DropCorpseLoot()`는 구형 컴포넌트의 무기 슬롯을 읽는다. 현재 흐름에서는 플레이어 시체에 시작 무기가 들어가지 않을 가능성이 높다.

### 3.2 자료형 중복

| 현재 자료형/필드 | 중복 대상 | 판단 |
|---|---|---|
| `FOBCountEntry` | `FOBItemStack`, 신형 슬롯의 태그/수량 | 구형 컴포넌트와 함께 삭제 |
| `FOBWeaponSlotEntry` | `FEquipmentSlotEntry` + `FInventoryData.MagazineAmmo` | 구형 컴포넌트와 함께 삭제 |
| `FWorldItemData` | `FOBItemStack` 및 `FInventoryData`의 태그/수량 | 삭제하고 월드 아이템을 런타임 인스턴스 하나로 통일 |
| `EItemType` / `FInventoryData.ItemType` | `FOBItemDefinitionRow.Category` | 저장 필드와 변환 함수 삭제, 필요 시 정의 행에서 계산 |
| `FInventoryQueryResult` | `GetItemCount`, `ConsumeItem` 내부 순회 | 외부 사용처가 없으므로 삭제하고 원자적 수량 API로 대체 |
| `FInventoryItemHandle.ItemTag` | 실제 슬롯의 `ItemTag` | 일반 슬롯에서는 검증 힌트일 뿐이며 퀵슬롯에만 필요. 위치별 검증 규칙을 명시하고 저장 원본으로 사용하지 않음 |
| `InventoryContrainerArray` | `AOBLootContainer.Contents` | 플레이어 소유 배열 제거, 외부 컨테이너가 직접 소유 |

### 3.3 기능/API 중복

신형 컴포넌트에도 다음과 같은 중복 API가 있다.

- `AddItemByTag(Tag, int32&) -> bool`
- `AddItem(Tag, Amount) -> void`
- `ConsumeItem(FInventoryQueryResult, Amount)`
- `ConsumeItem(Tag, Amount) -> int32`

호출자가 성공 여부, 실제 추가량, 남은 수량을 서로 다른 규칙으로 해석해야 하므로 하나의 계약으로 합쳐야 한다.

권장 공개 API는 다음 세 개다.

```cpp
int32 GetItemCount(const FGameplayTag& ItemTag) const;
int32 TryAddItem(const FGameplayTag& ItemTag, int32 RequestedAmount);
int32 TryRemoveItem(const FGameplayTag& ItemTag, int32 RequestedAmount);
```

반환값은 항상 실제 추가/제거된 수량이다. 부분 성공도 같은 규칙으로 표현할 수 있고, `RequestedAmount == ReturnValue`로 전체 성공을 판정한다.

GUID·탄창·배낭 내용물이 있는 드랍 아이템에는 수량 API와 분리된 다음 계약을 사용한다.

```cpp
int32 TryAddItemInstance(const FInventoryData& ItemInstance);
bool TryExtractItemInstance(const FGuid& InstanceId, FInventoryData& OutItemInstance);
```

정적 정의 행을 받는 함수는 private 구현으로 제한한다. Blueprint나 다른 시스템이 DataTable 행 포인터를 보관하지 못하게 한다.

## 4. 통합 후 책임 구조

### 4.1 `UPlayerInventoryComponent` — 플레이어 아이템의 유일한 소유자

다음 상태와 규칙을 소유한다.

- 가방 슬롯과 용량
- 장비 슬롯에 들어간 아이템 인스턴스
- 퀵슬롯의 아이템 태그 지정
- 아이템 추가, 제거, 이동, 스왑
- 탄약 수량과 무기별 탄창 상태
- 드랍/획득 시 GUID와 런타임 상태 보존
- 소유자 전용 복제와 모든 변경의 서버 권한 검증
- 사망 시 루팅 가능한 아이템을 추출하는 API

구형 `UOBInventoryComponent`의 데이터나 이벤트를 미러링하지 않는다.

### 4.2 `UOBEquipmentComponent` — 장착 결과 실행 담당

다음 역할만 유지한다.

- 선택된 무기/장비 액터 스폰
- 캐릭터 소켓 부착과 해제
- 장착 외형 및 패시브 효과 적용
- 현재 실제 장착 액터 복제

보유 여부, 수량, 탄약 풀, 탄창 원본은 저장하지 않는다. 장착 요청의 진실 원본은 `UPlayerInventoryComponent`다.

### 4.3 `UOBItemRegistry` — 정적 정의 조회 담당

- `DT_Items`와 `DT_LootTables` 참조를 한 곳에서 소유
- 태그로 `FOBItemDefinitionRow` 조회
- 무기 클래스에서 아이템 태그 역조회
- 빈 태그, 중복 태그, 잘못된 카테고리/필수 참조를 초기화 시 검증
- 에디터 DataTable Reimport 시 캐시를 무효화하고 재생성

### 4.4 `AOBLootContainer` — 외부 다중 아이템 소유자

- 상자, 적 드랍, 플레이어 시체의 내용물 소유
- 서버에서만 내용물 생성·추가·차감
- 클라이언트에는 결과만 복제
- `ServerTakeItem`, `ServerTakeAll` 요청에서 거리, 소유자, 수량, 인벤토리 여유 공간 검증
- 실제 추가된 수량만 컨테이너에서 차감
- 빈 컨테이너의 UI 종료 및 필요 시 액터 제거

### 4.5 `AWorldItem` — 단일 런타임 아이템 소유자

- `AOBInteractableActor`를 상속하거나 동일한 상호작용 인터페이스 구현
- `FWorldItemData`를 제거하고 `FInventoryData ItemInstance` 하나만 소유
- 에디터 배치 아이템은 서버 `BeginPlay`에서 GUID와 기본 런타임 상태 초기화
- 무기 탄창, 장비 인스턴스, 배낭 내용물을 드랍/재획득 사이에 보존

## 5. 최종 자료형 기준

| 자료형 | 최종 역할 | 처리 |
|---|---|---|
| `FGameplayTag` | 아이템 종류의 유일한 식별자 | 유지 |
| `FOBItemDefinitionRow` | 이름, 아이콘, 카테고리, 최대 스택, 가격, 무기/장비 연결 등 정적 정의 | 유지 |
| `FOBItemStack` | 루팅 테이블, 창고, 상점처럼 런타임 GUID가 필요 없는 `태그 + 총수량` | 유지 |
| `FInventoryData` | 플레이어 슬롯·월드 드랍·시체의 런타임 아이템 인스턴스 | 유지하되 `ItemType` 제거. 장기적으로 `FOBInventoryItemInstance`로 이름 변경 검토 |
| `FInventoryItemHandle` | UI 요청이 슬롯/장비/퀵슬롯을 지목하는 일회성 핸들 | 유지 |
| `FEquipmentSlotEntry` | 장비 위치와 해당 런타임 인스턴스 | 유지 |
| `FQuickSlotData` | 아이템 종류 지정. 실제 아이템 소유권 없음 | 유지 |
| `FOBLootEntry`, `FOBLootTableRow` | 정적 확률/수량 규칙 | 유지 |
| `FOBCountEntry` | 구형 집계 상태 | 삭제 |
| `FOBWeaponSlotEntry` | 구형 무기 상태 | 삭제 |
| `FWorldItemData` | 중복 월드 태그/수량 | 삭제 |
| `FInventoryQueryResult` | 인덱스가 포함된 임시 조회 결과 | 삭제 |
| `EItemType` | DataTable 카테고리의 복사본 | 삭제 |

자료형 이름 변경은 UHT/Blueprint 직렬화에 영향을 주므로 기능 통합과 동시에 강제하지 않는다. 먼저 필드와 책임을 통합한 뒤, 이름을 변경해야 한다면 `CoreRedirects` 추가와 전체 Blueprint Resave를 별도 커밋으로 수행한다.

## 6. 개발 단계

### 단계 0 — 기준선 고정 및 회귀 항목 확보

상태: 분석 완료, 자동 테스트 미작성

- 현재 시작 무기, 탄약, 소비품, HUD, 드랍/재획득 동작을 기록한다.
- Listen Server 1명 + Client 1명 기준 테스트 맵을 준비한다.
- `DT_Items`에 기본 배낭 행을 추가하고 `DA_PawnData_Default.DefaultBackpackTag`를 지정한다.
- `Data/Source/Items.csv`와 루팅 테이블 원본을 저장소에 추가한다.

완료 조건: 변경 전 동작과 알려진 실패 항목이 재현 가능한 체크리스트로 남는다.

### 단계 1 — 신형 컴포넌트 API 단일화

상태: 착수 전

- `TryAddItem`, `TryRemoveItem`, `TryAddItemInstance`, `TryExtractItemInstance`를 구현한다.
- 기존 `AddItemByTag`, `AddItem`, 두 종류의 `ConsumeItem`은 임시 래퍼로 바꾸고 deprecated 표시한다.
- `FInventoryQueryResult`, `QueryHasItem`, `QueryItemEnough` 의존을 제거한다.
- `EItemType` 사용을 `FOBItemDefinitionRow.Category` 조회로 교체한다.
- 행 포인터를 공개 API와 장기 상태에 저장하지 않는다.

완료 조건: 아이템 수량 변경 경로가 내부 구현 한 곳만 호출하고 기존 기능 테스트가 유지된다.

### 단계 2 — 구형 소비자 이관

상태: 부분 완료

- `OBConsumableWidget`과 `OBHUD`를 `UPlayerInventoryComponent`에 바인딩한다.
- 캐릭터 시작 소모품의 구형 컴포넌트 중복 지급을 제거한다.
- 사망 루팅이 `PlayerInventoryComponent`의 장비/가방 인스턴스를 사용하도록 변경한다.
- `DropCorpseLoot()`에서 컨테이너 스폰 성공 후에만 원본 인벤토리를 차감한다.
- 프로젝트 전체 C++와 Blueprint에서 `UOBInventoryComponent` 참조를 제거한다.

완료 조건: HUD, 어빌리티, 무기, 재장전, 사망 루팅이 모두 신형 컴포넌트만 사용한다.

### 단계 3 — 월드 아이템과 루팅 컨테이너 통합

상태: 착수 전

- `AWorldItem`을 공통 상호작용 경로에 연결한다.
- `FWorldItemData`를 제거하고 모든 월드 아이템을 런타임 인스턴스로 초기화한다.
- `AOBLootContainer.Contents`를 런타임 인스턴스 배열로 전환해 시체 무기의 GUID·탄창도 보존한다.
- LootTable의 `FOBItemStack` 결과를 컨테이너 스폰 시 런타임 인스턴스로 변환한다.
- 단일 아이템 가져가기와 전체 가져가기 서버 RPC를 구현한다.
- 부분 추가 시 실제 추가량만 차감하는 트랜잭션 규칙을 적용한다.

완료 조건: 상자, 적 드랍, 플레이어 시체, 바닥 드랍이 동일한 UI/서버 검증 규칙으로 획득된다.

### 단계 4 — 플레이어 내부 컨테이너 중복 제거

상태: 착수 전

- `InventoryContrainerArray`, `InventoryContainerSize`, `DefaultContainerSlotCount`를 제거한다.
- `EInventoryItemLocation::Container`를 외부 루팅 소스 핸들로 대체하거나 UI 전용 위치로 분리한다.
- `InventoryWindow`가 플레이어 가방 배열을 복사 보관하지 않고 현재 소유자의 읽기 전용 뷰를 사용하도록 정리한다.
- 외부 컨테이너 이동 요청은 플레이어 컴포넌트와 컨테이너 사이의 서버 트랜잭션으로만 처리한다.

완료 조건: 외부 루팅 아이템이 플레이어 컴포넌트 안에 복제·복사되지 않는다.

### 단계 5 — 레거시 코드와 자료형 삭제

상태: 착수 전

삭제 대상:

- `Public/Inventory/Components/OBInventoryComponent.h`
- `Private/Inventory/Components/OBInventoryComponent.cpp`
- `FOBWeaponSlotEntry`
- `FOBCountEntry`
- `FWorldItemData`
- `FInventoryQueryResult`
- `EItemType`
- deprecated 수량 변경 래퍼
- 이전 DataAsset/ItemDataSubsystem 기준 인벤토리 문서

완료 조건:

```text
rg "UOBInventoryComponent|FOBWeaponSlotEntry|FOBCountEntry|FWorldItemData|FInventoryQueryResult|EItemType" Source/OutBreak
```

검색 결과가 Core Redirect/마이그레이션 설명 외에는 없어야 한다.

### 단계 6 — 검증 및 문서 확정

상태: 착수 전

- UHT 포함 Development Editor 전체 빌드
- 모든 관련 Blueprint Compile 및 Resave
- Standalone, Listen Server, Dedicated Server + Client 검증
- `DT_Items` 누락/중복 태그와 잘못된 필수 참조 검사
- DataTable Reimport 후 레지스트리 캐시 재생성 확인
- 최종 API와 데이터 작성법 문서화

## 7. 파일별 예정 변경

| 파일/영역 | 예정 변경 |
|---|---|
| `PlayerInventoryComponent.h/.cpp` | 수량 API 단일화, 런타임 인스턴스 이관 API, 외부 컨테이너 배열 제거, 사망 루팅 추출 지원 |
| `OBInventoryComponent.h/.cpp` | 소비자 이관 완료 후 삭제 |
| `InventoryData.h` | 중복 자료형 및 `ItemType` 제거, 핸들 책임 명시 |
| `OBItemTypes.h/.cpp` | `FOBItemStack`을 집계형 표준으로 유지, 런타임 인스턴스 변환 헬퍼 추가 |
| `WorldItem.h/.cpp` | 단일 런타임 인스턴스 모델, 공통 상호작용 적용 |
| `OBLootContainer.h/.cpp` | 런타임 인스턴스 소유, 가져가기 RPC, 원자적 차감 |
| `OBLootTable.h/.cpp` | 확률 결과를 집계형으로 생성하고 런타임 인스턴스로 변환 |
| `OBCharacterBase.cpp` | 구형 컴포넌트 생성/중복 지급 제거, 신형 사망 루팅 사용 |
| `OBHUD.cpp`, `OBConsumableWidget.*` | 신형 컴포넌트 이벤트와 수량 사용 |
| `InventoryWindow.*`, `InventorySlot.*` | 플레이어/외부 컨테이너 공통 표시, 배열 복사 최소화 |
| `OBItemRegistry.*` | Reimport 캐시 무효화, 데이터 검증 강화 |
| `DefaultGame.ini` | 제거된 컨테이너 설정 정리, 데이터 테이블 설정 유지 |
| `DT_Items`, `DA_PawnData_Default` | 기본 배낭 행과 태그 연결 |

## 8. 네트워크 및 보안 규칙

- 클라이언트는 아이템 배열을 직접 수정하지 않는다.
- 이동, 사용, 드랍, 루팅은 서버가 현재 GUID·태그·수량·거리·소유권을 다시 확인한다.
- UI의 배열 인덱스는 힌트일 뿐이며 아이템 식별은 GUID로 재확인한다.
- 태그 기반 스택은 서버가 `MaxStack`과 카테고리를 레지스트리에서 다시 조회한다.
- 컨테이너 가져가기는 `플레이어 추가 -> 성공 수량 계산 -> 컨테이너 차감` 순으로 처리한다.
- 스폰 실패 시 원본 아이템을 제거하지 않는다.
- 사망 루팅도 컨테이너 스폰/초기화 성공 뒤 원본을 제거한다.
- 장비/무기와 일반 스택의 런타임 상태 보존 규칙을 분리한다.

## 9. 검증 체크리스트

### 데이터와 시작 상태

- [ ] `DT_Items`의 모든 태그가 유일하고 유효하다.
- [ ] 기본 배낭 행이 `Equipment` 카테고리와 Backpack 슬롯 데이터를 가진다.
- [ ] 시작 무기, 탄약, 소비품이 한 번만 지급된다.
- [ ] 기본 배낭 용량이 fallback이 아닌 배낭 데이터에서 적용된다.

### 인벤토리 기능

- [ ] 스택 병합, 부분 추가, 인벤토리 가득 참 결과가 실제 추가량과 일치한다.
- [ ] 드래그 이동, 스왑, 장비 장착/해제 후 GUID가 유지된다.
- [ ] 퀵슬롯은 수량이 0이어도 타입 지정이 유지되고 재획득 시 다시 사용된다.
- [ ] 소비품 사용 후 인벤토리와 HUD 수량이 동시에 감소한다.
- [ ] 재장전 후 예비탄과 HUD가 일치한다.

### 드랍과 루팅

- [ ] 무기 드랍/재획득 후 GUID와 탄창 수량이 유지된다.
- [ ] 배낭 드랍/재획득 후 내부 내용물이 유지된다.
- [ ] 상자에서 하나/일부/전체 가져가기가 정확히 차감된다.
- [ ] 가방이 가득 찬 경우 컨테이너 잔여 수량이 보존된다.
- [ ] 적 드랍과 플레이어 시체가 서버에서 한 번만 생성된다.
- [ ] 플레이어 시체에 신형 인벤토리의 무기·장비·가방 내용물이 들어간다.

### 멀티플레이

- [ ] Client가 보낸 잘못된 인덱스/GUID/수량 요청이 거부된다.
- [ ] 두 Client가 같은 컨테이너를 동시에 가져가도 아이템이 복제되지 않는다.
- [ ] OwnerOnly 인벤토리가 다른 플레이어에게 노출되지 않는다.
- [ ] 컨테이너 변경은 모든 관찰 클라이언트 UI에 반영된다.
- [ ] 드랍 액터와 시체 컨테이너의 생성/삭제가 모든 클라이언트에서 일치한다.

## 10. 주요 위험과 대응

| 위험 | 영향 | 대응 |
|---|---|---|
| 반사 자료형/프로퍼티 삭제 | Blueprint 핀과 직렬화 데이터 손실 | 사용처 이관 후 삭제, 필요 시 `CoreRedirects`, 전체 Resave |
| 구형·신형 동시 갱신 기간 | 수량 복제 또는 불일치 | 양방향 동기화 코드를 만들지 않고 소비자를 한 방향으로 순차 이관 |
| 외부 컨테이너 동시 접근 | 아이템 복제/음수 수량 | 서버 원자적 이관과 실제 추가량 기반 차감 |
| DataTable 행 포인터 캐시 | Reimport 후 무효 포인터 | Reimport 이벤트에서 캐시 폐기, 런타임 상태는 태그만 저장 |
| 배낭 용량 축소 | 슬롯 밖 아이템 손실 | 교체 전 수용 가능 여부 검사, 실패 시 원본 유지 |
| 대규모 삭제 커밋 | 회귀 원인 추적 어려움 | API 단일화, 소비자 이관, 자료형 삭제를 별도 커밋으로 분리 |

## 11. 권장 커밋 단위

1. `REFACTOR: Inventory quantity API consolidation`
2. `MIGRATE: HUD and consumables to PlayerInventoryComponent`
3. `MIGRATE: Player death loot to runtime inventory instances`
4. `ADD: Transactional loot container transfer RPCs`
5. `REFACTOR: World item and loot runtime item model`
6. `REMOVE: Legacy OBInventoryComponent and duplicate item types`
7. `DATA: Add backpack row and versioned item/loot source files`
8. `TEST: Inventory and loot multiplayer regression coverage`
9. `DOCS: Final unified inventory architecture`

## 12. 완료 정의

통합 작업은 단순히 빌드가 통과했을 때가 아니라 다음 조건을 모두 만족할 때 완료로 본다.

1. 플레이어 아이템 수량과 런타임 인스턴스의 진실 원본이 `UPlayerInventoryComponent` 하나다.
2. 구형 `UOBInventoryComponent`와 관련 집계 자료형이 삭제됐다.
3. 정적 아이템 정보는 `UOBItemRegistry`와 `DT_Items`에서만 조회된다.
4. 월드 아이템과 루팅 컨테이너가 공통 서버 이관 규칙을 사용한다.
5. 플레이어 컴포넌트가 외부 루팅 컨테이너 배열을 복사 소유하지 않는다.
6. HUD, 어빌리티, 무기, 재장전, 사망 루팅이 동일 인벤토리를 사용한다.
7. Listen/Dedicated 멀티플레이에서 드랍·루팅·동시 접근 검증을 통과한다.
8. Blueprint, DataTable 원본, 설정, 기술 문서가 최종 구조와 일치한다.

## 13. 2026-08-04 구현 진행 결과

### 완료된 통합 작업

- 플레이어 인벤토리의 진실 원본을 `UPlayerInventoryComponent` 하나로 통합했다.
- 구형 `UOBInventoryComponent`와 해당 소스 파일을 제거하고 HUD, 소비품 어빌리티, 시작 아이템, 사망 루팅 사용처를 신형 컴포넌트로 이관했다.
- 태그 수량 작업은 `TryAddItem`, `TryRemoveItem`, `GetItemCount`로, 런타임 상태가 있는 아이템 작업은 `TryAddItemInstance`, `TryExtractItemInstance`로 통합했다.
- `FInventoryQueryResult`, `FWorldItemData`, `EItemType` 및 `FInventoryData::ItemType` 중복 정의를 제거했다.
- 플레이어 컴포넌트 내부에 복사 보관하던 외부 컨테이너 배열과 Container 위치 분기를 제거했다.
- `AWorldItem`을 공통 상호작용 액터로 전환하고 서버 거리 검증을 거쳐 아이템 인스턴스를 획득하도록 변경했다.
- `AOBLootContainer`의 내용물을 `FInventoryData` 인스턴스로 변경해 무기 GUID와 탄창 등 런타임 상태를 보존했다.
- 상자 루팅은 플레이어에 실제로 추가된 수량만 원본에서 차감하며, 사망 루팅은 컨테이너 생성 성공 뒤 플레이어 원본을 비우도록 트랜잭션 순서를 통일했다.
- 플레이어 캐릭터 블루프린트를 재저장해 삭제된 네이티브 컴포넌트의 직렬화 흔적을 정리했다.
- 인벤토리 슬롯 아이콘 조회를 `UOBItemRegistry::GetItemDisplay`로 통일해, `DT_Items.Icon`이 비어 있는 무기도 WeaponData의 `WeaponIcon`을 정상 상속하도록 수정했다.
- `WBP_InventoryWindow`의 고정 1920 기준 그리드 배치를 우측 앵커 기반으로 변경하고 최소 슬롯 크기를 지정해, 작은 PIE 뷰포트에서도 슬롯과 아이콘이 화면 밖으로 밀리거나 0 크기로 축소되지 않도록 수정했다.

### 자동 검증 결과

- UnrealHeaderTool 및 `OutBreakEditor Win64 Development` C++ 빌드 통과.
- 플레이어 캐릭터 블루프린트 대상 `ResavePackages` 통과: 오류 0건.
- 인벤토리·루팅 직접 관련 Blueprint 4개(`BP_SandboxCharacter_Player`, `BP_Corpse`, `BP_LootContainer`, `BP_LootDrop`) 선별 컴파일 통과: 오류 0건, 경고 0건, 로드 실패 0건.
- 프로젝트 전체 Blueprint 검사에서는 관련 블루프린트를 포함해 수백 개 에셋이 컴파일됐으나, 대규모 외부 콘텐츠 때문에 전체 완료 전 중단했다. 외부 에셋의 빈 엔진 버전 등 기존 경고는 이 작업 범위에서 변경하지 않았다.

### 남은 수동 검증

- Listen Server와 Dedicated Server에서 2개 클라이언트가 같은 컨테이너를 동시에 루팅하는 경쟁 조건을 확인한다.
- 무기 드랍/재획득, 재장전 탄창 보존, 배낭 용량 및 내부 아이템 보존을 실제 플레이로 확인한다.
- `DT_Items` 태그 유일성, 기본 배낭 행의 카테고리·슬롯·용량 값, 시작 아이템 중복 지급 여부를 데이터 에디터에서 확인한다.
- 위 수동 검증까지 통과하면 12절의 완료 정의 중 멀티플레이와 데이터 검증 항목을 최종 완료 처리한다.
