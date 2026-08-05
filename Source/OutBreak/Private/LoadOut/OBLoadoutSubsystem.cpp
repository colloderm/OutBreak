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
	const FName ShopAction_Purchase(TEXT("Purchase"));
	const FName ShopAction_PurchaseBulk(TEXT("PurchaseBulk"));
	const FName ShopAction_SellOne(TEXT("SellOne"));
	const FName ShopAction_SellAll(TEXT("SellAll"));

	// 무기 클래스에서 슬롯/스탯을 얻는다. 슬롯은 무기 스펙의 소유라 ItemDefinition에 중복 저장하지 않았다.
	const UOBWeaponData* WeaponDataOf(TSubclassOf<AOBWeaponBase> WeaponClass)
	{
		if (!WeaponClass) return nullptr;
		const AOBWeaponBase* CDO = WeaponClass->GetDefaultObject<AOBWeaponBase>();
		return CDO ? CDO->GetWeaponData() : nullptr;
	}

	FName CategoryIdOf(EOBItemCategory Category)
	{
		return FName(*StaticEnum<EOBItemCategory>()->GetNameStringByValue(static_cast<int64>(Category)));
	}

	void AddStat(FShopItemViewData& Item, const TCHAR* Id, const TCHAR* Label, FText Value, int32 Order)
	{
		FShopItemStatViewData Stat;
		Stat.StatId = FName(Id);
		Stat.DisplayName = FText::FromString(Label);
		Stat.DisplayValue = MoveTemp(Value);
		Stat.SortOrder = Order;
		Item.Stats.Add(Stat);
	}

	void SetItemIcon(FShopItemViewData& Item, UTexture2D* Icon)
	{
		if (!Icon) return;

		FSlateBrush Brush;
		Brush.SetResourceObject(Icon);
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = FVector2D(64.f, 64.f);
		Item.ListIconBrush = Brush;
		Item.DetailImageBrush = Brush;
	}

	// 구매/판매가 공통으로 채우는 부분.
	// 무기는 이름/설명/아이콘/세부분류의 원본이 WeaponData다. ItemDefinition 쪽을 비워두면
	// 거기서 끌어와, 같은 정보를 두 에셋에 중복 입력하지 않게 한다.
	FShopItemViewData MakeItemView(const FOBItemDefinitionRow& Def, const UOBWeaponData* WData)
	{
		FShopItemViewData Item;
		Item.ItemId     = FName(*Def.ItemTag.ToString());
		Item.CategoryId = CategoryIdOf(Def.Category);

		Item.DisplayName = !Def.DisplayName.IsEmpty()
			? Def.DisplayName
			: (WData ? WData->DisplayName : FText::FromName(Def.ItemTag.GetTagName()));

		Item.Description = !Def.Description.IsEmpty()
			? Def.Description
			: (WData ? WData->Description : FText::GetEmpty());

		// 부제: 무기는 세부 분류(돌격소총/권총), 그 외는 아이템 분류(소모품/귀중품).
		Item.MetaText = WData
			? UEnum::GetDisplayValueAsText(WData->WeaponCategory)
			: UEnum::GetDisplayValueAsText(Def.Category);

		// 인벤토리 자동 정렬과 같은 기준(카테고리 → SortOrder).
		Item.SortOrder = static_cast<int32>(Def.Category) * 1000 + Def.SortOrder;

		// 표 전체가 로드돼도 아이콘은 필요할 때만 올린다(상점을 여는 순간).
		const TObjectPtr<UTexture2D> IconTex = Def.Icon.LoadSynchronous();
		SetItemIcon(Item, IconTex ? IconTex : (WData ? WData->WeaponIcon : nullptr));
		return Item;
	}

	// 무기만 상세 스탯을 갖는다. 상점을 여는 순간 목록에 뜬 무기 수만큼만 로드된다.
	void AppendWeaponStats(FShopItemViewData& Item, const UOBWeaponData* WData)
	{
		if (!WData) return;

		AddStat(Item, TEXT("Damage"),   TEXT("데미지"), FText::AsNumber(WData->BaseDamage), 0);
		AddStat(Item, TEXT("RPM"),      TEXT("연사력"), FText::AsNumber(WData->RoundsPerMinute), 1);
		AddStat(Item, TEXT("Spread"),   TEXT("탄퍼짐"), FText::FromString(FString::Printf(TEXT("%.2f°"), WData->BaseSpreadDegrees)), 2);
		AddStat(Item, TEXT("Recoil"),   TEXT("반동"),   FText::FromString(FString::Printf(TEXT("%.1f"), WData->VerticalRecoil)), 3);
		AddStat(Item, TEXT("Magazine"), TEXT("탄창"),   FText::AsNumber(WData->MagazineSize), 4);
		AddStat(Item, TEXT("Mobility"), TEXT("기동성"), FText::FromString(FString::Printf(TEXT("x%.2f"), WData->MobilityMultiplier)), 5);
	}
	
	// 무기 아이템이면 WeaponData를, 아니면 null. 상점을 여는 순간 목록에 뜬 무기 수만큼만 로드된다.
	const UOBWeaponData* WeaponDataForItem(const FOBItemDefinitionRow& Def)
	{
		if (Def.Category != EOBItemCategory::Weapon) return nullptr;
		return WeaponDataOf(UOBLoadoutSubsystem::ResolveWeaponClass(Def.ItemTag));
	}

	// 왼쪽 카테고리 목록은 실제로 존재하는 항목에서만 만든다(빈 카테고리를 띄우지 않는다).
	void MaterializeCategories(const TSet<EOBItemCategory>& Present, FShopWindowViewData& View)
	{
		for (int32 i = 0; i <= static_cast<int32>(EOBItemCategory::Material); ++i)
		{
			const EOBItemCategory C = static_cast<EOBItemCategory>(i);
			if (!Present.Contains(C)) continue;

			FShopCategoryViewData Cat;
			Cat.CategoryId  = CategoryIdOf(C);
			Cat.DisplayName = UEnum::GetDisplayValueAsText(C);
			Cat.SortOrder   = i;
			View.Categories.Add(Cat);
		}

		for (FShopCategoryViewData& Cat : View.Categories)
		{
			for (const FShopItemViewData& Item : View.Items)
			{
				if (Item.CategoryId == Cat.CategoryId) ++Cat.ItemCount;
			}
		}
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
	const FOBItemDefinitionRow* Def = UOBItemRegistry::FindItem(ItemTag);
	if (!Def || Def->Category != EOBItemCategory::Weapon) return nullptr;

	return TSubclassOf<AOBWeaponBase>(Def->WeaponClass.LoadSynchronous());
}

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

void UOBLoadoutSubsystem::AddStashItem(const FGameplayTag& ItemTag, int32 Count)
{
	if (AddStashItemInternal(ItemTag, Count))
	{
		SaveToDisk();
	}
}

void UOBLoadoutSubsystem::AddStashItems(const TArray<FOBItemStack>& Items)
{
	bool bChanged = false;
	for (const FOBItemStack& Stack : Items)
	{
		if (AddStashItemInternal(Stack.ItemTag, Stack.Count))
		{
			bChanged = true;
		}
	}

	// 정산 한 판에 파일 쓰기 한 번.
	if (bChanged)
	{
		SaveToDisk();
	}
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

bool UOBLoadoutSubsystem::HasAnyWeapon() const
{
	if (!CurrentLoadout.SlotWeapons.IsEmpty()) return true;

	for (const FOBItemStack& Stack : CurrentLoadout.StashItems)
	{
		if (Stack.IsEmpty()) continue;

		const FOBItemDefinitionRow* Def = UOBItemRegistry::FindItem(Stack.ItemTag);
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

FShopWindowViewData UOBLoadoutSubsystem::BuildShopView(EShopTab Tab) const
{
	FShopWindowViewData View;
	View.ShopId = TEXT("WeaponShop");
	View.Currency.Scrap = CurrentCurrency;

	TSet<EOBItemCategory> Present;
	switch (Tab)
	{
	case EShopTab::Buy:  
		AppendBuyItems(View, Present);
		break;
	case EShopTab::Sell: 
		AppendSellItems(View, Present); 
		break;
	default: 
		break;   // 교환/의뢰는 아직 내용이 없다
	}

	View.Items.Sort([](const FShopItemViewData& A, const FShopItemViewData& B)
	{
		return A.SortOrder != B.SortOrder ? A.SortOrder < B.SortOrder : A.ItemId.LexicalLess(B.ItemId);
	});

	MaterializeCategories(Present, View);

	// 열자마자 첫 카테고리의 첫 항목을 인스펙터에 표시.
	if (!View.Categories.IsEmpty()) 
		View.InitialSelectedCategoryId = View.Categories[0].CategoryId;
	if (!View.Items.IsEmpty())      
		View.InitialSelectedItemId     = View.Items[0].ItemId;

	return View;
}

void UOBLoadoutSubsystem::AppendBuyItems(FShopWindowViewData& View, TSet<EOBItemCategory>& OutCategories) const
{
	TArray<const FOBItemDefinitionRow*> AllItems;
	UOBItemRegistry::GetAllItems(AllItems);

	for (const FOBItemDefinitionRow* Def : AllItems)
	{
		if (!Def || Def->BuyPrice <= 0) continue;   // 비매품은 목록에 띄우지 않는다

		const bool bWeapon = (Def->Category == EOBItemCategory::Weapon);
		const UOBWeaponData* WData = WeaponDataForItem(*Def);
		const bool bOwned  = bWeapon && IsTagOwnedOrEquipped(Def->ItemTag);
		const bool bAfford = CurrentCurrency >= Def->BuyPrice;

		FShopItemViewData Item = MakeItemView(*Def, WData);
		Item.Price         = Def->BuyPrice;
		Item.StockQuantity = 1;
		Item.OwnedQuantity = bWeapon ? (bOwned ? 1 : 0) : GetStashCount(Def->ItemTag);

		AppendWeaponStats(Item, WData);
		AddStat(Item, TEXT("BuyPrice"), TEXT("가격"), FText::AsNumber(Def->BuyPrice), 10);
		AddStat(Item, TEXT("Weight"),   TEXT("무게"), FText::FromString(FString::Printf(TEXT("%.2f kg"), Def->Weight)), 11);

		FShopActionViewData Buy;
		Buy.ActionId         = ShopAction_Purchase;
		Buy.Label            = FText::FromString(TEXT("구매"));
		Buy.InputDisplayText = FText::FromString(TEXT("F"));
		Buy.InputKey         = EKeys::F;
		Buy.Cost             = Def->BuyPrice;
		Buy.Quantity         = 1;
		Buy.ActionType       = EShopActionType::Purchase;
		Buy.bCanExecute      = !bOwned && bAfford;
		if (bOwned)
			Buy.DisabledReason = FText::FromString(TEXT("보유 중"));
		else if (!bAfford) 
			Buy.DisabledReason = FText::FromString(TEXT("잔액 부족"));
		Item.Actions.Add(Buy);

		// 탄약/소모품은 한 발씩 사게 만들지 않는다.
		if (!bWeapon && Def->MaxStack > 1)
		{
			const int64 BulkCost = static_cast<int64>(Def->BuyPrice) * Def->MaxStack;

			FShopActionViewData Bulk;
			Bulk.ActionId         = ShopAction_PurchaseBulk;
			Bulk.Label            = FText::FromString(FString::Printf(TEXT("%d개 구매"), Def->MaxStack));
			Bulk.InputDisplayText = FText::FromString(TEXT("G"));
			Bulk.InputKey         = EKeys::G;
			Bulk.Cost             = static_cast<int32>(FMath::Min<int64>(BulkCost, MAX_int32));
			Bulk.Quantity         = Def->MaxStack;
			Bulk.ActionType       = EShopActionType::Purchase;
			Bulk.bCanExecute      = CurrentCurrency >= BulkCost;
			if (!Bulk.bCanExecute) 
				Bulk.DisabledReason = FText::FromString(TEXT("잔액 부족"));
			Item.Actions.Add(Bulk);
		}

		OutCategories.Add(Def->Category);
		View.Items.Add(MoveTemp(Item));
	}
}

void UOBLoadoutSubsystem::AppendSellItems(FShopWindowViewData& View, TSet<EOBItemCategory>& OutCategories) const
{
	for (const FOBItemStack& Stack : CurrentLoadout.StashItems)
	{
		if (Stack.IsEmpty()) continue;

		const FOBItemDefinitionRow* Def = UOBItemRegistry::FindItem(Stack.ItemTag);
		if (!Def || Def->SellPrice <= 0) continue;   // 못 파는 물건은 목록에 띄우지 않는다

		const UOBWeaponData* WData = WeaponDataForItem(*Def);

		FShopItemViewData Item = MakeItemView(*Def, WData);
		Item.Price         = Def->SellPrice;
		Item.StockQuantity = Stack.Count;
		Item.OwnedQuantity = Stack.Count;

		AppendWeaponStats(Item, WData);
		AddStat(Item, TEXT("SellPrice"), TEXT("개당 판매가"), FText::AsNumber(Def->SellPrice), 10);
		AddStat(Item, TEXT("Owned"),     TEXT("보유"),        FText::AsNumber(Stack.Count), 11);
		AddStat(Item, TEXT("Weight"),    TEXT("무게"),        FText::FromString(FString::Printf(TEXT("%.2f kg"), Def->Weight)), 12);

		FShopActionViewData SellOne;
		SellOne.ActionId         = ShopAction_SellOne;
		SellOne.Label            = FText::FromString(TEXT("판매"));
		SellOne.InputDisplayText = FText::FromString(TEXT("F"));
		SellOne.InputKey         = EKeys::F;
		SellOne.Cost             = 0;
		SellOne.Quantity         = 1;
		SellOne.ActionType       = EShopActionType::Exchange;
		SellOne.bCanExecute      = true;
		Item.Actions.Add(SellOne);

		// 잡템 20개를 스무 번 클릭하게 만들지 않는다.
		if (Stack.Count > 1)
		{
			FShopActionViewData SellAll;
			SellAll.ActionId         = ShopAction_SellAll;
			SellAll.Label            = FText::FromString(FString::Printf(TEXT("전부 판매 (%d)"), Stack.Count));
			SellAll.InputDisplayText = FText::FromString(TEXT("G"));
			SellAll.InputKey         = EKeys::G;
			SellAll.Quantity         = Stack.Count;
			SellAll.ActionType       = EShopActionType::Exchange;
			SellAll.bCanExecute      = true;
			Item.Actions.Add(SellAll);
		}

		OutCategories.Add(Def->Category);
		View.Items.Add(MoveTemp(Item));
	}
}

bool UOBLoadoutSubsystem::AddStashItemInternal(const FGameplayTag& ItemTag, int32 Count)
{
	if (!ItemTag.IsValid() || Count <= 0) return false;

	// 창고는 칸 제한이 없어서 MaxStack을 적용하지 않고 태그당 한 항목으로 합친다.
	const int32 Index = CurrentLoadout.StashItems.IndexOfByPredicate(
		[&ItemTag](const FOBItemStack& S)
		{
			return S.ItemTag == ItemTag;
		});

	if (Index != INDEX_NONE)
	{
		CurrentLoadout.StashItems[Index].Count += Count;
	}
	else
	{
		CurrentLoadout.StashItems.Emplace(ItemTag, Count);
	}
	return true;
}

bool UOBLoadoutSubsystem::TryPurchaseItem(const FGameplayTag& ItemTag, int32 Count)
{
	if (Count <= 0) return false;

	const FOBItemDefinitionRow* Def = UOBItemRegistry::FindItem(ItemTag);
	if (!Def || Def->BuyPrice <= 0) return false;   // 비매품

	// 무기는 알파 규칙상 종류당 1개.
	if (Def->Category == EOBItemCategory::Weapon)
	{
		if (IsTagOwnedOrEquipped(ItemTag)) return false;
		Count = 1;
	}

	// 돈 계산은 int32 넘침을 만들지 않는다.
	const int64 Cost = static_cast<int64>(Def->BuyPrice) * Count;
	if (Cost > MAX_int32) return false;

	if (!TrySpend(static_cast<int32>(Cost))) return false;   // 잔액 부족

	AddStashItem(ItemTag, Count);   // 슬롯 아님, 창고로(내부에서 저장)
	return true;
}

bool UOBLoadoutSubsystem::TrySell(const FGameplayTag& ItemTag, int32 Count)
{
	if (Count <= 0) return false;

	const FOBItemDefinitionRow* Def = UOBItemRegistry::FindItem(ItemTag);
	if (!Def || Def->SellPrice <= 0) return false;   // 비매품

	// 창고에서 먼저 빼고, 성공했을 때만 돈을 준다. 실패하면 창고는 그대로다.
	if (!RemoveStashItem(ItemTag, Count)) return false;

	// 돈 계산은 int32 넘침을 만들지 않는다.
	const int64 Gain = static_cast<int64>(Def->SellPrice) * Count;
	AddCurrency(static_cast<int32>(FMath::Min<int64>(Gain, MAX_int32)));

	return true;
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

bool UOBLoadoutSubsystem::AddCarryItem(const FGameplayTag& ItemTag, int32 Count)
{
	// 창고에서 먼저 뺀다. 부분 차감을 안 하므로 실패하면 아무 일도 안 일어난다.
	if (!RemoveStashItem(ItemTag, Count)) return false;

	OBItemStacks::Add(CurrentLoadout.CarryItems, ItemTag, Count);
	SaveToDisk();
	return true;
}

bool UOBLoadoutSubsystem::RemoveCarryItem(const FGameplayTag& ItemTag, int32 Count)
{
	if (!ItemTag.IsValid() || Count <= 0) return false;

	const int32 Index = CurrentLoadout.CarryItems.IndexOfByPredicate(
		[&ItemTag](const FOBItemStack& S)
		{
			return S.ItemTag == ItemTag;
		});
	if (Index == INDEX_NONE || CurrentLoadout.CarryItems[Index].Count < Count) return false;

	CurrentLoadout.CarryItems[Index].Count -= Count;
	if (CurrentLoadout.CarryItems[Index].Count <= 0)
	{
		CurrentLoadout.CarryItems.RemoveAt(Index);
	}

	AddStashItemInternal(ItemTag, Count);   // 창고로 되돌린다
	SaveToDisk();
	return true;
}

void UOBLoadoutSubsystem::ClearCarryItems()
{
	if (CurrentLoadout.CarryItems.IsEmpty()) return;

	CurrentLoadout.CarryItems.Empty();
	SaveToDisk();
}

int32 UOBLoadoutSubsystem::GetCarryCount(const FGameplayTag& ItemTag) const
{
	const FOBItemStack* Found = CurrentLoadout.CarryItems.FindByPredicate(
		[&ItemTag](const FOBItemStack& S)
		{
			return S.ItemTag == ItemTag;
		});
	return Found ? Found->Count : 0;
}
