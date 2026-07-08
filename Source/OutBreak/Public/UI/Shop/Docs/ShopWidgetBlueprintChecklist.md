# Shop Widget Blueprint Checklist

Use this checklist in the Unreal Editor after compiling the C++ changes.

- [ ] Widget Blueprint: `WBP_ShopWindow`
- [ ] Parent C++ class: `UShopWindow`
- [ ] Required BindWidget names: `WBP_ShopkeeperPortrait`, `WBP_UserCurrency`, `WBP_ShopCategory`, `WBP_ShopItemList`, `WBP_ItemInspector`
- [ ] Expected widget types: `UShopkeeperPortrait`, `UUserCurrency`, `UShopCategory`, `UShopItemList`, `UShopItemInspector`
- [ ] Entry Class setting: not applicable
- [ ] Clickable Button exists: optional tabs `BTN_TabBuy`, `BTN_TabRequests`, `BTN_TabSell`, `BTN_TabTrade` exist but are not used by C++ shop data flow
- [ ] Selected state widget exists: tab selected visuals exist, category/item selected state is handled in child widgets
- [ ] Default Visibility: all child panels visible when shop opens
- [ ] Input focus setting: call `SetKeyboardFocus` or project UI manager focus policy when opening
- [ ] Test method: initialize with a valid `FShopWindowViewData` and verify category, item, inspector, and action requests

- [ ] Widget Blueprint: `WBP_ShopkeeperPortrait`
- [ ] Parent C++ class: `UShopkeeperPortrait`
- [ ] Required BindWidget names: `TXT_Role`, `TXT_ShopkeeperName`, `TXT_ShopkeeperSubtitle`, `IMG_ShopkeeperPortrait_Placeholder`
- [ ] Expected widget types: `UTextBlock`, `UTextBlock`, `UTextBlock`, `UBorder`
- [ ] Entry Class setting: not applicable
- [ ] Clickable Button exists: not required
- [ ] Selected state widget exists: not required
- [ ] Default Visibility: portrait and text widgets visible
- [ ] Input focus setting: not required
- [ ] Test method: call `SetShopkeeperData` and `ClearShopkeeperData`

- [ ] Widget Blueprint: `WBP_UserCurrency`
- [ ] Parent C++ class: `UUserCurrency`
- [ ] Required BindWidget names: `TXT_ScrapValue`
- [ ] Expected widget types: `UTextBlock`
- [ ] Entry Class setting: not applicable
- [ ] Clickable Button exists: not required
- [ ] Selected state widget exists: not required
- [ ] Default Visibility: currency panel visible
- [ ] Input focus setting: not required
- [ ] Test method: call `SetCurrencyData`

- [ ] Widget Blueprint: `WBP_ShopCategory`
- [ ] Parent C++ class: `UShopCategory`
- [ ] Required BindWidget names: `VBX_CategoryList`, `TXT_NewStockTime`
- [ ] Expected widget types: `UVerticalBox`, `UTextBlock`
- [ ] Entry Class setting: set `CategoryElementClass` to `WBP_ShopCategoryElement`
- [ ] Clickable Button exists: category buttons live in `WBP_ShopCategoryElement`
- [ ] Selected state widget exists: child row should provide selected visual if needed
- [ ] Default Visibility: category list visible
- [ ] Input focus setting: not required
- [ ] Test method: call `SetCategories` with at least two categories and click rows

- [ ] Widget Blueprint: `WBP_ShopCategoryElement`
- [ ] Parent C++ class: `UShopCategoryElement`
- [ ] Required BindWidget names: `TXT_Category_Name`, `TXT_Category_Count`
- [ ] Expected widget types: `UTextBlock`, `UTextBlock`
- [ ] Entry Class setting: not applicable
- [ ] Clickable Button exists: `BTN_Category_All` is optional; C++ also searches the WidgetTree for a `UButton`
- [ ] Selected state widget exists: optional `BG_Category_All_Selected` is not present in the observed asset strings; add or verify another selected visual if needed
- [ ] Default Visibility: text visible, selected visual hidden by default
- [ ] Input focus setting: not required
- [ ] Test method: click row and verify `OnCategorySelected`

- [ ] Widget Blueprint: `WBP_ShopItemList`
- [ ] Parent C++ class: `UShopItemList`
- [ ] Required BindWidget names: `VBX_ItemList`
- [ ] Expected widget types: `UVerticalBox`
- [ ] Entry Class setting: set `ItemElementClass` to `WBP_ItemListElement`
- [ ] Clickable Button exists: item buttons live in `WBP_ItemListElement`
- [ ] Selected state widget exists: child row should provide selected visual if needed
- [ ] Default Visibility: item list visible
- [ ] Input focus setting: not required
- [ ] Test method: call `SetItems` with matching category-filtered data

- [ ] Widget Blueprint: `WBP_ItemListElement`
- [ ] Parent C++ class: `UItemListElement`
- [ ] Required BindWidget names: `TXT_ItemName`, `TXT_ItemPrice`, `TXT_ItemQty`, `TXT_ItemMeta`, `IMG_ItemThumb_Placeholder`
- [ ] Expected widget types: `UTextBlock`, `UTextBlock`, `UTextBlock`, `UTextBlock`, `UBorder`
- [ ] Entry Class setting: not applicable
- [ ] Clickable Button exists: optional `BTN_ItemRow` or `BTN_ItemRow_0`; C++ also searches the WidgetTree for a `UButton`
- [ ] Selected state widget exists: optional `BG_ItemRow_0_Outline` is used when present
- [ ] Default Visibility: selected outline hidden by default
- [ ] Input focus setting: not required
- [ ] Test method: click row and verify `OnItemSelected`

- [ ] Widget Blueprint: `WBP_ItemInspector`
- [ ] Parent C++ class: `UShopItemInspector`
- [ ] Required BindWidget names: `TXT_InspectorTitle`, `TXT_InspectorQty`, `TXT_InspectorMeta`, `IMG_InspectorPreview_Placeholder`, `TXT_InspectorDescription`, `VBX_ItemStatList`, `TXT_InspectorPriceValue`, `BTN_0`, `BTN_1`
- [ ] Expected widget types: `UTextBlock`, `UTextBlock`, `UTextBlock`, `UBorder`, `UTextBlock`, `UVerticalBox`, `UTextBlock`, `UKeyBindableBtn`, `UKeyBindableBtn`
- [ ] Entry Class setting: set `ItemStatElementClass` to `WBP_ItemStatElement`
- [ ] Clickable Button exists: action buttons are inside `BTN_0` and `BTN_1`
- [ ] Selected state widget exists: not required
- [ ] Default Visibility: action buttons collapse when no matching action data exists
- [ ] Input focus setting: focus can remain on `WBP_ShopWindow`; action key routing exists at window level
- [ ] Test method: select item with one and two actions, then click/key each action

- [ ] Widget Blueprint: `WBP_ItemStatElement`
- [ ] Parent C++ class: `UItemStatElement`
- [ ] Required BindWidget names: `TXT_StatLabel`, `TXT_StatValue`
- [ ] Expected widget types: `UTextBlock`, `UTextBlock`
- [ ] Entry Class setting: not applicable
- [ ] Clickable Button exists: not required
- [ ] Selected state widget exists: not required
- [ ] Default Visibility: stat row visible
- [ ] Input focus setting: not required
- [ ] Test method: call `SetStatData`

- [ ] Widget Blueprint: `WBP_KeyBindableBtn`
- [ ] Parent C++ class: `UKeyBindableBtn`
- [ ] Required BindWidget names: `TXT_BindedKey`, `TXT_Action`
- [ ] Expected widget types: text widgets; observed asset also contains a `Button`
- [ ] Entry Class setting: not applicable
- [ ] Clickable Button exists: optional `Button`; C++ also searches the WidgetTree for a `UButton`
- [ ] Selected state widget exists: not required
- [ ] Default Visibility: collapsed when cleared, visible when action data exists
- [ ] Input focus setting: window-level key handling is available; button-level key handling works when focused
- [ ] Test method: call `SetActionData`, click the button, press its `InputKey`, and verify `OnActionTriggered`

## Known Manual Follow-ups

- [ ] Confirm `CategoryElementClass`, `ItemElementClass`, and `ItemStatElementClass` in the editor after C++ recompilation.
- [ ] Confirm `WBP_ItemInspector` is intentionally named without `Shop`; do not rename unless all references are updated.
- [ ] Add or verify category selected-state visual if designers require visible category selection beyond internal state.
- [ ] Add more action button widgets or a dynamic action container if more than two item actions must be displayed.
- [ ] Verify focus/input mode in the existing PlayerController or UI manager when opening the shop.
