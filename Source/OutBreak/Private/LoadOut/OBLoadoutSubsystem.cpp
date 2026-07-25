// Fill out your copyright notice in the Description page of Project Settings.

#include "LoadOut/OBLoadoutSubsystem.h"

#include "SaveGame/OBSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Weapon/OBWeaponBase.h"
#include "Weapon/Data/OBWeaponCatalog.h"
#include "Weapon/Data/OBWeaponData.h"

const FString UOBLoadoutSubsystem::SlotName = TEXT("OBPlayerProfile");

void UOBLoadoutSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 게임 시작 시 저장된 Loadout을 메모리로.
	LoadFromDisk();
}

void UOBLoadoutSubsystem::SetWeapon(EOBWeaponSlot Slot, TSubclassOf<AOBWeaponBase> WeaponClass)
{
	// 슬롯당 1개: 지정하면 덮어쓰고, null이면 비운다.
	if (WeaponClass)
	{
		CurrentLoadout.SlotWeapons.Add(Slot, TSoftClassPtr<AOBWeaponBase>(WeaponClass));
	}
	else
	{
		CurrentLoadout.SlotWeapons.Remove(Slot);
	}

	// "선택 즉시 저장" 요구사항.
	SaveToDisk();
}

void UOBLoadoutSubsystem::ClearLoadout()
{
	CurrentLoadout.SlotWeapons.Empty();
	SaveToDisk();
}

TArray<TSubclassOf<AOBWeaponBase>> UOBLoadoutSubsystem::GetSelectedClasses() const
{
	TArray<TSubclassOf<AOBWeaponBase>> Out;
	for (const TPair<EOBWeaponSlot, TSoftClassPtr<AOBWeaponBase>>& Pair : CurrentLoadout.SlotWeapons)
	{
		// 소프트 클래스 동기 로드(선택된 소수 무기라 비용 미미).
		if (UClass* Loaded = Pair.Value.LoadSynchronous())
		{
			Out.Add(Loaded);
		}
	}
	return Out;
}

TArray<TSubclassOf<AOBWeaponBase>> UOBLoadoutSubsystem::GetOwnedClasses() const
{
	TArray<TSubclassOf<AOBWeaponBase>> Out;
	for (const TSoftClassPtr<AOBWeaponBase>& Soft : CurrentLoadout.OwnedWeapons)
	{
		if (UClass* Loaded = Soft.LoadSynchronous())
		{
			Out.Add(Loaded);
		}
	}
	
	return Out;
}

bool UOBLoadoutSubsystem::IsOwnedOrEquipped(TSubclassOf<AOBWeaponBase> WeaponClass) const
{
	if (!WeaponClass) return false;
	
	const TSoftClassPtr<AOBWeaponBase> Soft(WeaponClass);
	if (CurrentLoadout.OwnedWeapons.Contains(Soft)) return true;

	for (const TPair<EOBWeaponSlot, TSoftClassPtr<AOBWeaponBase>>& Pair : CurrentLoadout.SlotWeapons)
	{
		if (Pair.Value == Soft) return true;
	}
	
	return false;
}

void UOBLoadoutSubsystem::EquipFromStash(TSubclassOf<AOBWeaponBase> WeaponClass)
{
	if (!WeaponClass) return;

	const AOBWeaponBase* CDO = WeaponClass->GetDefaultObject<AOBWeaponBase>();
	const UOBWeaponData* Data = CDO ? CDO->GetWeaponData() : nullptr;
	if (!Data) return;

	const TSoftClassPtr<AOBWeaponBase> Soft(WeaponClass);

	// 창고에 없으면(이미 장착이거나 미보유) 장착 불가.
	if (!CurrentLoadout.OwnedWeapons.Contains(Soft)) return;

	CurrentLoadout.OwnedWeapons.Remove(Soft);

	// 그 슬롯에 이미 무기가 있으면 창고로 반환(스왑).
	if (const TSoftClassPtr<AOBWeaponBase>* Old = CurrentLoadout.SlotWeapons.Find(Data->WeaponSlot))
	{
		CurrentLoadout.OwnedWeapons.Add(*Old);
	}

	CurrentLoadout.SlotWeapons.Add(Data->WeaponSlot, Soft);
	
	SaveToDisk();
}

bool UOBLoadoutSubsystem::TrySpend(int32 Amount)
{
	if (Amount <= 0 || CurrentCurrency < Amount) return false;
	
	CurrentCurrency -= Amount;
	SaveToDisk();
	
	return true;
}

void UOBLoadoutSubsystem::AddCurrency(int32 Amount)
{
	CurrentCurrency = FMath::Max(0, CurrentCurrency + Amount);
	SaveToDisk();
}

FShopWindowViewData UOBLoadoutSubsystem::BuildShopView(UOBWeaponCatalog* Catalog) const
{
	FShopWindowViewData View;
	View.ShopId = TEXT("WeaponShop");
	View.Currency.Scrap = CurrentCurrency;

	// 카테고리 1개(무기)로 시작. 슬롯별로 나누려면 여기서 확장.
	FShopCategoryViewData Cat;
	Cat.CategoryId = TEXT("Weapons");
	Cat.DisplayName = FText::FromString(TEXT("무기"));
	View.Categories.Add(Cat);

	if (Catalog)
	{
		for (const TSubclassOf<AOBWeaponBase>& WClass : Catalog->AvailableWeapons)
		{
			if (!WClass) continue;
			const AOBWeaponBase* CDO = WClass->GetDefaultObject<AOBWeaponBase>();
			const UOBWeaponData* Data = CDO ? CDO->GetWeaponData() : nullptr;
			if (!Data) continue;

			FShopItemViewData Item;
			Item.ItemId = FName(*WClass->GetName());
			Item.CategoryId = TEXT("Weapons");
			Item.DisplayName = Data->DisplayName;
			Item.MetaText = UEnum::GetDisplayValueAsText(Data->WeaponSlot);
			Item.Price = Data->WeaponPrice;
			Item.StockQuantity = 1;

			// 아이콘(리스트 + 상세 미리보기)
			if (Data->WeaponIcon)
			{
				FSlateBrush Brush;
				Brush.SetResourceObject(Data->WeaponIcon);
				Brush.DrawAs = ESlateBrushDrawType::Image;
				Brush.ImageSize = FVector2D(64.f, 64.f);
				Item.ListIconBrush = Brush;
				Item.DetailImageBrush = Brush;
			}

			// 보유/구매 상태
			const bool bOwned  = IsOwnedOrEquipped(WClass);
			const bool bAfford = CurrentCurrency >= Data->WeaponPrice;
			Item.OwnedQuantity = bOwned ? 1 : 0;

			// 구매 버튼(액션)
			FShopActionViewData Buy;
			Buy.ActionId   = TEXT("Purchase");
			Buy.Label      = FText::FromString(TEXT("구매"));
			Buy.InputDisplayText = FText::FromString(TEXT("F"));
			Buy.InputKey   = EKeys::F;
			Buy.Cost       = Data->WeaponPrice;
			Buy.Quantity   = 1;
			Buy.ActionType = EShopActionType::Purchase;
			Buy.bCanExecute = !bOwned && bAfford;
			if (bOwned)        Buy.DisabledReason = FText::FromString(TEXT("보유 중"));
			else if (!bAfford) Buy.DisabledReason = FText::FromString(TEXT("잔액 부족"));
			Item.Actions.Add(Buy);

			View.Items.Add(Item);
		}
	}
	
	// 카테고리 수량 + 초기 선택(열자마자 인스펙터에 첫 아이템 표시)
	View.Categories[0].ItemCount = View.Items.Num();
	View.InitialSelectedCategoryId = TEXT("Weapons");
	if (View.Items.Num() > 0)
	{
		View.InitialSelectedItemId = View.Items[0].ItemId;
	}
	
	return View;
}

bool UOBLoadoutSubsystem::TryPurchase(UOBWeaponCatalog* Catalog, FName ItemId)
{
	if (!Catalog) return false;
	
	for (const TSubclassOf<AOBWeaponBase>& WClass : Catalog->AvailableWeapons)
	{
		if (!WClass || FName(*WClass->GetName()) != ItemId) continue;
		
		// 이미 보유/장착 중이면 중복 구매 금지(알파: 클래스당 1개).
		if (IsOwnedOrEquipped(WClass)) return false;

		if (!TrySpend(GetWeaponPrice(WClass))) return false; // 잔액 부족

		CurrentLoadout.OwnedWeapons.Add(TSoftClassPtr<AOBWeaponBase>(WClass)); // 슬롯 아님, 창고로
		SaveToDisk();
		
		return true;
	}
	
	return false;
}

int32 UOBLoadoutSubsystem::GetWeaponPrice(TSubclassOf<AOBWeaponBase> WeaponClass)
{
	if (!WeaponClass) return 0;
	const AOBWeaponBase* CDO = WeaponClass->GetDefaultObject<AOBWeaponBase>();
	const UOBWeaponData* Data = CDO ? CDO->GetWeaponData() : nullptr;
	
	return Data ? Data->WeaponPrice : 0;
}

void UOBLoadoutSubsystem::SaveToDisk()
{
	UOBSaveGame* Save = Cast<UOBSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UOBSaveGame::StaticClass()));
	if (!Save) return;

	Save->Loadout = CurrentLoadout;
	Save->Currency = CurrentCurrency;
	UGameplayStatics::SaveGameToSlot(Save, SlotName, UserIndex);
}

void UOBLoadoutSubsystem::LoadFromDisk()
{
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
	{
		CurrentLoadout = FOBLoadout(); // 최초 실행: 빈 Loadout.
		return;
	}

	if (UOBSaveGame* Save = Cast<UOBSaveGame>(
		UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex)))
	{
		CurrentLoadout = Save->Loadout;
		CurrentCurrency = Save->Currency;
	}
}