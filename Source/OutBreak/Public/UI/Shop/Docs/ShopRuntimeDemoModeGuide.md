# 상점 런타임 데모 모드 사용 가이드

## 1. 추가된 클래스

상점 UI를 실제 실행 중에 테스트하기 위해 아래 C++ 클래스를 추가했다.

```text
Source/OutBreak/Public/UI/Shop/Demo/OBShopDemoGameMode.h
Source/OutBreak/Private/UI/Shop/Demo/OBShopDemoGameMode.cpp

Source/OutBreak/Public/UI/Shop/Demo/OBShopDemoPlayerController.h
Source/OutBreak/Private/UI/Shop/Demo/OBShopDemoPlayerController.cpp

Source/OutBreak/Public/UI/Shop/Demo/OBShopDemoPawn.h
Source/OutBreak/Private/UI/Shop/Demo/OBShopDemoPawn.cpp
```

역할:

- `AOBShopDemoGameMode`: 데모용 Pawn과 PlayerController를 지정한다.
- `AOBShopDemoPlayerController`: 상점 위젯을 열고, 특정 키 입력으로 더미 아이템을 랜덤 추가한다.
- `AOBShopDemoPawn`: UI 테스트용 고정 카메라 Pawn이다.

## 2. 실행 방법

에디터에서 테스트할 레벨을 연다.

`World Settings`에서 `GameMode Override`를 다음 클래스로 지정한다.

```text
OBShopDemoGameMode
```

이후 Play를 누르면 `WBP_ShopWindow`가 자동으로 열린다.

기본 입력:

```text
R 키: 랜덤 더미 아이템 1개 추가
E 키: 선택된 아이템의 Buy action 요청
Q 키: 선택된 아이템의 Exchange action 요청
Esc 키: 상점 닫기
```

## 3. 동작 흐름

게임 시작:

```text
AOBShopDemoGameMode
  -> AOBShopDemoPlayerController 생성
  -> WBP_ShopWindow 생성
  -> FShopDummyShopData 초기화
  -> UShopWindow::InitializeShop()
```

`R` 키 입력:

```text
AOBShopDemoPlayerController::HandleAddRandomItemPressed()
  -> AddRandomItemToShop()
  -> BuildRandomItemData()
  -> ActiveShopData.Items.Add()
  -> UShopWindow::RefreshShop()
  -> UShopWindow::SelectCategory()
  -> UShopWindow::SelectItem()
```

상점 행동 버튼 입력:

```text
UKeyBindableBtn
  -> UShopItemInspector
  -> UShopWindow::OnShopActionRequested
  -> AOBShopDemoPlayerController::HandleShopActionRequested()
```

데모 컨트롤러는 요청을 화면 로그와 `UE_LOG`로만 출력한다. 실제 재화 차감이나 아이템 지급은 하지 않는다.

## 4. 기본 위젯 클래스

`AOBShopDemoPlayerController`는 생성자에서 아래 위젯을 기본으로 찾는다.

```text
/Game/UI/Shop/WBP_ShopWindow
```

해당 에셋이 이동되었거나 이름이 바뀌었다면, 데모 컨트롤러 Blueprint 자식을 만들고 `ShopWindowClass`를 직접 지정한다.

## 5. 키 변경 방법

기본 랜덤 추가 키는 `R`이다.

다른 키를 쓰고 싶으면 `AOBShopDemoPlayerController`의 Blueprint 자식을 만들고 아래 프로퍼티를 수정한다.

```text
AddRandomItemKey
```

예:

```text
F5 키로 변경
```

## 6. 시작 데이터

게임 시작 시 상점에는 카테고리만 존재한다.

기본 카테고리:

- `Weapons`
- `Consumables`
- `Resources`

`R` 키를 누를 때마다 아래 템플릿 중 하나가 랜덤으로 생성된다.

- `Demo Rifle Parts`
- `Demo Bandage`
- `Demo Fuel Cell`
- `Demo Ammo Pack`
- `Demo Tool Kit`

각 아이템은 생성 번호를 붙여 고유한 `ItemId`를 가진다.

예:

```text
DemoItem_001
DemoItem_002
DemoItem_003
```

## 7. 거래 처리는 어디까지 되는가

이 데모는 UI 연결 확인용이다.

처리하는 것:

- 상점 위젯 생성
- 더미 상점 데이터 초기화
- 키 입력으로 랜덤 아이템 추가
- 카테고리 개수 자동 갱신
- 새로 추가된 아이템 자동 선택
- Buy/Exchange 요청 로그 출력

처리하지 않는 것:

- 실제 인벤토리 지급
- 실제 재화 차감
- 실제 상점 재고 차감
- 서버 RPC
- 저장 데이터 변경

## 8. 프로젝트 기본 GameMode로 지정하고 싶을 때

기존 게임 흐름에 영향을 주지 않기 위해 이번 작업에서는 `DefaultEngine.ini`의 기본 GameMode를 바꾸지 않았다.

전역 기본값으로 쓰고 싶을 때만 `Config/DefaultEngine.ini`의 `GameMapsSettings`에 아래 설정을 추가하거나 수정한다.

```ini
[/Script/EngineSettings.GameMapsSettings]
GlobalDefaultGameMode=/Script/OutBreak.OBShopDemoGameMode
```

일반 개발 중에는 레벨별 `World Settings -> GameMode Override` 방식이 안전하다.

## 9. 문제 해결

상점 위젯이 안 뜬다:

- `WBP_ShopWindow` 경로가 `/Game/UI/Shop/WBP_ShopWindow`인지 확인한다.
- `OBShopDemoGameMode`가 현재 레벨의 GameMode Override로 설정되어 있는지 확인한다.

`R` 키를 눌러도 추가되지 않는다:

- Play 중 화면에 `Shop demo ready` 메시지가 떴는지 확인한다.
- 데모 컨트롤러 Blueprint 자식을 쓴다면 `AddRandomItemKey`가 원하는 키인지 확인한다.

아이템은 추가되지만 클릭이 안 된다:

- `WBP_ItemListElement` 안에 클릭 가능한 `Button`이 있는지 확인한다.
- C++은 `BTN_ItemRow`, `BTN_ItemRow_0`, 또는 WidgetTree의 첫 `UButton`을 찾는다.

Buy/Exchange가 실제로 처리되지 않는다:

- 정상이다. 데모는 요청 로그만 출력한다.
- 실제 처리하려면 `UShopWindow`의 delegate를 상점 시스템이나 서버 요청 코드에 연결해야 한다.
