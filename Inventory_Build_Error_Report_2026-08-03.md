# Inventory 빌드 오류 추적 보고서

- 작성일: 2026-08-03
- 프로젝트: `OutBreak`
- 빌드 대상: `OutBreakEditor Win64 Development`
- 확인 환경: Unreal Engine 5.7.4, MSVC 14.44.35207
- 조사 범위: Inventory 관련 현재 소스, UHT 생성 코드, 최신 UnrealBuildTool 로그

## 1. 결론 요약

현재 빌드는 서로 독립적인 두 원인 때문에 중단된다.

1. `UInventoryWindow::InventoryArray`가 초기화되지 않은 C++ 참조 멤버이므로 UHT가 생성한 생성자를 컴파일할 수 없다.
2. `TotalStack - MaxStack;`가 결과를 저장하지 않는 무효 표현식이므로 `C4552`가 발생한다.

`InventoryWindow.gen.cpp`의 127행과 128행에서 보고된 두 `C2530`은 별개의 결함이 아니라 `InventoryArray` 선언 하나에서 파생된 중복 증상이다. 생성 파일을 직접 수정하면 다음 UHT 실행 때 덮어써지므로 반드시 원본 헤더를 수정해야 한다.

또한 위 두 컴파일 차단점 뒤에 위젯 null 역참조, 슬롯 데이터 미연결, 미완성 `AddItem()` 등 즉시 드러날 가능성이 높은 런타임/기능 결함이 확인되었다. 따라서 두 줄만 기계적으로 고친 뒤 완료로 판단하면 안 된다.

## 2. 빌드 차단 오류 분석

### 2.1 C2530: `UInventoryWindow::InventoryArray` 참조 초기화 누락

근거 코드:

- `Source/OutBreak/Public/Inventory/Widget/InventoryWindow.h:32-33`

```cpp
UPROPERTY(meta = (AllowPrivateAccess = "true"))
TArray<FInventoryData>& InventoryArray;
```

- UHT 생성 결과 `Intermediate/Build/Win64/UnrealEditor/Inc/OutBreak/UHT/InventoryWindow.gen.cpp:127`

```cpp
UInventoryWindow::UInventoryWindow(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer) {}
```

C++ 참조 멤버는 객체 생성 시 생성자 초기화 목록에서 반드시 실제 객체에 바인딩되어야 한다. 현재 UHT 생성 생성자에는 `InventoryArray` 초기화가 없으므로 `C2530`이 발생한다.

현재 setter도 이 설계를 해결하지 못한다.

```cpp
void UInventoryWindow::SetInventoryArray(TArray<FInventoryData>& ArrayRef)
{
    InventoryArray = ArrayRef;
}
```

참조는 생성 후 다른 객체로 재바인딩할 수 없다. 위 대입은 참조를 연결하는 동작이 아니라, 이미 바인딩된 배열에 `ArrayRef`의 원소를 복사하는 동작이다. 하지만 현재 참조는 애초에 생성할 수 없으므로 setter에 도달할 수 없다.

권장 수정 방향:

- UI가 인벤토리의 스냅샷만 필요하다면 `InventoryArray`를 값 타입 `TArray<FInventoryData>`로 보관하고 setter는 `const TArray<FInventoryData>&`를 받아 복사한다.
- 더 단순하고 안전한 구조는 `Refresh(const TArray<FInventoryData>& Items)` 한 번 안에서 슬롯을 갱신하고, 위젯이 외부 배열의 수명에 의존하지 않게 하는 것이다.
- 반드시 실시간 비소유 참조가 필요하다면 `UPROPERTY`가 아닌 포인터/소유 컴포넌트 참조를 사용하고 null 및 수명 검증을 해야 한다. 단순히 명시적 생성자에서 임시 배열에 참조를 바인딩하는 방식은 setter의 복사 의미와 수명 문제를 남기므로 권장하지 않는다.

### 2.2 C4552: 스택 초과분 계산 결과 미사용

근거 코드:

- `Source/OutBreak/Private/Inventory/Components/PlayerInventoryComponent.cpp:262-266`

```cpp
int TotalStack = Element.ItemStack + ItemStack;
if (TotalStack > MaxStack)
{
    TotalStack - MaxStack;
}
```

`TotalStack - MaxStack`은 계산만 하고 결과를 어느 변수에도 저장하지 않는다. 따라서 프로그램 상태가 전혀 바뀌지 않으며 현재 빌드에서 `C4552`로 처리된다.

호출부가 `ItemData->ItemStack`을 비-const 참조로 넘기고, 주석이 “전부 추가되었는지”를 반환한다고 설명하므로 의도는 다음과 같은 잔여 수량 처리로 추정된다.

```cpp
Element.ItemStack = MaxStack;
ItemStack = TotalStack - MaxStack; // 아직 넣지 못한 잔여 수량
```

반대로 `TotalStack <= MaxStack`이면 기존 슬롯을 `TotalStack`으로 갱신하고 `ItemStack = 0`으로 만든 뒤 성공을 반환하는 분기가 필요하다. 정확한 수정은 빈 슬롯, 컨테이너 사용 순서, 실패 시 월드 아이템 잔량 정책까지 포함하여 `AddItem()` 전체 알고리즘을 완성하면서 적용해야 한다.

## 3. 컴파일 이후 예상되는 후속 결함

### 3.1 [치명적] 컴포넌트 생성 중 `InventoryWidget` null 역참조

호출 경로:

```text
UPlayerInventoryComponent 생성자 (:20)
  -> UpdateInventory() (:202)
     -> UpdateInventoryWidget() (:222)
        -> InventoryWidget->Update() (:229)
```

`InventoryWidget`은 `TObjectPtr`이며 생성자에서 생성하거나 유효성을 검사하지 않는다. Blueprint/에디터 기본값도 네이티브 생성자 실행 시점에는 안전하게 사용할 수 있다고 가정할 수 없다. 초기 배열 크기 설정과 위젯 갱신을 분리하고, 위젯 인스턴스가 생성·바인딩된 뒤에만 갱신해야 한다.

### 3.2 [높음] `SetInventoryArray()` 호출부가 없음

`Source` 전체를 검색한 결과 선언과 구현 외에 `SetInventoryArray()`를 호출하는 코드가 없다. 참조형 문제를 값 타입이나 포인터로 바꾸더라도 백팩 배열이 위젯에 전달되지 않아 UI가 실제 인벤토리와 동기화되지 않는다.

### 3.3 [치명적] 슬롯 데이터 설정 전에 `UInventorySlot::Update()` 호출

- `UInventoryWindow::Update()`는 각 슬롯에 대해 `InventorySlot->Update()`만 호출한다 (`InventoryWindow.cpp:148-154`).
- `UInventorySlot::InventoryData`는 초기화되지 않은 원시 포인터다 (`InventorySlot.h:33`).
- `UInventorySlot::Update()`는 null 검사 없이 `InventoryData->ItemName`을 역참조한다 (`InventorySlot.cpp:23`).
- `SetSlotData()`의 외부 호출부가 없고 접근 수준도 `protected`다.

각 인덱스의 `FInventoryData`를 슬롯에 먼저 전달해야 한다. UI 슬롯이 데이터 값을 복사해 보관하도록 만들면 배열 재할당으로 포인터가 무효화되는 문제도 피할 수 있다.

### 3.4 [높음] `AddItem()`이 전반적으로 미완성

현재 함수는 다음 상태다.

- 일치한 기존 슬롯의 `ItemStack`을 실제로 변경하지 않는다.
- 빈 백팩 슬롯을 찾거나 새 아이템을 기록하지 않는다.
- `InventoryContrainerArray;`는 아무 동작도 하지 않는 독립 표현식이다 (`:269`).
- 모든 실행 경로가 최종적으로 `false`를 반환한다 (`:271`).
- `FindItemRow()` 결과를 null 검사 없이 `MetaData->MaxItemStack`으로 역참조한다 (`:255-256`).

따라서 265행만 대입문으로 바꿔도 아이템 획득 기능은 정상화되지 않는다.

### 3.5 [치명적] 한 저장소에만 아이템이 있으면 `QueryItemEnough()`가 Fatal 처리

`PlayerInventoryComponent.cpp:83`의 조건은 다음과 같다.

```cpp
if (Result.BackpackIndices.Num() == 0 || Result.ContainerIndices.Num() == 0)
```

아이템이 백팩에만 있거나 컨테이너에만 있는 정상 상황도 조건이 참이 되어 `UE_LOG(..., Fatal, ...)`로 종료된다. “양쪽 모두 결과가 없음”을 검사하려는 의도라면 `||`가 아니라 `&&`가 맞다. 다만 앞에서 `HasItem`을 이미 검사하므로 이 검사가 실제로 필요한지도 재검토할 수 있다.

### 3.6 [높음] 정상적인 빈 슬롯이 Fatal 경로로 들어감

배열은 `SetNum()`으로 기본 원소를 만들고 `FInventoryData::ItemName` 기본값은 `NAME_None`이다. 그런데 `UInventorySlot::Update()`는 `ItemName.IsNone()`을 정상적인 빈 슬롯 표시가 아니라 `Fatal`로 처리한다. 인벤토리가 비어 있는 초기 상태에서 슬롯 데이터 연결을 완료하면 곧바로 종료될 수 있다. 빈 슬롯은 이미지와 수량 텍스트를 지우는 정상 상태로 처리해야 한다.

## 4. 권장 수정 순서

1. 인벤토리 데이터 소유권을 확정한다. 권장안은 컴포넌트가 데이터를 소유하고, Window/Slot은 갱신 시 전달받은 값을 표시하는 구조다.
2. `InventoryArray` 참조 멤버를 제거하고 `Refresh(const TArray<FInventoryData>&)` 또는 값 복사 setter로 교체한다.
3. 생성자에서는 배열 크기 같은 데이터 초기화만 수행하고, 위젯 갱신은 `BeginPlay`, 위젯 생성 완료 콜백 또는 명시적 바인딩 이후로 이동한다.
4. Window가 각 슬롯에 해당 인덱스의 데이터를 전달하도록 하고, Slot의 데이터 멤버를 안전하게 초기화한다.
5. `AddItem()`을 기존 스택 채우기 → 빈 백팩 슬롯 → 빈 컨테이너 슬롯 → 잔여 수량/반환값 확정 순서로 완성한다.
6. `QueryItemEnough()`의 단일 저장소 정상 케이스와 빈 슬롯 표시를 수정한다.
7. 클린 빌드 후 PIE 기능 테스트를 수행한다.

## 5. 검증 체크리스트

| 구분 | 검증 시나리오 | 기대 결과 |
|---|---|---|
| 빌드 | `OutBreakEditor Win64 Development` 재빌드 | C2530/C4552 없이 성공 |
| 초기화 | `InventoryWidget` 미지정 상태로 컴포넌트 생성 | 크래시 없이 갱신 생략 및 진단 로그 |
| UI | 빈 백팩/컨테이너 표시 | Fatal 없이 빈 슬롯 렌더링 |
| UI | 배열 크기 증가/감소 후 갱신 | 슬롯 수와 데이터가 정확히 일치 |
| 추가 | 최대 스택 미만 아이템 추가 | 기존 스택 증가, 잔여 0, 성공 반환 |
| 추가 | 최대 스택 초과 아이템 추가 | 기존 슬롯은 최대치, 초과분은 다음 빈 슬롯로 이동 |
| 추가 | 백팩 가득 참, 컨테이너 여유 | 컨테이너에 잔여 수량 저장 |
| 추가 | 전체 공간 부족 | 저장된 양과 외부 잔여 수량이 일치, 실패 반환 |
| 조회 | 아이템이 백팩에만 존재 | 정상 조회, Fatal 없음 |
| 조회 | 아이템이 컨테이너에만 존재 | 정상 조회, Fatal 없음 |
| 소비 | 양쪽 저장소에 분산된 수량 소비 | 요청량만 차감되고 빈 슬롯 정리/갱신 |

## 6. 조사 시 변경 사항

Inventory 소스 코드는 수정하지 않았다. 본 문서만 추가했으며, 현재 실패 상태에서 동일 빌드를 반복 실행하지 않았다.
