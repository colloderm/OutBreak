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
	View.Currency.Scrap = CurrentCurrency;   // 필드명은 실제 struct에 맞출 것

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
			Item.ItemId = FName(*WClass->GetName());   // 구매 시 클래스 역추적 키
			Item.CategoryId = TEXT("Weapons");
			Item.DisplayName = Data->DisplayName;
			Item.Price = Data->WeaponPrice;
			Item.StockQuantity = 1;
			// 아이콘: Data->WeaponIcon → ListIconBrush.SetResourceObject(...)
			View.Items.Add(Item);
		}
	}
	
	return View;
}

bool UOBLoadoutSubsystem::TryPurchase(UOBWeaponCatalog* Catalog, FName ItemId)
{
	if (!Catalog) return false;
	
	for (const TSubclassOf<AOBWeaponBase>& WClass : Catalog->AvailableWeapons)
	{
		if (!WClass || FName(*WClass->GetName()) != ItemId) continue;

		if (!TrySpend(GetWeaponPrice(WClass))) return false;   // 잔액 부족

		const AOBWeaponBase* CDO = WClass->GetDefaultObject<AOBWeaponBase>();
		const UOBWeaponData* Data = CDO ? CDO->GetWeaponData() : nullptr;
		if (Data) 
			SetWeapon(Data->WeaponSlot, WClass);   // 슬롯은 무기 데이터가 결정
		
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