// Fill out your copyright notice in the Description page of Project Settings.

#include "Item/OBItemRegistry.h"

#include "AssetRegistry/AssetData.h"
#include "Engine/AssetManager.h"
#include "Item/Data/OBItemDefinition.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#endif

const FName UOBItemRegistry::ItemAssetType(TEXT("OBItemDefinition"));


const UOBItemDefinition* UOBItemRegistry::FindItem(const FGameplayTag& ItemTag)
{
	if (!ItemTag.IsValid()) return nullptr;

	// 캐시를 채우려면 const가 아닌 CDO가 필요하다(설정을 바꾸는 게 아니라 캐시만 채운다).
	UOBItemRegistry* Registry = GetMutableDefault<UOBItemRegistry>();
	if (!Registry) return nullptr;

	if (!Registry->bCacheBuilt)
	{
		Registry->RebuildCache();
	}

	const TObjectPtr<UOBItemDefinition>* Found = Registry->ItemCache.Find(ItemTag);
	return Found ? Found->Get() : nullptr;
}

FGameplayTag UOBItemRegistry::FindTagForWeaponClass(const UClass* WeaponClass)
{
	if (!WeaponClass) return FGameplayTag();

	UOBItemRegistry* Registry = GetMutableDefault<UOBItemRegistry>();
	if (!Registry) return FGameplayTag();

	if (!Registry->bCacheBuilt)
	{
		Registry->RebuildCache();
	}

	const FGameplayTag* Found = Registry->WeaponPathToTag.Find(FSoftObjectPath(WeaponClass));
	return Found ? *Found : FGameplayTag();
}

void UOBItemRegistry::RebuildCache()
{
	UAssetManager* Manager = UAssetManager::GetIfInitialized();
	if (!Manager)
	{
		// 엔진 초기화보다 먼저 불렸다. 캐시를 확정하지 말고 다음 호출에서 다시 시도한다.
		return;
	}

	ItemCache.Reset();
	WeaponPathToTag.Reset();
	bCacheBuilt = true;

#if WITH_EDITOR
	// 에디터에서 아이템 에셋을 만들거나 지우면 캐시를 버린다(에디터 재시작 없이 반영).
	if (!bAssetHooksBound)
	{
		bAssetHooksBound = true;
		IAssetRegistry& AR = FAssetRegistryModule::GetRegistry();
		AR.OnAssetAdded().AddUObject(this, &UOBItemRegistry::HandleAssetChanged);
		AR.OnAssetRemoved().AddUObject(this, &UOBItemRegistry::HandleAssetChanged);
	}
#endif

	TArray<FAssetData> ScannedAssets;
	Manager->GetPrimaryAssetDataList(FPrimaryAssetType(ItemAssetType), ScannedAssets);

	if (ScannedAssets.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[OBItemRegistry] '%s' 타입이 하나도 스캔되지 않았다. Project Settings > Game > Asset Manager 의 "
				 "Primary Asset Types to Scan 에 같은 이름으로 등록됐는지 확인할 것."),
			*ItemAssetType.ToString());
		return;
	}

	for (const FAssetData& Data : ScannedAssets)
	{
		UOBItemDefinition* Def = Cast<UOBItemDefinition>(Data.GetAsset());
		if (!Def)
		{
			UE_LOG(LogTemp, Warning, TEXT("[OBItemRegistry] 로드 실패: %s"), *Data.GetObjectPathString());
			continue;
		}

		if (!Def->ItemTag.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("[OBItemRegistry] %s: ItemTag가 비어 있어 조회되지 않는다."),
				*Def->GetName());
			continue;
		}

		// 중복은 조용히 앞의 것만 반환돼서 원인 찾기가 오래 걸린다. 등록 시점에 알린다.
		if (const TObjectPtr<UOBItemDefinition>* Dup = ItemCache.Find(Def->ItemTag))
		{
			UE_LOG(LogTemp, Warning, TEXT("[OBItemRegistry] ItemTag 중복: %s — %s / %s. 앞의 것만 사용된다."),
				*Def->ItemTag.ToString(), *(*Dup)->GetName(), *Def->GetName());
			continue;
		}

		ItemCache.Add(Def->ItemTag, Def);
		// 무기는 역방향 조회도 필요하다. 경로만 넣으므로 무기 BP는 로드되지 않는다.
		if (Def->Category == EOBItemCategory::Weapon && !Def->WeaponClass.IsNull())
		{
			WeaponPathToTag.Add(Def->WeaponClass.ToSoftObjectPath(), Def->ItemTag);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[OBItemRegistry] 아이템 %d종 등록됨."), ItemCache.Num());
}

#if WITH_EDITOR
void UOBItemRegistry::HandleAssetChanged(const FAssetData& AssetData)
{
	if (AssetData.AssetClassPath == UOBItemDefinition::StaticClass()->GetClassPathName())
	{
		bCacheBuilt = false;
	}
}
#endif
