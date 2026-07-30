// Fill out your copyright notice in the Description page of Project Settings.

#include "LoadOut/OBLoadoutSubsystem.h"

#include "SaveGame/OBSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Item/OBItemRegistry.h"
#include "Item/Data/OBItemDefinition.h"
#include "Weapon/OBWeaponBase.h"
#include "Weapon/Data/OBWeaponCatalog.h"
#include "Weapon/Data/OBWeaponData.h"

const FString UOBLoadoutSubsystem::SlotName = TEXT("OBPlayerProfile");

namespace
{
	// 무기 클래스에서 슬롯을 얻는다. 슬롯은 무기 스펙의 소유라 ItemDefinition에 중복 저장하지 않았다.
	const UOBWeaponData* WeaponDataOf(TSubclassOf<AOBWeaponBase> WeaponClass)
	{
		if (!WeaponClass) return nullptr;
		const AOBWeaponBase* CDO = WeaponClass->GetDefaultObject<AOBWeaponBase>();
		return CDO ? CDO->GetWeaponData() : nullptr;
	}
}

void UOBLoadoutSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 게임 시작 시 저장된 Loadout을 메모리로.
	LoadFromDisk();
}

TSubclassOf<AOBWeaponBase> UOBLoadoutSubsystem::ResolveWeaponClass(const FGameplayTag& ItemTag)
{
	const UOBItemDefinition* Def = UOBItemRegistry::FindItem(ItemTag);
	if (!Def || Def->Category != EOBItemCategory::Weapon) return nullptr;

	return TSubclassOf<AOBWeaponBase>(Def->WeaponClass.LoadSynchronous());
}

// --- 슬롯 ---

void UOBLoadoutSubsystem::SetWeapon(EOBWeaponSlot Slot, TSubclassOf<AOBWeaponBase> WeaponClass)
{
	if (WeaponClass)
	{
		const FGameplayTag Tag = UOBItemRegistry::FindTagForWeaponClass(WeaponClass);
		if (!Tag.IsValid())
		{
			// 이 무기의 ItemDefinition을 안 만들었다. 조용히 사라지면 원인을 못 찾으니 크게 남긴다.
			UE_LOG(LogTemp, Warning,
				TEXT("[Loadout] %s 에 대응하는 ItemDefinition이 없어 슬롯 지정을 무시했다. "
					 "Content/Data/Items 에 WeaponClass=%s 인 정의를 만들 것."),
				*WeaponClass->GetName(), *WeaponClass->GetName());
			return;
		}
		CurrentLoadout.SlotWeapons.Add(Slot, Tag);
	}
	else
	{
		CurrentLoadout.SlotWeapons.Remove(Slot);
	}

	// "선택 즉시 저장" 요구사항.
	SaveToDisk();
}

TSubclassOf<AOBWeaponBase> UOBLoadoutSubsystem::GetSlotWeaponClass(EOBWeaponSlot Slot) const
{
	const FGameplayTag* Tag = CurrentLoadout.SlotWeapons.Find(Slot);
	return Tag ? ResolveWeaponClass(*Tag) : nullptr;
}

TArray<TSubclassOf<AOBWeaponBase>> UOBLoadoutSubsystem::GetSelectedClasses() const
{
	TArray<TSubclassOf<AOBWeaponBase>> Out;
	for (const TPair<EOBWeaponSlot, FGameplayTag>& Pair : CurrentLoadout.SlotWeapons)
	{
		// 선택된 소수 무기라 동기 로드 비용 미미.
		if (TSubclassOf<AOBWeaponBase> Loaded = ResolveWeaponClass(Pair.Value))
		{
			Out.Add(Loaded);
		}
	}
	return Out;
}

void UOBLoadoutSubsystem::ClearLoadout()
{
	CurrentLoadout.SlotWeapons.Empty();
	SaveToDisk();
}

// --- 창고 ---

void UOBLoadoutSubsystem::AddStashItem(const FGameplayTag& ItemTag, int32 Count)
{
	if (!ItemTag.IsValid() || Count <= 0) return;

	// 창고는 칸 제한이 없어서 MaxStack을 적용하지 않고 태그당 한 항목으로 합친다.
	const int32 Index = CurrentLoadout.StashItems.IndexOfByPredicate(
		[&ItemTag](const FOBItemStack& S) { return S.ItemTag == ItemTag; });

	if (Index != INDEX_NONE)
	{
		CurrentLoadout.StashItems[Index].Count += Count;
	}
	else
	{
		CurrentLoadout.StashItems.Emplace(ItemTag, Count);
	}

	// ponytail: 호출마다 파일 쓰기. 정산에서 수십 종을 한꺼번에 넣게 되면 배치 저장으로 바꾼다.
	SaveToDisk();
}

bool UOBLoadoutSubsystem::RemoveStashItem(const FGameplayTag& ItemTag, int32 Count)
{
	if (!ItemTag.IsValid() || Count <= 0) return false;

	const int32 Index = CurrentLoadout.StashItems.IndexOfByPredicate(
		[&ItemTag](const FOBItemStack& S) { return S.ItemTag == ItemTag; });
	if (Index == INDEX_NONE) return false;

	// 부분 차감은 하지 않는다. 실패하면 창고는 그대로다.
	if (CurrentLoadout.StashItems[Index].Count < Count) return false;

	CurrentLoadout.StashItems[Index].Count -= Count;
	if (CurrentLoadout.StashItems[Index].Count <= 0)
	{
		CurrentLoadout.StashItems.RemoveAt(Index);
	}

	SaveToDisk();
	return true;
}

int32 UOBLoadoutSubsystem::GetStashCount(const FGameplayTag& ItemTag) const
{
	for (const FOBItemStack& Stack : CurrentLoadout.StashItems)
	{
		if (Stack.ItemTag == ItemTag) return Stack.Count;
	}
	return 0;
}

TArray<TSubclassOf<AOBWeaponBase>> UOBLoadoutSubsystem::GetOwnedClasses() const
{
	TArray<TSubclassOf<AOBWeaponBase>> Out;
	for (const FOBItemStack& Stack : CurrentLoadout.StashItems)
	{
		if (Stack.IsEmpty()) continue;

		// 창고에는 소모품/귀중품도 섞여 있다. 작업대 리스트는 무기만 본다.
		if (TSubclassOf<AOBWeaponBase> Loaded = ResolveWeaponClass(Stack.ItemTag))
		{
			Out.Add(Loaded);
		}
	}
	return Out;
}

bool UOBLoadoutSubsystem::IsTagOwnedOrEquipped(const FGameplayTag& ItemTag) const
{
	if (!ItemTag.IsValid()) return false;

	for (const TPair<EOBWeaponSlot, FGameplayTag>& Pair : CurrentLoadout.SlotWeapons)
	{
		if (Pair.Value == ItemTag) return true;
	}
	return GetStashCount(ItemTag) > 0;
}

bool UOBLoadoutSubsystem::IsOwnedOrEquipped(TSubclassOf<AOBWeaponBase> WeaponClass) const
{
	return IsTagOwnedOrEquipped(UOBItemRegistry::FindTagForWeaponClass(WeaponClass));
}

void UOBLoadoutSubsystem::EquipFromStash(TSubclassOf<AOBWeaponBase> WeaponClass)
{
	const FGameplayTag Tag = UOBItemRegistry::FindTagForWeaponClass(WeaponClass);
	if (!Tag.IsValid()) return;

	const UOBWeaponData* WData = WeaponDataOf(WeaponClass);
	if (!WData) return;

	// 창고에 없으면(이미 장착이거나 미보유) 장착 불가.
	if (!RemoveStashItem(Tag, 1)) return;

	// 그 슬롯에 이미 무기가 있으면 창고로 반환(스왑).
	if (const FGameplayTag* Old = CurrentLoadout.SlotWeapons.Find(WData->WeaponSlot))
	{
		AddStashItem(*Old, 1);
	}

	CurrentLoadout.SlotWeapons.Add(WData->WeaponSlot, Tag);

	SaveToDisk();
}

// --- 스타터킷 ---

bool UOBLoadoutSubsystem::HasAnyWeapon() const
{
	if (!CurrentLoadout.SlotWeapons.IsEmpty()) return true;

	for (const FOBItemStack& Stack : CurrentLoadout.StashItems)
	{
		if (Stack.IsEmpty()) continue;

		const UOBItemDefinition* Def = UOBItemRegistry::FindItem(Stack.ItemTag);
		if (Def && Def->Category == EOBItemCategory::Weapon) return true;
	}
	return false;
}

void UOBLoadoutSubsystem::GrantStarterIfEmpty(UOBWeaponCatalog* Catalog)
{
	if (!Catalog || HasAnyWeapon()) return;

	for (const TSubclassOf<AOBWeaponBase>& WClass : Catalog->StarterWeapons)
	{
		if (!WClass) continue;

		const FGameplayTag Tag = UOBItemRegistry::FindTagForWeaponClass(WClass);
		if (!Tag.IsValid())
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Loadout] 스타터 무기 %s 의 ItemDefinition이 없어 지급하지 못했다."), *WClass->GetName());
			continue;
		}

		AddStashItem(Tag, 1);   // 내부에서 저장
	}
}

int32 UOBLoadoutSubsystem::GrantMissingStarters(const TArray<TSubclassOf<AOBWeaponBase>>& Weapons)
{
	int32 Granted = 0;

	for (const TSubclassOf<AOBWeaponBase>& WClass : Weapons)
	{
		if (!WClass) continue;

		const FGameplayTag Tag = UOBItemRegistry::FindTagForWeaponClass(WClass);
		if (!Tag.IsValid())
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Loadout] 기본 지급 무기 %s 의 ItemDefinition이 없어 건너뛰었다."), *WClass->GetName());
			continue;
		}

		// 불변식 유지: 한 아이템은 창고 OR 슬롯 중 한 곳에만 존재한다.
		if (IsTagOwnedOrEquipped(Tag)) continue;

		const UOBWeaponData* WData = WeaponDataOf(WClass);
		if (!WData) continue;

		// 이미 찬 슬롯은 건너뛴다. 빈 슬롯만 메우는 게 목적이지 무료 교체가 아니다.
		if (CurrentLoadout.SlotWeapons.Contains(WData->WeaponSlot)) continue;

		CurrentLoadout.SlotWeapons.Add(WData->WeaponSlot, Tag);
		++Granted;
	}

	if (Granted > 0)
		SaveToDisk();

	return Granted;
}

// --- 통화 ---

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

// --- 상점 ---

FShopWindowViewData UOBLoadoutSubsystem::BuildShopView(UOBWeaponCatalog* Catalog) const
{
	FShopWindowViewData View;
	View.ShopId = TEXT("WeaponShop");
	View.Currency.Scrap = CurrentCurrency;

	// 카테고리 1개(무기)로 시작. 아이템 판매 탭은 L8에서 붙인다.
	FShopCategoryViewData Cat;
	Cat.CategoryId = TEXT("Weapons");
	Cat.DisplayName = FText::FromString(TEXT("무기"));
	View.Categories.Add(Cat);

	if (Catalog)
	{
		for (const TSubclassOf<AOBWeaponBase>& WClass : Catalog->AvailableWeapons)
		{
			if (!WClass) continue;
			const UOBWeaponData* Data = WeaponDataOf(WClass);
			if (!Data) continue;

			FShopItemViewData Item;
			Item.ItemId = FName(*WClass->GetName());
			Item.CategoryId = TEXT("Weapons");
			Item.DisplayName = Data->DisplayName;
			Item.MetaText = UEnum::GetDisplayValueAsText(Data->WeaponCategory);
			Item.Description = Data->Description;
			Item.Price = Data->WeaponPrice;
			Item.StockQuantity = 1;
			
			// 상세 스탯(인스펙터 VBX_ItemStatList)
			auto AddStat = [&Item](const TCHAR* Id, const TCHAR* Label, FText Value, int32 Order)
			{
				FShopItemStatViewData Stat;
				Stat.StatId = FName(Id);
				Stat.DisplayName = FText::FromString(Label);
				Stat.DisplayValue = MoveTemp(Value);
				Stat.SortOrder = Order;
				Item.Stats.Add(Stat);
			};
			AddStat(TEXT("Damage"),   TEXT("데미지"), FText::AsNumber(Data->BaseDamage), 0);
			AddStat(TEXT("RPM"),      TEXT("연사력"), FText::AsNumber(Data->RoundsPerMinute), 1);
			AddStat(TEXT("Spread"),   TEXT("탄퍼짐"), FText::FromString(FString::Printf(TEXT("%.2f°"), Data->BaseSpreadDegrees)), 2);
			AddStat(TEXT("Recoil"),   TEXT("반동"),   FText::FromString(FString::Printf(TEXT("%.1f"), Data->VerticalRecoil)), 3);
			AddStat(TEXT("Magazine"), TEXT("탄창"),   FText::AsNumber(Data->MagazineSize), 4);
			AddStat(TEXT("Mobility"), TEXT("기동성"), FText::FromString(FString::Printf(TEXT("x%.2f"), Data->MobilityMultiplier)), 5);

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

		// 정의가 없으면 창고에 넣을 수 없다. 돈이 사라지지 않게 지불 전에 막는다.
		const FGameplayTag Tag = UOBItemRegistry::FindTagForWeaponClass(WClass);
		if (!Tag.IsValid())
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Loadout] %s 의 ItemDefinition이 없어 구매를 막았다."), *WClass->GetName());
			return false;
		}

		if (!TrySpend(GetWeaponPrice(WClass))) return false; // 잔액 부족

		AddStashItem(Tag, 1);   // 슬롯 아님, 창고로(내부에서 저장)
		
		return true;
	}
	
	return false;
}

int32 UOBLoadoutSubsystem::GetWeaponPrice(TSubclassOf<AOBWeaponBase> WeaponClass)
{
	const UOBWeaponData* Data = WeaponDataOf(WeaponClass);
	return Data ? Data->WeaponPrice : 0;
}

// --- 저장 ---

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