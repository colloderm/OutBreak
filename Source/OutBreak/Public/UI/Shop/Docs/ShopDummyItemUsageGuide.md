# 더미 상점 아이템 구조체 사용 가이드

## 1. 목적

`ShopDummyItemTypes.h`는 실제 상점/인벤토리/저장 시스템이 완성되기 전에도 상점 UI를 테스트할 수 있도록 만든 더미 표시 데이터 구조체 파일이다.

이 파일은 UI 전용 표시 구조체인 `FShopWindowViewData`, `FShopItemViewData`, `FShopCategoryViewData`로 변환되는 얇은 어댑터 역할만 한다. 실제 재화 차감, 인벤토리 지급, 상점 재고 변경, 서버 검증은 여기서 처리하지 않는다.

## 2. 파일 위치

```text
Source/OutBreak/Public/UI/Shop/ShopDummyItemTypes.h
```

사용법 문서:

```text
Source/OutBreak/Public/UI/Shop/Docs/ShopDummyItemUsageGuide.md
```

## 3. 포함된 구조체

`FShopDummyItemStatData`

- 더미 아이템의 스탯 한 줄을 표현한다.
- `FShopItemStatViewData`로 변환된다.
- 예: 공격력, 탄약 수, 회복량, 무게, 희귀도 등.

`FShopDummyItemActionData`

- 아이템 상세 패널의 행동 버튼 하나를 표현한다.
- `FShopActionViewData`로 변환된다.
- 예: 구매, 교환, 요청, 장착 미리보기 등.

`FShopDummyItemData`

- 상점 아이템 하나를 표현한다.
- `FShopItemViewData`로 변환된다.
- 이름, 설명, 가격, 재고, 보유 수량, 스탯, 행동 버튼 배열을 가진다.

`FShopDummyCategoryData`

- 상점 카테고리 하나를 표현한다.
- `FShopCategoryViewData`로 변환된다.
- 아이템 수는 `FShopDummyShopData::ToWindowViewData()`에서 자동 계산된다.

`FShopDummyShopData`

- 더미 상점 전체 데이터를 표현한다.
- `FShopWindowViewData`로 변환된다.
- `UShopWindow::InitializeShop` 또는 `RefreshShop`에 바로 전달할 수 있다.

## 4. 기본 흐름

```text
FShopDummyShopData
  -> ToWindowViewData()
  -> FShopWindowViewData
  -> UShopWindow::InitializeShop()
  -> 상점 UI 표시
```

더미 구조체는 최종 UI 구조체를 대체하지 않는다. 외부 테스트 코드나 임시 프레젠터에서 데이터를 만들기 쉽게 해주는 보조 타입이다.

## 5. C++ 사용 예시

```cpp
#include "UI/Shop/ShopDummyItemTypes.h"
#include "UI/Shop/ShopWindow.h"

FShopDummyShopData BuildDummyShopData()
{
	FShopDummyShopData ShopData;
	ShopData.ShopId = TEXT("DummyShop");
	ShopData.Shopkeeper.ShopkeeperId = TEXT("DummyShopkeeper");
	ShopData.Shopkeeper.RoleText = FText::FromString(TEXT("보급 담당"));
	ShopData.Shopkeeper.DisplayName = FText::FromString(TEXT("테스트 상인"));
	ShopData.Shopkeeper.Subtitle = FText::FromString(TEXT("UI 테스트용 더미 상점"));
	ShopData.Currency.Scrap = 500;
	ShopData.NewStockText = FText::FromString(TEXT("다음 갱신: 테스트 데이터"));

	FShopDummyCategoryData ConsumableCategory;
	ConsumableCategory.CategoryId = TEXT("Consumables");
	ConsumableCategory.DisplayName = FText::FromString(TEXT("소모품"));
	ConsumableCategory.SortOrder = 10;
	ShopData.Categories.Add(ConsumableCategory);

	FShopDummyItemData Bandage;
	Bandage.ItemId = TEXT("DummyBandage");
	Bandage.CategoryId = ConsumableCategory.CategoryId;
	Bandage.DisplayName = FText::FromString(TEXT("더미 붕대"));
	Bandage.MetaText = FText::FromString(TEXT("소모품"));
	Bandage.Description = FText::FromString(TEXT("상점 UI 표시 테스트용 아이템입니다."));
	Bandage.Price = 35;
	Bandage.StockQuantity = 6;
	Bandage.OwnedQuantity = 2;
	Bandage.SortOrder = 10;

	FShopDummyItemStatData HealStat;
	HealStat.StatId = TEXT("Heal");
	HealStat.DisplayName = FText::FromString(TEXT("회복량"));
	HealStat.DisplayValue = FText::FromString(TEXT("25"));
	Bandage.Stats.Add(HealStat);

	FShopDummyItemActionData BuyAction;
	BuyAction.ActionId = TEXT("Buy");
	BuyAction.Label = FText::FromString(TEXT("구매"));
	BuyAction.InputKey = EKeys::E;
	BuyAction.InputDisplayText = FText::FromString(TEXT("E"));
	BuyAction.ActionType = EShopActionType::Purchase;
	BuyAction.Quantity = 1;
	Bandage.Actions.Add(BuyAction);

	FShopDummyItemActionData ExchangeAction;
	ExchangeAction.ActionId = TEXT("Exchange");
	ExchangeAction.Label = FText::FromString(TEXT("교환"));
	ExchangeAction.InputKey = EKeys::Q;
	ExchangeAction.InputDisplayText = FText::FromString(TEXT("Q"));
	ExchangeAction.ActionType = EShopActionType::Exchange;
	ExchangeAction.CostOverride = 0;
	ExchangeAction.Quantity = 1;
	Bandage.Actions.Add(ExchangeAction);

	ShopData.Items.Add(Bandage);
	ShopData.InitialSelectedCategoryId = ConsumableCategory.CategoryId;
	ShopData.InitialSelectedItemId = Bandage.ItemId;

	return ShopData;
}

void InitializeDummyShop(UShopWindow* ShopWindow)
{
	if (!ShopWindow)
	{
		return;
	}

	const FShopDummyShopData DummyData = BuildDummyShopData();
	ShopWindow->InitializeShop(DummyData.ToWindowViewData());
}
```

## 6. Action 비용 규칙

`FShopDummyItemActionData::CostOverride` 값이 `0` 이상이면 해당 값을 사용한다.

`CostOverride`가 `-1`이면 아이템의 `Price` 값을 행동 비용으로 사용한다.

예:

```cpp
BuyAction.CostOverride = -1; // 아이템 Price 사용
ExchangeAction.CostOverride = 0; // 교환 버튼 비용 0으로 표시
```

## 7. 카테고리 아이템 수

`FShopDummyCategoryData`에는 `ItemCount` 필드가 없다. 대신 `FShopDummyShopData::ToWindowViewData()`가 `Items` 배열을 순회해서 같은 `CategoryId`를 가진 아이템 수를 자동 계산한다.

따라서 더미 아이템의 `CategoryId`가 카테고리의 `CategoryId`와 정확히 일치해야 한다.

## 8. 이미지 사용

이미지는 `FSlateBrush`로 전달한다.

더미 테스트 단계에서 이미지가 없으면 `ListIconBrush`와 `DetailImageBrush`를 비워 둬도 된다. 이 경우 기존 Blueprint placeholder가 유지되거나 빈 brush로 표시될 수 있으므로, 실제 이미지 검증은 에디터에서 별도로 확인한다.

## 9. 구매와 교환 요청 연결

더미 데이터로 UI를 띄워도 구매/교환 처리 자체는 외부 시스템 delegate에 연결해야 한다.

```cpp
ShopWindow->OnPurchaseRequested.AddDynamic(this, &UMyShopPresenter::HandlePurchaseRequested);
ShopWindow->OnExchangeRequested.AddDynamic(this, &UMyShopPresenter::HandleExchangeRequested);
```

핸들러 예시:

```cpp
void UMyShopPresenter::HandlePurchaseRequested(FName ShopId, FName ItemId, FName ActionId, int32 Quantity)
{
	UE_LOG(LogTemp, Log, TEXT("Purchase requested. Shop=%s Item=%s Action=%s Quantity=%d"),
		*ShopId.ToString(),
		*ItemId.ToString(),
		*ActionId.ToString(),
		Quantity);
}
```

## 10. Blueprint에서 쓰는 방식

구조체들은 `BlueprintType`이므로 Blueprint 변수나 Data Asset 성격의 객체에서 필드를 구성할 수 있다.

다만 `ToWindowViewData()` 같은 C++ 멤버 함수는 Blueprint 노드로 노출하지 않았다. Blueprint 전용 변환 노드가 필요하면 별도의 `UBlueprintFunctionLibrary`를 추가하는 방식이 적절하다.

권장 흐름:

- C++ 테스트 프레젠터에서 `FShopDummyShopData`를 생성한다.
- `ToWindowViewData()`로 변환한다.
- `UShopWindow::InitializeShop()`에 전달한다.

## 11. 주의사항

- 더미 구조체는 테스트/프로토타입용이다.
- 실제 게임 데이터의 authoritative source로 사용하지 않는다.
- `ItemId`, `CategoryId`, `ActionId`는 배열 인덱스가 아니라 안정적인 `FName` 값을 사용한다.
- `StockQuantity`가 0이어도 UI가 자동으로 구매 금지로 바꾸지는 않는다. 필요하면 `bIsEnabled`나 action의 `bCanExecute`를 외부에서 계산해서 넣는다.
- `UShopWindow`는 거래 성공/실패를 직접 반영하지 않는다. 거래 결과가 나오면 외부 시스템이 `RefreshShop`, `UpdateItem`, `UpdateCurrency` 등을 호출해야 한다.

## 12. 언제 제거하거나 교체해야 하는가

실제 상점 카탈로그, 인벤토리, 저장 데이터, 서버 거래 요청 구조가 정해지면 다음 중 하나를 선택한다.

- 더미 구조체를 테스트 전용으로 유지한다.
- 실제 카탈로그 타입에서 바로 `FShopWindowViewData`를 생성한다.
- 더미 구조체를 삭제하고 실제 shop presenter가 표시 데이터를 조립한다.

UI 위젯은 이미 `FShopWindowViewData`만 알면 동작하므로, 더미 구조체를 제거해도 위젯 코드는 바꿀 필요가 없다.
