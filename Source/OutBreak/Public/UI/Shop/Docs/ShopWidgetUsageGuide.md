# Shop Widget Usage Guide

## 1. Prerequisites

Widget Blueprints must keep these C++ parents:

- `WBP_ShopWindow` -> `UShopWindow`
- `WBP_ShopkeeperPortrait` -> `UShopkeeperPortrait`
- `WBP_UserCurrency` -> `UUserCurrency`
- `WBP_ShopCategory` -> `UShopCategory`
- `WBP_ShopCategoryElement` -> `UShopCategoryElement`
- `WBP_ShopItemList` -> `UShopItemList`
- `WBP_ItemListElement` -> `UItemListElement`
- `WBP_ItemInspector` -> `UShopItemInspector`
- `WBP_ItemStatElement` -> `UItemStatElement`
- `WBP_KeyBindableBtn` -> `UKeyBindableBtn`

## 2. Widget Blueprint Setup

Verify the BindWidget names listed in `ShopWidgetTechnicalDocument.md`.

Set these Blueprint class properties explicitly where possible:

- On `WBP_ShopCategory`: `CategoryElementClass = WBP_ShopCategoryElement`.
- On `WBP_ShopItemList`: `ItemElementClass = WBP_ItemListElement`.
- On `WBP_ItemInspector`: `ItemStatElementClass = WBP_ItemStatElement`.

C++ can infer these from existing template children in the matching `VerticalBox`, but explicit setup is easier to audit.

## 3. Creating Shop Data

Construct display data outside the widget layer:

```cpp
FShopWindowViewData ViewData;
ViewData.ShopId = TEXT("HomeVendor");
ViewData.Shopkeeper.ShopkeeperId = TEXT("Quartermaster");
ViewData.Shopkeeper.RoleText = FText::FromString(TEXT("Quartermaster"));
ViewData.Shopkeeper.DisplayName = FText::FromString(TEXT("Mara"));
ViewData.Shopkeeper.Subtitle = FText::FromString(TEXT("Field supplies"));
ViewData.Currency.Scrap = CurrentScrap;
ViewData.NewStockText = NextRefreshText;

FShopCategoryViewData Category;
Category.CategoryId = TEXT("Weapons");
Category.DisplayName = FText::FromString(TEXT("Weapons"));
Category.ItemCount = 1;
Category.SortOrder = 10;
ViewData.Categories.Add(Category);

FShopItemViewData Item;
Item.ItemId = TEXT("RifleAmmoPack");
Item.CategoryId = Category.CategoryId;
Item.DisplayName = FText::FromString(TEXT("Ammo Pack"));
Item.MetaText = FText::FromString(TEXT("Ammunition"));
Item.Description = FText::FromString(TEXT("A prepared pack from the shop data source."));
Item.Price = 75;
Item.StockQuantity = 3;
Item.OwnedQuantity = OwnedCount;
Item.SortOrder = 10;

FShopItemStatViewData Stat;
Stat.StatId = TEXT("Rounds");
Stat.DisplayName = FText::FromString(TEXT("Rounds"));
Stat.DisplayValue = FText::AsNumber(30);
Item.Stats.Add(Stat);

FShopActionViewData BuyAction;
BuyAction.ActionId = TEXT("Buy");
BuyAction.Label = FText::FromString(TEXT("Buy"));
BuyAction.InputKey = EKeys::E;
BuyAction.Cost = Item.Price;
BuyAction.bCanExecute = CurrentScrap >= Item.Price && Item.StockQuantity > 0;
BuyAction.ActionType = EShopActionType::Purchase;
BuyAction.Quantity = 1;
Item.Actions.Add(BuyAction);

ViewData.Items.Add(Item);
ViewData.InitialSelectedCategoryId = Category.CategoryId;
ViewData.InitialSelectedItemId = Item.ItemId;
```

## 4. Creating And Initializing `UShopWindow`

Create the widget from an existing `PlayerController`, HUD, or UI manager:

```cpp
UShopWindow* ShopWindow = CreateWidget<UShopWindow>(OwningPlayer, ShopWindowClass);
if (!ShopWindow)
{
	return;
}

ShopWindow->OnPurchaseRequested.AddDynamic(this, &UMyShopPresenter::HandlePurchaseRequested);
ShopWindow->OnExchangeRequested.AddDynamic(this, &UMyShopPresenter::HandleExchangeRequested);
ShopWindow->OnShopCloseRequested.AddDynamic(this, &UMyShopPresenter::HandleShopCloseRequested);

ShopWindow->InitializeShop(ViewData);
ShopWindow->AddToViewport();
ShopWindow->SetKeyboardFocus();
```

Use the project's existing UI manager/input mode policy when deciding whether to call `SetInputModeUIOnly`, `SetInputModeGameAndUI`, or cursor visibility APIs.

## 5. Connecting Purchase Requests

Bind a handler on the owning system:

```cpp
void UMyShopPresenter::HandlePurchaseRequested(FName ShopId, FName ItemId, FName ActionId, int32 Quantity)
{
	// Validate with the authoritative shop service.
	// Do not rely on UI-side price, stock, or currency as authority.
	RequestServerPurchase(ShopId, ItemId, ActionId, Quantity);
}
```

The UI does not decrement currency or stock locally.

## 6. Applying Transaction Results

After the authoritative transaction finishes, rebuild or patch the display snapshot:

```cpp
void UMyShopPresenter::HandleTransactionResult(const FShopWindowViewData& ConfirmedViewData)
{
	if (ShopWindow)
	{
		ShopWindow->RefreshShop(ConfirmedViewData);
	}
}
```

For smaller updates, use:

- `UpdateCurrency(NewCurrency)`
- `UpdateItem(ChangedItem)`
- `UpdateItems(AllItems)`
- `UpdateCategories(AllCategories)`

## 7. Selecting Category And Item From Code

```cpp
ShopWindow->SelectCategory(TEXT("Weapons"));
ShopWindow->SelectItem(TEXT("RifleAmmoPack"));
```

Both calls validate the id and return `false` when the target is missing, disabled, or outside the current category.

## 8. Clearing And Closing

When closing a shop:

```cpp
if (ShopWindow)
{
	ShopWindow->ClearShop();
	ShopWindow->RemoveFromParent();
}
```

`NativeDestruct` removes child delegate bindings. External owners should remove their own delegate bindings if the owner outlives the widget.

## 9. Blueprint Usage

The main entry points are BlueprintCallable:

- `InitializeShop`
- `RefreshShop`
- `ClearShop`
- `UpdateCurrency`
- `UpdateItem`
- `UpdateItems`
- `UpdateCategories`
- `SelectCategory`
- `SelectItem`
- `RequestClose`

Blueprint can bind to the `UShopWindow` BlueprintAssignable delegates when a Blueprint presenter owns the shop flow.

## 10. Required Inspector Setup

`WBP_ItemInspector` has only `BTN_0` and `BTN_1`. If an item has more than two actions, only the first two are rendered by the current layout. Add more `UKeyBindableBtn` widgets or a dynamic action container before exposing more actions.

## 11. Troubleshooting

- BindWidget is null: confirm the designer variable name exactly matches the C++ property.
- List is empty: check `CategoryElementClass`/`ItemElementClass` or leave one template child in the matching `VerticalBox` for class inference.
- Click does not fire: confirm the row has a `UButton`; C++ also searches the WidgetTree for the first button.
- Click fires twice: check for duplicate Blueprint graph click forwarding in the WBP.
- Category changes but items do not: verify each item `CategoryId` matches a category `CategoryId`.
- Inspector shows old data: call `RefreshShop`, `UpdateItem`, or `UpdateItems` after the authoritative data changes.
- Image does not show: pass a valid `FSlateBrush` resource; null brushes clear or preserve placeholder state depending on the target widget.
- Entry class not set: set the class in Blueprint or keep a template child in the container.
- Unreal Header Tool delegate error: keep delegate parameters to reflected types such as `FName`, `int32`, and USTRUCTs marked `BlueprintType`.

## 12. Minimal Integration Example

```cpp
void UMyShopPresenter::OpenShop(APlayerController* OwningPlayer)
{
	if (!OwningPlayer || !ShopWindowClass)
	{
		return;
	}

	ShopWindow = CreateWidget<UShopWindow>(OwningPlayer, ShopWindowClass);
	if (!ShopWindow)
	{
		return;
	}

	ShopWindow->OnShopActionRequested.AddDynamic(this, &UMyShopPresenter::HandleAnyShopAction);
	ShopWindow->OnPurchaseRequested.AddDynamic(this, &UMyShopPresenter::HandlePurchaseRequested);
	ShopWindow->OnExchangeRequested.AddDynamic(this, &UMyShopPresenter::HandleExchangeRequested);
	ShopWindow->OnShopCloseRequested.AddDynamic(this, &UMyShopPresenter::HandleShopCloseRequested);

	ShopWindow->InitializeShop(BuildCurrentShopViewData());
	ShopWindow->AddToViewport();
	ShopWindow->SetKeyboardFocus();
}

void UMyShopPresenter::HandleAnyShopAction(FName ShopId, FName ItemId, FName ActionId, int32 Quantity)
{
	SubmitShopActionToAuthoritativeSystem(ShopId, ItemId, ActionId, Quantity);
}
```
