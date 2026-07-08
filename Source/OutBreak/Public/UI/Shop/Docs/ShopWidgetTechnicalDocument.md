# Shop Widget Technical Document

## 1. Purpose

The shop UI now accepts a single `FShopWindowViewData` snapshot from an external shop system and renders shopkeeper, currency, category, item list, inspector, stat, and action button state without owning gameplay data. The widgets forward user intent through delegates and do not mutate inventory, currency, stock, save data, or server authority state.

## 2. Existing Structure Analysis

The target C++ classes already existed under `Source/OutBreak/Public/UI/Shop` and `Source/OutBreak/Private/UI/Shop`, but most implementations only called `Super::NativeConstruct()`.

Observed Widget Blueprint assets:

- `Content/UI/Shop/WBP_ShopWindow.uasset` parent: `/Script/OutBreak.ShopWindow`
- `Content/UI/Shop/WBP_ShopkeeperPortrait.uasset` parent: `/Script/OutBreak.ShopkeeperPortrait`
- `Content/UI/Shop/WBP_UserCurrency.uasset` parent: `/Script/OutBreak.UserCurrency`
- `Content/UI/Shop/WBP_ShopCategory.uasset` parent: `/Script/OutBreak.ShopCategory`
- `Content/UI/Shop/WBP_ShopCategoryElement.uasset` parent: `/Script/OutBreak.ShopCategoryElement`
- `Content/UI/Shop/WBP_ShopItemList.uasset` parent: `/Script/OutBreak.ShopItemList`
- `Content/UI/Shop/WBP_ItemListElement.uasset` parent: `/Script/OutBreak.ItemListElement`
- `Content/UI/Shop/WBP_ItemInspector.uasset` parent: `/Script/OutBreak.ShopItemInspector`
- `Content/UI/Shop/WBP_ItemStatElement.uasset` parent: `/Script/OutBreak.ItemStatElement`
- `Content/UI/Shop/WBP_KeyBindableBtn.uasset` parent: `/Script/OutBreak.KeyBindableBtn`

The request name `WBP_ShopItemInspector` does not match the actual asset string; the existing asset is `WBP_ItemInspector`.

`OutBreak.Build.cs` already includes `UMG`, `Slate`, `SlateCore`, `InputCore`, and `EnhancedInput`. `DefaultGame.ini` contains Common UI settings, but the project module does not list `CommonUI`, so the shop implementation does not depend on Common UI.

## 3. Final Class Responsibilities

- `UShopWindow`: owns the current display snapshot, selection ids, lookup maps, filtering, child widget coordination, and outgoing shop request delegates.
- `UShopkeeperPortrait`: renders shopkeeper role, name, subtitle, and portrait brush.
- `UUserCurrency`: renders user scrap amount.
- `UShopCategory`: dynamically rebuilds category entries in `VBX_CategoryList`.
- `UShopCategoryElement`: renders one category row and forwards category selection.
- `UShopItemList`: dynamically rebuilds item entries in `VBX_ItemList`.
- `UItemListElement`: renders one item row and forwards item selection.
- `UShopItemInspector`: renders one selected item, stat entries, and up to two pre-placed action buttons.
- `UItemStatElement`: renders one stat label/value row.
- `UKeyBindableBtn`: renders one action label/key hint and forwards action id on click or matching key.

## 4. Data Structures

`ShopWidgetTypes.h` defines the UI-only data surface:

- `FShopkeeperViewData`: shopkeeper id, role, display name, subtitle, portrait brush, reputation fields, note text.
- `FUserCurrencyViewData`: scrap amount.
- `FShopCategoryViewData`: category id, display name, item count, icon brush, enabled flag, sort order.
- `FShopItemStatViewData`: stat id, display name, display value, sort order.
- `FShopActionViewData`: action id, label, key display, `FKey`, cost, enabled state, disabled reason, action type, quantity.
- `FShopItemViewData`: item id, category id, display name, meta, description, brushes, price, stock, owned quantity, enabled flag, sort order, stats, actions.
- `FShopWindowViewData`: shop id, shopkeeper, currency, categories, items, initial category/item ids, new stock text.

Action type is represented by `EShopActionType::{Generic, Purchase, Exchange}`.

## 5. Data Ownership

External gameplay systems own authoritative shop data, inventory, currency, stock, and transaction results. `UShopWindow` stores only the latest display snapshot and lightweight lookup maps:

- `CategoryIndexById`
- `ItemIndexById`
- `DisplayedItems`
- selected category and item ids

Child widgets store only their own row/item/action display data.

## 6. Initialization Flow

External shop system -> `FShopWindowViewData` -> `UShopWindow::InitializeShop` -> child widgets:

- `UShopkeeperPortrait::SetShopkeeperData`
- `UUserCurrency::SetCurrencyData`
- `UShopCategory::SetCategories`
- `UShopItemList::SetItems`
- `UShopItemInspector::SetItemData`

`UShopWindow` rebuilds indexes, resolves an initial category, filters items for that category, resolves an initial item, then updates the inspector.

## 7. Category Selection Flow

User clicks category entry -> `UShopCategoryElement::OnCategorySelected` -> `UShopCategory::OnCategorySelected` -> `UShopWindow::HandleCategorySelected`.

`UShopWindow` validates the id, updates category selection visuals, rebuilds the displayed item array, preserves the previous item if it still belongs to the category, otherwise selects the first enabled item. It broadcasts `OnCategorySelectionChanged`; if the item selection changes, it also broadcasts `OnItemSelectionChanged`.

## 8. Item Selection Flow

User clicks item row -> `UItemListElement::OnItemSelected` -> `UShopItemList::OnItemSelected` -> `UShopWindow::HandleItemSelected`.

`UShopWindow` validates that the item exists, is enabled, and belongs to the selected category, then updates row selection and inspector data.

## 9. Action Request Flow

User clicks/keys action -> `UKeyBindableBtn::OnActionTriggered` -> `UShopItemInspector::OnActionTriggered` -> `UShopWindow::HandleActionTriggered`.

The window validates selected item and action, then broadcasts:

- `OnShopActionRequested` for every valid action.
- `OnPurchaseRequested` when `ActionType == Purchase`.
- `OnExchangeRequested` when `ActionType == Exchange`.

The request payload is shop id, selected item id, action id, and quantity. No local currency or stock mutation occurs.

## 10. Transaction Result Refresh Flow

After a transaction succeeds or fails, the external shop system should update UI by calling one of:

- `RefreshShop` with a complete new `FShopWindowViewData`.
- `UpdateCurrency` for currency-only changes.
- `UpdateItem` for one item.
- `UpdateItems` for a full item list.
- `UpdateCategories` when category counts or enabled state changed.

The UI does not roll back speculative state because it does not apply speculative mutations.

## 11. Delegate List

- `FShopCategorySelectedSignature`: child category id.
- `FShopItemSelectedSignature`: child item id.
- `FShopActionTriggeredSignature`: child action id.
- `FShopWindowCategorySelectionChangedSignature`: shop id, category id.
- `FShopWindowItemSelectionChangedSignature`: shop id, item id.
- `FShopWindowActionRequestedSignature`: shop id, item id, action id, quantity.
- `FShopWindowCloseRequestedSignature`: shop id.

## 12. Widget Blueprint Contract

Required BindWidget names currently used by C++:

- `WBP_ShopWindow`: `WBP_ShopkeeperPortrait`, `WBP_UserCurrency`, `WBP_ShopCategory`, `WBP_ShopItemList`, `WBP_ItemInspector`.
- `WBP_ShopkeeperPortrait`: `TXT_Role`, `TXT_ShopkeeperName`, `TXT_ShopkeeperSubtitle`, `IMG_ShopkeeperPortrait_Placeholder`.
- `WBP_UserCurrency`: `TXT_ScrapValue`.
- `WBP_ShopCategory`: `VBX_CategoryList`, `TXT_NewStockTime`.
- `WBP_ShopCategoryElement`: `TXT_Category_Name`, `TXT_Category_Count`; optional `BTN_Category_All`, `BG_Category_All_Selected`.
- `WBP_ShopItemList`: `VBX_ItemList`.
- `WBP_ItemListElement`: `TXT_ItemName`, `TXT_ItemPrice`, `TXT_ItemQty`, `TXT_ItemMeta`, `IMG_ItemThumb_Placeholder`; optional `BTN_ItemRow`, `BTN_ItemRow_0`, `BG_ItemRow_0_Outline`.
- `WBP_ItemInspector`: `TXT_InspectorTitle`, `TXT_InspectorQty`, `TXT_InspectorMeta`, `IMG_InspectorPreview_Placeholder`, `TXT_InspectorDescription`, `VBX_ItemStatList`, `TXT_InspectorPriceValue`, `BTN_0`, `BTN_1`.
- `WBP_ItemStatElement`: `TXT_StatLabel`, `TXT_StatValue`.
- `WBP_KeyBindableBtn`: `TXT_BindedKey`, `TXT_Action`; optional `Button`.

Entry classes:

- `UShopCategory::CategoryElementClass`
- `UShopItemList::ItemElementClass`
- `UShopItemInspector::ItemStatElementClass`

These properties can be set in Blueprint. If omitted, the C++ attempts to infer the class from an existing template child in the matching `VerticalBox` before clearing children.

## 13. Exception Handling

Invalid ids, empty arrays, and missing optional widgets are handled without crashing. Missing required containers or entry classes use `ensureMsgf` because they represent setup errors. Duplicate category/item ids use `ensureMsgf` and skip later duplicates in lookup maps.

## 14. Performance Characteristics

No tick-based refresh is used. Lists rebuild only when new data is injected. `UShopWindow` uses `TMap<FName, int32>` lookup caches for category and item ids. Filtering creates a separate `DisplayedItems` array without mutating the source snapshot.

## 15. Extension Points

- Sell tab: add new `EShopActionType` and action data.
- Transaction log: bind to `OnShopActionRequested`.
- Quantity selector: add quantity input in `UShopItemInspector` and update request quantity.
- Sorting/search/rarity filters: extend `BuildItemsForCategory`.
- Server transaction requests: bind external subsystem to window delegates.
- Async icon loading: update `FSlateBrush` fields and call `UpdateItem` or `RefreshShop`.
- Virtualized lists: replace `VerticalBox` rebuilds in `UShopCategory` and `UShopItemList`.
- Common UI integration: replace `UKeyBindableBtn` internals after adding the module dependency.

## 16. Changed Files

- Added `Source/OutBreak/Public/UI/Shop/ShopWidgetTypes.h`: display structs and delegate signatures.
- Updated `ShopWindow.h/.cpp`: snapshot ownership, selection, filtering, outgoing delegates.
- Updated `ShopkeeperPortrait.h/.cpp`: shopkeeper data render/clear.
- Updated `UserCurrency.h/.cpp`: currency render/clear.
- Updated `ShopCategory.h/.cpp`: dynamic category list and delegate forwarding.
- Updated `ShopCategoryElement.h/.cpp`: row render, selected state, click handling.
- Updated `ShopItemList.h/.cpp`: dynamic item list and delegate forwarding.
- Updated `ItemListElement.h/.cpp`: row render, selected state, click handling.
- Updated `ShopItemInspector.h/.cpp`: inspector render, stat list, action buttons.
- Updated `ItemStatElement.h/.cpp`: stat render/clear.
- Updated `KeyBindableBtn.h/.cpp`: action render, click/key forwarding.
- Added shop docs under `Source/OutBreak/Public/UI/Shop/Docs`.

## 17. Build And Verification Result

Command:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' OutBreakEditor Win64 Development -Project='C:\Users\Admin\Documents\Unreal Projects\OutBreak\OutBreak.uproject' -WaitMutex -NoHotReloadFromIDE
```

Result: succeeded.

Verification covered Unreal Header Tool generation, reflected structs/delegates, includes, dynamic delegate bindings, and C++ compile/link for the `OutBreakEditor` target.
