# Unreal Engine Shop Widget C++ Backend 및 WBP 구성 가이드

## 1. 시스템 개요

상점 UI는 C++에서 위젯을 생성하거나 좌표를 설정하지 않는다. C++는 `UUserWidget` 부모 클래스, `BindWidget` 계약, 표시용 View Data, 사용자 요청 Command, Gateway 추상 경계, ViewModel 상태 저장소만 제공한다. 실제 Widget Blueprint 생성, Panel 배치, Anchor, Alignment, Offset, Padding, Brush, Font, Animation은 WBP에서 조정한다.

데이터 흐름:

```text
Game Data / Database / Inventory / Server
-> UShopDataGatewayBase 파생 클래스
-> UShopViewModel 또는 UShopWindowWidget Entry Point
-> UShopWindowWidget
-> 각 Section Widget
-> ListView Entry Data UObject
-> Entry Widget
```

사용자 입력 흐름:

```text
Entry 또는 Section 입력
-> Section Delegate
-> UShopWindowWidget
-> FShop...Request Command 생성
-> UShopWindowWidget End Point Delegate Broadcast
-> UShopDataGatewayBase 요청 함수
-> Game System / Database / Server
```

## 2. 생성된 C++ 클래스 목록

| 클래스 | 부모 클래스 | 역할 | 소유 관계 | 데이터 입력 | 출력 Delegate |
|---|---|---|---|---|---|
| `UShopWindowWidget` | `UUserWidget` | 상점 전체 화면 중앙 조정자 | 각 Section을 `BindWidget`으로 소유 | `ApplyShopViewState`, `InitializeShop`, Gateway/ViewModel | `OnTabRequested`, `OnCategoryRequested`, `OnItemSelectionRequested`, `OnSortRequested`, `OnPurchaseRequested`, `OnBarterRequested`, `OnStockRefreshRequested`, `OnCompareRequested`, `OnCloseRequested` |
| `UShopHeaderSectionWidget` | `UUserWidget` | 상인/평판/지갑 표시 | ShopWindow의 Header Section | `ApplyVendorData`, `ApplyWalletData` | 없음 |
| `UShopTabSectionWidget` | `UUserWidget` | 탭 목록 표시와 탭 요청 | ShopWindow의 Tab Section | `ApplyTabs`, `SetSelectedTab` | `OnTabRequested(FName)` |
| `UShopCategorySectionWidget` | `UUserWidget` | 카테고리 목록과 재고 갱신 요청 | ShopWindow의 Category Section | `ApplyCategories`, `SetSelectedCategory` | `OnCategoryRequested(FName)`, `OnStockRefreshRequested()` |
| `UShopItemListSectionWidget` | `UUserWidget` | 중앙 아이템 목록 표시 | ShopWindow의 Item List Section | `ApplyItems`, `SetSelectedItem` | `OnItemSelectionRequested(FName)`, `OnSortRequested(FName)` |
| `UShopItemDetailSectionWidget` | `UUserWidget` | 선택 아이템 상세 표시 | ShopWindow의 Item Detail Section | `ApplyItemDetail`, `ApplyActionState` | 없음 |
| `UShopActionSectionWidget` | `UUserWidget` | 구매/교환/수량 입력 | Item Detail Section 내부 | `ApplyActionState` | `OnPurchaseRequested`, `OnBarterRequested`, `OnQuantityChanged` |
| `UShopFooterSectionWidget` | `UUserWidget` | 닫기/뒤로/비교/도움말 요청 | ShopWindow의 Footer Section | `ApplyKeyHints` | `OnCloseRequested`, `OnBackRequested`, `OnCompareRequested`, `OnHelpRequested` |
| `UShopDataGatewayBase` | `UObject` | UI와 실제 데이터 시스템 사이의 추상 경계 | 외부 시스템이 생성/주입 | 요청 함수 호출 | `OnShopViewStateReceived`, `OnCategoryItemsReceived`, `OnItemDetailReceived`, `OnTransactionCompleted`, `OnGatewayError`, `OnBusyStateChanged` |
| `UShopViewModel` | `UObject` | 명시적 상점 상태 저장소 | 외부 시스템 또는 ShopWindow가 보유 | Setter, `ApplyViewState` | `OnViewStateChanged` |

Entry Widget:

| 클래스 | 부모 클래스 | 역할 |
|---|---|---|
| `UShopTabEntryWidget` | `UUserWidget`, `IUserObjectListEntry` | 탭 행 표시 |
| `UShopCategoryEntryWidget` | `UUserWidget`, `IUserObjectListEntry` | 카테고리 행 표시 |
| `UShopItemEntryWidget` | `UUserWidget`, `IUserObjectListEntry` | 아이템 요약 행 표시 |
| `UShopItemStatEntryWidget` | `UUserWidget`, `IUserObjectListEntry` | 아이템 스탯 행 표시 |
| `UShopCurrencyEntryWidget` | `UUserWidget`, `IUserObjectListEntry` | 화폐 행 표시 |
| `UShopKeyHintEntryWidget` | `UUserWidget`, `IUserObjectListEntry` | 키 힌트 행 표시 |

Entry Data UObject:

`UShopTabEntryData`, `UShopCategoryEntryData`, `UShopItemEntryData`, `UShopItemStatEntryData`, `UShopCurrencyEntryData`, `UShopKeyHintEntryData`

이 객체들은 화면 표시 데이터만 가진다. Actor, Component, DataTable Row 포인터, Database Row 포인터, 서버 핸들은 보관하지 않는다.

## 3. WBP 생성 순서

1. `WBP_ShopWindow`를 생성하고 부모 클래스를 `UShopWindowWidget`으로 설정한다.
2. 아래 Section WBP를 각각 생성한다.

| WBP 이름 | 부모 클래스 |
|---|---|
| `WBP_ShopHeaderSection` | `UShopHeaderSectionWidget` |
| `WBP_ShopTabSection` | `UShopTabSectionWidget` |
| `WBP_ShopCategorySection` | `UShopCategorySectionWidget` |
| `WBP_ShopItemListSection` | `UShopItemListSectionWidget` |
| `WBP_ShopItemDetailSection` | `UShopItemDetailSectionWidget` |
| `WBP_ShopActionSection` | `UShopActionSectionWidget` |
| `WBP_ShopFooterSection` | `UShopFooterSectionWidget` |

3. `WBP_ShopWindow` 안에 Section WBP들을 배치하고 Component 이름을 C++ `BindWidget` 이름과 정확히 맞춘다.
4. 아래 Entry WBP를 각각 생성하고 부모 클래스를 지정한다.

| WBP 이름 예시 | 부모 클래스 | 사용 ListView |
|---|---|---|
| `WBP_ShopTabEntry` | `UShopTabEntryWidget` | `TabListView` |
| `WBP_ShopCategoryEntry` | `UShopCategoryEntryWidget` | `CategoryListView` |
| `WBP_ShopItemEntry` | `UShopItemEntryWidget` | `ItemListView` |
| `WBP_ShopItemStatEntry` | `UShopItemStatEntryWidget` | `StatListView` |
| `WBP_ShopCurrencyEntry` | `UShopCurrencyEntryWidget` | `CurrencyListView` |
| `WBP_ShopKeyHintEntry` | `UShopKeyHintEntryWidget` | `KeyHintListView` |

5. 각 `UListView`의 Entry Widget Class를 해당 Entry WBP로 지정한다.
6. C++는 `WidgetTree`, `ConstructWidget`, `CreateWidget`을 사용해 Section이나 Component를 만들지 않는다.

## 4. BindWidget Component 계약

이름은 대소문자를 포함해 정확히 일치해야 한다.

### UShopWindowWidget

| Component 이름 | C++ 타입 | 필수 여부 | 역할 |
|---|---|---|---|
| `HeaderSection` | `UShopHeaderSectionWidget` | 필수 | 상단 상인/지갑 정보 |
| `TabSection` | `UShopTabSectionWidget` | 필수 | 상단 탭 목록 |
| `CategorySection` | `UShopCategorySectionWidget` | 필수 | 좌측 카테고리 |
| `ItemListSection` | `UShopItemListSectionWidget` | 필수 | 중앙 아이템 목록 |
| `ItemDetailSection` | `UShopItemDetailSectionWidget` | 필수 | 우측 아이템 상세 |
| `FooterSection` | `UShopFooterSectionWidget` | 필수 | 하단 입력/닫기 영역 |
| `LoadingOverlay` | `UWidget` | 선택 | 로딩 상태 표시 |
| `ErrorOverlay` | `UWidget` | 선택 | 오류 상태 표시 |
| `ErrorMessageText` | `UTextBlock` | 선택 | 오류 메시지 표시 |

### UShopHeaderSectionWidget

| Component 이름 | C++ 타입 | 필수 여부 | 역할 |
|---|---|---|---|
| `VendorPortraitImage` | `UImage` | 필수 | 상인 초상화 |
| `VendorRoleText` | `UTextBlock` | 필수 | 상인 역할 |
| `VendorNameText` | `UTextBlock` | 필수 | 상인 이름 |
| `VendorDescriptionText` | `UTextBlock` | 필수 | 상인 설명 |
| `ReputationNameText` | `UTextBlock` | 필수 | 평판 이름 |
| `ReputationValueText` | `UTextBlock` | 필수 | 평판 수치 |
| `ReputationProgressBar` | `UProgressBar` | 필수 | 평판 진행률 |
| `CurrencyListView` | `UListView` | 필수 | 보유 화폐 목록 |

### UShopTabSectionWidget

| Component 이름 | C++ 타입 | 필수 여부 | 역할 |
|---|---|---|---|
| `TabListView` | `UListView` | 필수 | 탭 Entry 목록 |

### UShopCategorySectionWidget

| Component 이름 | C++ 타입 | 필수 여부 | 역할 |
|---|---|---|---|
| `CategoryListView` | `UListView` | 필수 | 카테고리 Entry 목록 |
| `StockRefreshText` | `UTextBlock` | 필수 | 재고 갱신 상태 문구 |
| `RefreshButton` | `UButton` | 선택 | 재고 새로고침 요청 |
| `EmptyCategoryText` | `UTextBlock` | 선택 | 카테고리 없음 안내 |

### UShopItemListSectionWidget

| Component 이름 | C++ 타입 | 필수 여부 | 역할 |
|---|---|---|---|
| `ItemListView` | `UListView` | 필수 | 아이템 Entry 목록 |
| `ItemCountText` | `UTextBlock` | 필수 | 표시 아이템 개수 |
| `EmptyStateText` | `UTextBlock` | 선택 | 아이템 없음 안내 |
| `LoadingIndicator` | `UWidget` | 선택 | 목록 로딩 표시 |
| `SortButton` | `UButton` | 선택 | 정렬 요청 |

### UShopItemDetailSectionWidget

| Component 이름 | C++ 타입 | 필수 여부 | 역할 |
|---|---|---|---|
| `ItemNameText` | `UTextBlock` | 필수 | 선택 아이템 이름 |
| `ItemTypeText` | `UTextBlock` | 필수 | 선택 아이템 타입 |
| `RarityText` | `UTextBlock` | 필수 | 희귀도 |
| `DescriptionText` | `UTextBlock` | 필수 | 설명 |
| `ItemPreviewImage` | `UImage` | 필수 | 아이템 미리보기 |
| `OwnedQuantityText` | `UTextBlock` | 필수 | 보유 수량 |
| `PriceText` | `UTextBlock` | 필수 | 가격 |
| `CurrencyIconImage` | `UImage` | 필수 | 가격 화폐 아이콘 |
| `StatListView` | `UListView` | 필수 | 스탯 목록 |
| `ActionSection` | `UShopActionSectionWidget` | 필수 | 구매/교환 입력 영역 |
| `EmptyStateWidget` | `UWidget` | 선택 | 선택 없음 안내 |

### UShopActionSectionWidget

| Component 이름 | C++ 타입 | 필수 여부 | 역할 |
|---|---|---|---|
| `BuyButton` | `UButton` | 필수 | 구매 요청 |
| `QuantityText` | `UTextBlock` | 필수 | 현재 수량 |
| `BarterButton` | `UButton` | 선택 | 교환 요청 |
| `IncreaseQuantityButton` | `UButton` | 선택 | 수량 증가 |
| `DecreaseQuantityButton` | `UButton` | 선택 | 수량 감소 |
| `MaximumQuantityText` | `UTextBlock` | 선택 | 최대 수량 |
| `HoldProgressBar` | `UProgressBar` | 선택 | 홀드 입력 진행률 |
| `UnavailableReasonText` | `UTextBlock` | 선택 | 구매 불가 사유 |

### UShopFooterSectionWidget

| Component 이름 | C++ 타입 | 필수 여부 | 역할 |
|---|---|---|---|
| `CloseButton` | `UButton` | 필수 | 상점 닫기 |
| `BackButton` | `UButton` | 선택 | 뒤로가기 |
| `CompareButton` | `UButton` | 선택 | 비교 요청 |
| `HelpButton` | `UButton` | 선택 | 도움말 요청 |
| `KeyHintListView` | `UListView` | 선택 | 키 힌트 목록 |

### Entry Widget BindWidget

| 클래스 | Component 이름 | C++ 타입 | 필수 여부 | 역할 |
|---|---|---|---|---|
| `UShopTabEntryWidget` | `TabLabelText` | `UTextBlock` | 필수 | 탭 이름 |
| `UShopTabEntryWidget` | `SelectedIndicator` | `UWidget` | 선택 | 선택 강조 |
| `UShopCategoryEntryWidget` | `CategoryNameText` | `UTextBlock` | 필수 | 카테고리 이름 |
| `UShopCategoryEntryWidget` | `ItemCountText` | `UTextBlock` | 선택 | 아이템 개수 |
| `UShopCategoryEntryWidget` | `SelectedIndicator` | `UWidget` | 선택 | 선택 강조 |
| `UShopItemEntryWidget` | `ItemNameText` | `UTextBlock` | 필수 | 아이템 이름 |
| `UShopItemEntryWidget` | `PriceText` | `UTextBlock` | 필수 | 가격 |
| `UShopItemEntryWidget` | `ItemTypeText` | `UTextBlock` | 선택 | 아이템 타입 |
| `UShopItemEntryWidget` | `RarityText` | `UTextBlock` | 선택 | 희귀도 |
| `UShopItemEntryWidget` | `OwnedQuantityText` | `UTextBlock` | 선택 | 보유 수량 |
| `UShopItemEntryWidget` | `UnavailableReasonText` | `UTextBlock` | 선택 | 구매 불가 사유 |
| `UShopItemEntryWidget` | `ThumbnailImage` | `UImage` | 선택 | 썸네일 |
| `UShopItemEntryWidget` | `SelectedIndicator` | `UWidget` | 선택 | 선택 강조 |
| `UShopItemStatEntryWidget` | `StatNameText` | `UTextBlock` | 필수 | 스탯 이름 |
| `UShopItemStatEntryWidget` | `StatValueText` | `UTextBlock` | 필수 | 스탯 값 |
| `UShopItemStatEntryWidget` | `DeltaText` | `UTextBlock` | 선택 | 비교 증감 |
| `UShopCurrencyEntryWidget` | `CurrencyNameText` | `UTextBlock` | 필수 | 화폐 이름 |
| `UShopCurrencyEntryWidget` | `CurrencyAmountText` | `UTextBlock` | 필수 | 보유량 |
| `UShopCurrencyEntryWidget` | `CurrencyIconImage` | `UImage` | 선택 | 화폐 아이콘 |
| `UShopKeyHintEntryWidget` | `ActionText` | `UTextBlock` | 필수 | 액션 이름 |
| `UShopKeyHintEntryWidget` | `KeyText` | `UTextBlock` | 필수 | 키 이름 |

## 5. 권장 Widget Class 계층

```text
WBP_ShopWindow : UShopWindowWidget
├─ HeaderSection : WBP_ShopHeaderSection
├─ TabSection : WBP_ShopTabSection
├─ CategorySection : WBP_ShopCategorySection
├─ ItemListSection : WBP_ShopItemListSection
├─ ItemDetailSection : WBP_ShopItemDetailSection
│  └─ ActionSection : WBP_ShopActionSection
└─ FooterSection : WBP_ShopFooterSection
```

실제 Panel 계층은 자유롭게 구성할 수 있다. `Canvas Panel`, `Overlay`, `Vertical Box`, `Horizontal Box`, `Size Box` 중 어떤 부모 Panel을 쓰더라도 C++는 Panel 타입에 의존하지 않는다.

## 6. 상대적 레이아웃 가이드

기준 화면 비율은 16:9로 가정하되, 고정 픽셀 좌표를 사용하지 않는다. 아래 값은 시작점용 상대 비율이다.

| 영역 | 권장 위치 | 권장 비율 |
|---|---|---|
| Header Section | 화면 상단 전체 폭 | 화면 높이의 12~17% |
| Tab Section | Header 바로 아래 전체 폭 | 화면 높이의 5~8% |
| Body 영역 | Tab 아래, Footer 위 | 남은 높이 대부분 |
| Category Section | Body 좌측 | 화면 너비의 18~22% |
| Item List Section | Body 중앙 | 화면 너비의 38~45% |
| Item Detail Section | Body 우측 | 화면 너비의 30~36% |
| Footer Section | 화면 하단 전체 폭 | 화면 높이의 6~10% |
| Section 간격 | Section 사이 | 화면 너비의 1~2% 이하 |

WBP 배치 권장:

- `WBP_ShopWindow` Root에는 전체 화면을 채우는 Panel을 둔다.
- Header/Footer는 가로 전체 폭에 Anchor를 맞춘다.
- Body는 좌측 Category, 중앙 Item List, 우측 Item Detail을 수평 배치한다.
- Category/ItemList/ItemDetail의 상대 너비는 위 표를 시작점으로 잡고 실제 텍스트 길이와 이미지 비율에 맞춰 WBP에서 조정한다.
- C++ 코드에는 Anchor, Alignment, Offset, Size, Padding, ZOrder 값을 넣지 않는다.
- 정확한 좌표 계산은 하지 않는다. 사용자가 WBP에서 미리보기 해상도별로 직접 조정한다.

## 7. Entry Point 목록

| 함수 | 호출 주체 | 입력 데이터 | 호출 시점 | 갱신 Section | 빈 데이터 처리 | Game Thread |
|---|---|---|---|---|---|---|
| `SetShopViewModel` | Game Code / Blueprint | `UShopViewModel*` | 상점 초기화 전후 | 전체 | ViewModel 상태 적용 | 필요 |
| `SetShopDataGateway` | Game Code / Blueprint | `UShopDataGatewayBase*` | 상점 초기화 전 | 없음 | 기존 Gateway Delegate 해제 | 필요 |
| `InitializeShop` | Game Code / Blueprint | `FShopInitializationData` | 상점 진입 | 전체 요청 시작 | Gateway 없으면 현재 상태 적용 | 필요 |
| `OpenShop` | Game Code / Blueprint | 없음 | 상점 표시 | 전체 표시 | 없음 | 필요 |
| `ApplyShopViewState` | Gateway / ViewModel / Game Code | `FShopViewState` | 전체 스냅샷 수신 | 전체 | 선택 없음이면 Detail Clear | 필요 |
| `ApplyVendorData` | Game Code / Gateway | `FShopVendorViewData` | 상인 데이터 수신 | Header | 빈 텍스트 적용 | 필요 |
| `ApplyWalletData` | Game Code / Gateway | `FShopWalletViewData` | 지갑 갱신 | Header | Currency List 비움 | 필요 |
| `ApplyTabs` | Game Code / Gateway | `TArray<FShopTabViewData>` | 탭 구성 수신 | Tab | Tab List 비움 | 필요 |
| `ApplyCategories` | Game Code / Gateway | `TArray<FShopCategoryViewData>` | 카테고리 수신 | Category | Empty 표시 | 필요 |
| `ApplyVisibleItems` | Game Code / Gateway | `TArray<FShopItemSummaryViewData>` | 목록 수신 | Item List | Empty 표시 | 필요 |
| `ApplySelectedItem` | Game Code / Gateway | `FShopItemDetailViewData` | 상세 수신 | Item Detail, Action | ItemId 없음이면 선택 없음 | 필요 |
| `ApplyTransactionResult` | Gateway / Game Code | `FShopTransactionResult` | 구매/교환 응답 | Header, Item List, Detail | 실패 시 Error 적용 | 필요 |
| `ApplyError` | Gateway / Game Code | `FShopErrorViewData` | 오류 수신 | Error Overlay | Message 비면 숨김 | 필요 |
| `SetBusy` | Gateway / Game Code | `bool` | 비동기 요청 시작/끝 | Loading | false면 숨김 | 필요 |
| `SetInteractionEnabled` | Game Code / Gateway | `bool` | 입력 잠금/해제 | 전체 | false면 버튼 비활성 | 필요 |
| `ResetShop` | Game Code / Blueprint | 없음 | 상점 재사용 전 | 전체 Clear | 모든 목록 비움 | 필요 |
| `CloseShop` | Game Code / Footer | 없음 | 상점 닫기 | 전체 숨김 | Pending Request 취소 | 필요 |

## 8. End Point Delegate 목록

| Delegate | 발생 Widget | 전달 데이터 | 수신 주체 | Gateway 연결 |
|---|---|---|---|---|
| `OnTabRequested` | `UShopWindowWidget` | `FShopTabRequest` | Game System / Blueprint | 기본 구현은 Broadcast만 수행 |
| `OnCategoryRequested` | `UShopWindowWidget` | `FShopCategoryRequest` | Game System / Blueprint | `RequestCategoryItems` 호출 |
| `OnItemSelectionRequested` | `UShopWindowWidget` | `FShopItemSelectionRequest` | Game System / Blueprint | `RequestItemDetail` 호출 |
| `OnSortRequested` | `UShopWindowWidget` | `FShopSortRequest` | Game System / Blueprint | 기본 구현은 Broadcast만 수행 |
| `OnPurchaseRequested` | `UShopWindowWidget` | `FShopPurchaseRequest` | Game System / Blueprint | `SubmitPurchase` 호출 |
| `OnBarterRequested` | `UShopWindowWidget` | `FShopBarterRequest` | Game System / Blueprint | `SubmitBarter` 호출 |
| `OnStockRefreshRequested` | `UShopWindowWidget` | `FShopStockRefreshRequest` | Game System / Blueprint | `RequestStockRefresh` 호출 |
| `OnCompareRequested` | `UShopWindowWidget` | 없음 | Game System / Blueprint | 직접 연결 없음 |
| `OnCloseRequested` | `UShopWindowWidget` | 없음 | Game System / Blueprint | Pending Request 취소 후 닫기 |
| `OnBackRequested` | `UShopWindowWidget` | 없음 | Game System / Blueprint | 직접 연결 없음 |
| `OnHelpRequested` | `UShopWindowWidget` | 없음 | Game System / Blueprint | 직접 연결 없음 |

## 9. ListView 설정 방법

- 각 ListView의 Entry Widget Class는 WBP에서 지정한다.
- Entry Widget은 `IUserObjectListEntry`를 구현한다.
- C++는 `UShop...EntryData` UObject를 만들어 ListView에 전달한다.
- Entry Widget은 `NativeOnListItemObjectSet`에서 Entry Data를 안전하게 Cast한다.
- 잘못된 타입이 들어오면 Entry를 초기화하고 로그를 남긴다.
- Entry Widget은 Gateway, ViewModel, Database, Inventory를 직접 참조하지 않는다.
- Entry Widget 재사용으로 이전 이미지/텍스트/선택 표시가 남지 않도록 `ResetEntry`를 먼저 호출한다.
- 클릭 이벤트는 Entry Widget 내부 버튼이 아니라 Section의 `UListView::OnItemClicked()`에서 받아 ShopWindow로 전달한다.

## 10. 데이터 주입 예시

```cpp
// Database Row 또는 게임 데이터 구조
// -> 게임 전용 변환 코드
FShopItemSummaryViewData ItemViewData;
ItemViewData.ItemId = ItemRow.ItemId;
ItemViewData.DisplayName = ItemRow.DisplayName;
ItemViewData.PriceText = FText::AsNumber(ItemRow.Price);

FShopViewState ViewState;
ViewState.Vendor = ConvertedVendor;
ViewState.Wallet = ConvertedWallet;
ViewState.Tabs = ConvertedTabs;
ViewState.Categories = ConvertedCategories;
ViewState.VisibleItems = ConvertedItems;
ViewState.bCanInteract = true;

ShopWindow->ApplyShopViewState(ViewState);
```

Gateway 사용 예시 흐름:

```cpp
ShopWindow->SetShopDataGateway(MyGateway);
ShopWindow->InitializeShop(InitializationData);

// MyGateway 파생 클래스 내부
OnShopViewStateReceived.Broadcast(ViewState);
```

## 11. 구매 요청 예시

```text
BuyButton 클릭
-> UShopActionSectionWidget::OnPurchaseRequested(FShopPurchaseRequest)
-> UShopWindowWidget::HandlePurchaseRequested
-> VendorId, CurrencyId 보강
-> UShopWindowWidget::OnPurchaseRequested Broadcast
-> UShopDataGatewayBase::SubmitPurchase
-> 거래 결과 수신
-> UShopWindowWidget::ApplyTransactionResult
-> Wallet, Item List, Item Detail 갱신
```

`UShopActionSectionWidget`은 화폐 차감, 인벤토리 추가, 서버 요청을 직접 수행하지 않는다.

## 12. Delegate 생명주기

- 고정 Button 이벤트는 각 Section의 `NativeOnInitialized`에서 `AddUniqueDynamic`으로 등록한다.
- `NativeDestruct`에서 Button Delegate를 `RemoveDynamic`으로 해제한다.
- ListView 네이티브 클릭 이벤트는 Section의 `NativeOnInitialized`에서 등록하고 `NativeDestruct`에서 `RemoveAll(this)`로 해제한다.
- ShopWindow는 Section Delegate를 `NativeConstruct`에서 등록하고 `NativeDestruct`에서 해제한다.
- Gateway나 ViewModel을 교체할 때 기존 객체의 Delegate를 먼저 해제한 뒤 새 객체를 바인딩한다.
- 상점이 닫히거나 Widget이 파괴될 때 `CancelPendingRequests`를 호출해 늦게 도착하는 비동기 응답을 방지한다.
- 같은 객체를 다시 설정해도 중복 바인딩되지 않도록 `AddUniqueDynamic`을 사용한다.

## 13. 확장 방법

- 판매 탭 추가: `FShopTabViewData`를 추가하고 Gateway가 탭별 카테고리/목록을 제공한다.
- 교환 탭 추가: `UShopActionSectionWidget`의 `OnBarterRequested`를 Gateway 파생 클래스에서 처리한다.
- 새 화폐 추가: `FShopCurrencyViewData`를 지갑 배열에 추가하고 `CurrencyListView` Entry WBP를 조정한다.
- 새 카테고리 추가: `FShopCategoryViewData`를 추가하고 CategoryId 기반으로 목록을 요청한다.
- 새 아이템 스탯 추가: `FShopItemStatViewData`를 `FShopItemDetailViewData::Stats`에 추가한다.
- 아이템 비교: `OnCompareRequested`를 외부 비교 시스템에 연결한다.
- 상점별 스타일: WBP, Brush, Font, Material, Animation만 교체하고 C++ 계약은 유지한다.
- 재고 갱신 타이머: Gateway 파생 클래스에서 비동기 요청을 수행하고 결과를 `OnShopViewStateReceived` 또는 `OnCategoryItemsReceived`로 전달한다.
- 게임패드 입력: Footer의 `ApplyKeyHints`에 입력 장치별 표시 텍스트를 주입한다.
- CommonUI 적용: WBP 부모나 버튼 타입을 프로젝트 표준에 맞게 확장하되 `BindWidget` 이름과 중앙 Delegate 흐름은 유지한다.

## Build.cs 변경 사항

상점 UI C++ 부모 클래스와 `FSlateBrush` View Data를 위해 `OutBreak.Build.cs`에 다음 모듈 의존성이 포함된다.

```csharp
"UMG",
"Slate",
"SlateCore",
```

기존 프로젝트에 이미 `ModelViewViewModel` 플러그인 사용 흔적이 있지만, 상점 UI는 `UShopViewModel`의 명시적 Delegate 기반 상태 갱신으로 구성되어 매 프레임 UMG Property Binding Getter 호출에 의존하지 않는다.

## Unreal Engine 제약 사항

- C++는 Widget Component를 생성하지 않는다.
- C++는 WBP 좌표, Anchor, Alignment, Offset, Padding, Size, ZOrder를 설정하지 않는다.
- C++는 Texture/Material/Font/Widget Blueprint/Brush 에셋 경로를 하드코딩하지 않는다.
- Widget은 Database, Server, Inventory, DataTable에 직접 접근하지 않는다.
- Widget Tick으로 매 프레임 상태를 갱신하지 않는다.
- 기존 UMG Property Binding Getter를 매 프레임 상태 갱신 용도로 사용하지 않는다.
- 데이터 변경은 `Apply...` 함수, Gateway Delegate, ViewModel Delegate로 명시적으로 전달한다.
