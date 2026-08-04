// Fill out your copyright notice in the Description page of Project Settings.

#include "Item/OBItemRegistry.h"

#include "Engine/DataTable.h"
#include "Item/Data/OBItemDefinition.h"

namespace
{
	// 캐시를 채우려면 const가 아닌 CDO가 필요하다(설정을 바꾸는 게 아니라 캐시만 채운다).
	UOBItemRegistry* GetReadyRegistry()
	{
		UOBItemRegistry* Registry = GetMutableDefault<UOBItemRegistry>();
		if (!Registry) return nullptr;

		Registry->EnsureCache();
		return Registry;
	}
}

const FOBItemDefinitionRow* UOBItemRegistry::FindItem(const FGameplayTag& ItemTag)
{
	if (!ItemTag.IsValid()) return nullptr;

	UOBItemRegistry* Registry = GetReadyRegistry();
	if (!Registry) return nullptr;

	const FOBItemDefinitionRow* const* Found = Registry->ItemCache.Find(ItemTag);
	return Found ? *Found : nullptr;
}

FGameplayTag UOBItemRegistry::FindTagForWeaponClass(const UClass* WeaponClass)
{
	if (!WeaponClass) return FGameplayTag();

	UOBItemRegistry* Registry = GetReadyRegistry();
	if (!Registry) return FGameplayTag();

	const FGameplayTag* Found = Registry->WeaponPathToTag.Find(FSoftObjectPath(WeaponClass));
	return Found ? *Found : FGameplayTag();
}

void UOBItemRegistry::GetAllItems(TArray<const FOBItemDefinitionRow*>& OutItems)
{
	OutItems.Reset();

	UOBItemRegistry* Registry = GetReadyRegistry();
	if (!Registry) return;

	OutItems.Reserve(Registry->ItemCache.Num());
	for (const TPair<FGameplayTag, const FOBItemDefinitionRow*>& Pair : Registry->ItemCache)
	{
		if (Pair.Value) OutItems.Add(Pair.Value);
	}
}

UDataTable* UOBItemRegistry::GetLootTable()
{
	UOBItemRegistry* Registry = GetReadyRegistry();
	return Registry ? Registry->LoadedLootTable.Get() : nullptr;
}

void UOBItemRegistry::EnsureCache()
{
	if (!bCacheBuilt)
	{
		RebuildCache();
	}
}

void UOBItemRegistry::RebuildCache()
{
	ItemCache.Reset();
	WeaponPathToTag.Reset();
	bCacheBuilt = true;

	// 드랍 테이블은 컨테이너의 RowHandle이 참조하므로 여기서 붙잡아 두기만 한다.
	LoadedLootTable = LootTable.LoadSynchronous();

	LoadedItemTable = ItemTable.LoadSynchronous();
	if (!LoadedItemTable)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[OBItemRegistry] 아이템 표가 없다. Project Settings > Game > OutBreak Items 의 "
				 "Item Table에 DT_Items를 지정할 것."));
		return;
	}

	// 캐시는 표의 행 메모리를 직접 가리킨다. 표를 하드 참조로 붙잡아 두므로 언로드되지 않는다.
	for (const TPair<FName, uint8*>& Pair : LoadedItemTable->GetRowMap())
	{
		const FOBItemDefinitionRow* Row = reinterpret_cast<const FOBItemDefinitionRow*>(Pair.Value);
		if (!Row) continue;

		if (!Row->ItemTag.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("[OBItemRegistry] 행 '%s': ItemTag가 비어 있어 조회되지 않는다."),
				*Pair.Key.ToString());
			continue;
		}

		// 중복은 조용히 앞의 것만 반환돼서 원인 찾기가 오래 걸린다. 임포트 직후에 알린다.
		if (ItemCache.Contains(Row->ItemTag))
		{
			UE_LOG(LogTemp, Warning, TEXT("[OBItemRegistry] ItemTag 중복: %s (행 '%s'). 앞의 행만 사용된다."),
				*Row->ItemTag.ToString(), *Pair.Key.ToString());
			continue;
		}

		ItemCache.Add(Row->ItemTag, Row);

		// 무기는 역방향(클래스 → 태그) 조회도 필요하다. 경로만 넣으므로 BP를 로드하지 않는다.
		if (Row->Category == EOBItemCategory::Weapon && !Row->WeaponClass.IsNull())
		{
			WeaponPathToTag.Add(Row->WeaponClass.ToSoftObjectPath(), Row->ItemTag);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[OBItemRegistry] 아이템 %d종 등록됨."), ItemCache.Num());
}

#if WITH_EDITOR
void UOBItemRegistry::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// 표를 갈아끼우면 캐시가 옛 행을 가리킨 채 남는다. 즉시 버린다.
	bCacheBuilt = false;
}
#endif
