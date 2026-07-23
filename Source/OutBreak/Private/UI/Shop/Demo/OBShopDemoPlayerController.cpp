// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Shop/Demo/OBShopDemoPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "UObject/ConstructorHelpers.h"
#include "UI/Shop/ShopWindow.h"

#define LOCTEXT_NAMESPACE "OBShopDemoPlayerController"

AOBShopDemoPlayerController::AOBShopDemoPlayerController()
{
	static ConstructorHelpers::FClassFinder<UShopWindow> ShopWindowFinder(TEXT("/Game/UI/Shop/WBP_ShopWindow"));
	if (ShopWindowFinder.Succeeded())
	{
		ShopWindowClass = ShopWindowFinder.Class;
	}
}

void AOBShopDemoPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	ActiveShopData = BuildInitialDummyShopData();

	if (bOpenShopOnBeginPlay)
	{
		CreateShopWindow();
	}

	NotifyDemoMessage(FString::Printf(TEXT("Shop demo ready. Press %s to add a random item."), *AddRandomItemKey.GetDisplayName().ToString()));
}

void AOBShopDemoPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindShopWindowDelegates();
	ShopWindow = nullptr;

	Super::EndPlay(EndPlayReason);
}

void AOBShopDemoPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (InputComponent)
	{
		InputComponent->BindKey(AddRandomItemKey, IE_Pressed, this, &AOBShopDemoPlayerController::HandleAddRandomItemPressed);
	}
}

void AOBShopDemoPlayerController::AddRandomItemToShop()
{
	if (!EnsureShopWindow())
	{
		return;
	}

	FShopDummyItemData NewItem = BuildRandomItemData();
	ActiveShopData.InitialSelectedCategoryId = NewItem.CategoryId;
	ActiveShopData.InitialSelectedItemId = NewItem.ItemId;
	ActiveShopData.Items.Add(NewItem);

	ShopWindow->RefreshShop(ActiveShopData.ToWindowViewData());
	ShopWindow->SelectCategory(NewItem.CategoryId);
	ShopWindow->SelectItem(NewItem.ItemId);
	ApplyShopInputMode();

	NotifyDemoMessage(FString::Printf(TEXT("Added shop item: %s"), *NewItem.DisplayName.ToString()));
}

void AOBShopDemoPlayerController::ResetDummyShop()
{
	ActiveShopData = BuildInitialDummyShopData();
	GeneratedItemSerial = 0;

	if (EnsureShopWindow())
	{
		ShopWindow->InitializeShop(ActiveShopData.ToWindowViewData());
		ApplyShopInputMode();
	}
}

void AOBShopDemoPlayerController::HandleShopActionRequested(FName ShopId, FName ItemId, FName ActionId, int32 Quantity)
{
	NotifyDemoMessage(FString::Printf(
		TEXT("Shop action requested. Shop=%s Item=%s Action=%s Quantity=%d"),
		*ShopId.ToString(),
		*ItemId.ToString(),
		*ActionId.ToString(),
		Quantity));
}

void AOBShopDemoPlayerController::HandleShopCloseRequested(FName ShopId)
{
	UnbindShopWindowDelegates();

	if (ShopWindow)
	{
		ShopWindow->RemoveFromParent();
		ShopWindow = nullptr;
	}

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	SetShowMouseCursor(false);

	NotifyDemoMessage(FString::Printf(TEXT("Closed shop: %s"), *ShopId.ToString()));
}

void AOBShopDemoPlayerController::HandleAddRandomItemPressed()
{
	AddRandomItemToShop();
}

bool AOBShopDemoPlayerController::EnsureShopWindow()
{
	if (!ShopWindow)
	{
		CreateShopWindow();
	}

	return ShopWindow != nullptr;
}

void AOBShopDemoPlayerController::CreateShopWindow()
{
	if (ShopWindow || !IsLocalController())
	{
		return;
	}

	if (!ShopWindowClass)
	{
		NotifyDemoMessage(TEXT("ShopWindowClass is not set. Assign WBP_ShopWindow on the demo controller."));
		return;
	}

	ShopWindow = CreateWidget<UShopWindow>(this, ShopWindowClass);
	if (!ShopWindow)
	{
		NotifyDemoMessage(TEXT("Failed to create shop window."));
		return;
	}

	BindShopWindowDelegates();
	ShopWindow->InitializeShop(ActiveShopData.ToWindowViewData());
	ShopWindow->AddToViewport(ShopViewportZOrder);
	ApplyShopInputMode();
}

void AOBShopDemoPlayerController::BindShopWindowDelegates()
{
	if (!ShopWindow)
	{
		return;
	}

	ShopWindow->OnShopActionRequested.RemoveDynamic(this, &AOBShopDemoPlayerController::HandleShopActionRequested);
	ShopWindow->OnShopCloseRequested.RemoveDynamic(this, &AOBShopDemoPlayerController::HandleShopCloseRequested);

	ShopWindow->OnShopActionRequested.AddDynamic(this, &AOBShopDemoPlayerController::HandleShopActionRequested);
	ShopWindow->OnShopCloseRequested.AddDynamic(this, &AOBShopDemoPlayerController::HandleShopCloseRequested);
}

void AOBShopDemoPlayerController::UnbindShopWindowDelegates()
{
	if (!ShopWindow)
	{
		return;
	}

	ShopWindow->OnShopActionRequested.RemoveDynamic(this, &AOBShopDemoPlayerController::HandleShopActionRequested);
	ShopWindow->OnShopCloseRequested.RemoveDynamic(this, &AOBShopDemoPlayerController::HandleShopCloseRequested);
}

void AOBShopDemoPlayerController::ApplyShopInputMode()
{
	if (!ShopWindow)
	{
		return;
	}

	ShopWindow->SetIsFocusable(true);

	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(ShopWindow->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	SetShowMouseCursor(true);
}

FShopDummyShopData AOBShopDemoPlayerController::BuildInitialDummyShopData() const
{
	FShopDummyShopData ShopData;
	ShopData.ShopId = TEXT("ShopDemo");
	ShopData.Currency.Scrap = InitialScrap;
	ShopData.NewStockText = LOCTEXT("DemoNewStockText", "Press R to add stock");

	ShopData.Shopkeeper.ShopkeeperId = TEXT("DemoQuartermaster");
	ShopData.Shopkeeper.RoleText = LOCTEXT("DemoShopkeeperRole", "Quartermaster");
	ShopData.Shopkeeper.DisplayName = LOCTEXT("DemoShopkeeperName", "Demo Vendor");
	ShopData.Shopkeeper.Subtitle = LOCTEXT("DemoShopkeeperSubtitle", "Runtime shop UI test");
	ShopData.Shopkeeper.NoteText = LOCTEXT("DemoShopkeeperNote", "Dummy data only");

	FShopDummyCategoryData WeaponsCategory;
	WeaponsCategory.CategoryId = TEXT("Weapons");
	WeaponsCategory.DisplayName = LOCTEXT("DemoWeaponsCategory", "Weapons");
	WeaponsCategory.SortOrder = 10;
	ShopData.Categories.Add(WeaponsCategory);

	FShopDummyCategoryData ConsumablesCategory;
	ConsumablesCategory.CategoryId = TEXT("Consumables");
	ConsumablesCategory.DisplayName = LOCTEXT("DemoConsumablesCategory", "Consumables");
	ConsumablesCategory.SortOrder = 20;
	ShopData.Categories.Add(ConsumablesCategory);

	FShopDummyCategoryData ResourcesCategory;
	ResourcesCategory.CategoryId = TEXT("Resources");
	ResourcesCategory.DisplayName = LOCTEXT("DemoResourcesCategory", "Resources");
	ResourcesCategory.SortOrder = 30;
	ShopData.Categories.Add(ResourcesCategory);

	ShopData.InitialSelectedCategoryId = WeaponsCategory.CategoryId;
	return ShopData;
}

FShopDummyItemData AOBShopDemoPlayerController::BuildRandomItemData()
{
	++GeneratedItemSerial;

	const int32 TemplateIndex = FMath::RandRange(0, 4);
	const int32 Price = FMath::RandRange(25, 180);

	FShopDummyItemData Item;
	Item.ItemId = FName(*FString::Printf(TEXT("DemoItem_%03d"), GeneratedItemSerial));
	Item.Price = Price;
	Item.StockQuantity = FMath::RandRange(1, 9);
	Item.OwnedQuantity = FMath::RandRange(0, 3);
	Item.SortOrder = GeneratedItemSerial;
	Item.bIsEnabled = true;

	switch (TemplateIndex)
	{
	case 0:
		Item.CategoryId = TEXT("Weapons");
		Item.DisplayName = FText::FromString(FString::Printf(TEXT("Demo Rifle Parts %03d"), GeneratedItemSerial));
		Item.MetaText = LOCTEXT("DemoWeaponMeta", "Weapon Parts");
		Item.Description = LOCTEXT("DemoWeaponDescription", "Generated weapon component for shop UI testing.");
		break;
	case 1:
		Item.CategoryId = TEXT("Consumables");
		Item.DisplayName = FText::FromString(FString::Printf(TEXT("Demo Bandage %03d"), GeneratedItemSerial));
		Item.MetaText = LOCTEXT("DemoBandageMeta", "Consumable");
		Item.Description = LOCTEXT("DemoBandageDescription", "Generated healing supply for shop UI testing.");
		break;
	case 2:
		Item.CategoryId = TEXT("Resources");
		Item.DisplayName = FText::FromString(FString::Printf(TEXT("Demo Fuel Cell %03d"), GeneratedItemSerial));
		Item.MetaText = LOCTEXT("DemoFuelMeta", "Resource");
		Item.Description = LOCTEXT("DemoFuelDescription", "Generated resource item for shop UI testing.");
		break;
	case 3:
		Item.CategoryId = TEXT("Weapons");
		Item.DisplayName = FText::FromString(FString::Printf(TEXT("Demo Ammo Pack %03d"), GeneratedItemSerial));
		Item.MetaText = LOCTEXT("DemoAmmoMeta", "Ammunition");
		Item.Description = LOCTEXT("DemoAmmoDescription", "Generated ammunition pack for shop UI testing.");
		break;
	default:
		Item.CategoryId = TEXT("Resources");
		Item.DisplayName = FText::FromString(FString::Printf(TEXT("Demo Tool Kit %03d"), GeneratedItemSerial));
		Item.MetaText = LOCTEXT("DemoToolMeta", "Tool");
		Item.Description = LOCTEXT("DemoToolDescription", "Generated utility item for shop UI testing.");
		break;
	}

	FShopDummyItemStatData PriceStat;
	PriceStat.StatId = TEXT("Price");
	PriceStat.DisplayName = LOCTEXT("DemoPriceStatLabel", "Price");
	PriceStat.DisplayValue = FText::AsNumber(Item.Price);
	PriceStat.SortOrder = 10;
	Item.Stats.Add(PriceStat);

	FShopDummyItemStatData StockStat;
	StockStat.StatId = TEXT("Stock");
	StockStat.DisplayName = LOCTEXT("DemoStockStatLabel", "Stock");
	StockStat.DisplayValue = FText::AsNumber(Item.StockQuantity);
	StockStat.SortOrder = 20;
	Item.Stats.Add(StockStat);

	FShopDummyItemActionData BuyAction;
	BuyAction.ActionId = TEXT("Buy");
	BuyAction.Label = LOCTEXT("DemoBuyAction", "Buy");
	BuyAction.InputDisplayText = LOCTEXT("DemoBuyKey", "E");
	BuyAction.InputKey = EKeys::E;
	BuyAction.ActionType = EShopActionType::Purchase;
	BuyAction.bCanExecute = ActiveShopData.Currency.Scrap >= Item.Price && Item.StockQuantity > 0;
	BuyAction.Quantity = 1;
	Item.Actions.Add(BuyAction);

	FShopDummyItemActionData ExchangeAction;
	ExchangeAction.ActionId = TEXT("Exchange");
	ExchangeAction.Label = LOCTEXT("DemoExchangeAction", "Exchange");
	ExchangeAction.InputDisplayText = LOCTEXT("DemoExchangeKey", "Q");
	ExchangeAction.InputKey = EKeys::Q;
	ExchangeAction.ActionType = EShopActionType::Exchange;
	ExchangeAction.CostOverride = 0;
	ExchangeAction.bCanExecute = true;
	ExchangeAction.Quantity = 1;
	Item.Actions.Add(ExchangeAction);

	return Item;
}

void AOBShopDemoPlayerController::NotifyDemoMessage(const FString& Message) const
{
	UE_LOG(LogTemp, Log, TEXT("%s"), *Message);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, Message);
	}
}

#undef LOCTEXT_NAMESPACE
