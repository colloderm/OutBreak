// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Loadout/OBLoadoutTypes.h"
#include "UI/Shop/ShopWidgetTypes.h"
#include "OBLoadoutSubsystem.generated.h"

class AOBWeaponBase;
class UOBWeaponCatalog;

/**
 * 개인 Loadout의 런타임 소유자(GameInstance 수명 = 세션 내내, 맵 전환에도 생존).
 * - 내부는 전부 아이템 태그. 무기 클래스로 오는 호출은 레지스트리로 태그를 찾아 변환한다.
 * - 세션 맵 진입 시 클라 컨트롤러가 GetSelectedClasses()를 서버로 push.
 * - 머신 로컬이라 서버 권위가 아니다(정산은 클라가 한다). 스푸핑 방어는 서버 창고로 옮길 때 붙인다.
 */
UCLASS(BlueprintType)
class OUTBREAK_API UOBLoadoutSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// --- 슬롯 ---

	// 슬롯 무기 지정(로컬 갱신 + 즉시 저장). null이면 비운다.
	UFUNCTION(BlueprintCallable, Category = "Loadout")
	void SetWeapon(EOBWeaponSlot Slot, TSubclassOf<AOBWeaponBase> WeaponClass);

	// 해당 슬롯의 무기 클래스(없으면 null). UI가 스탯/아이콘을 뽑을 때 쓴다.
	UFUNCTION(BlueprintPure, Category = "Loadout")
	TSubclassOf<AOBWeaponBase> GetSlotWeaponClass(EOBWeaponSlot Slot) const;

	// 서버 push용: 장착 슬롯의 무기 클래스들.
	UFUNCTION(BlueprintCallable, Category = "Loadout")
	TArray<TSubclassOf<AOBWeaponBase>> GetSelectedClasses() const;

	// 사망 페널티: 장착 슬롯만 비움(창고는 유지) + 즉시 저장.
	UFUNCTION(BlueprintCallable, Category = "Loadout")
	void ClearLoadout();

	// --- 창고 ---

	// 창고에 있는 무기 클래스들(무기 카테고리만). 작업대 리스트용.
	UFUNCTION(BlueprintCallable, Category = "Loadout")
	TArray<TSubclassOf<AOBWeaponBase>> GetOwnedClasses() const;

	// 창고의 무기를 해당 슬롯에 장착(창고→슬롯, 기존 슬롯 무기는 창고로 반환).
	UFUNCTION(BlueprintCallable, Category = "Loadout")
	void EquipFromStash(TSubclassOf<AOBWeaponBase> WeaponClass);

	// 이미 창고에 있거나 장착 중인지.
	UFUNCTION(BlueprintPure, Category = "Loadout")
	bool IsOwnedOrEquipped(TSubclassOf<AOBWeaponBase> WeaponClass) const;

	UFUNCTION(BlueprintPure, Category = "Loadout")
	bool IsTagOwnedOrEquipped(const FGameplayTag& ItemTag) const;

	// 창고 입출고. 각각 즉시 저장한다(추출 정산/판매가 이걸 쓴다).
	UFUNCTION(BlueprintCallable, Category = "Loadout")
	void AddStashItem(const FGameplayTag& ItemTag, int32 Count);
	
	// 정산용 일괄 추가. 파일 쓰기를 한 번만 한다.
	UFUNCTION(BlueprintCallable, Category = "Loadout")
	void AddStashItems(const TArray<FOBItemStack>& Items);

	// 수량이 부족하면 아무것도 빼지 않고 false. 부분 차감은 하지 않는다.
	UFUNCTION(BlueprintCallable, Category = "Loadout")
	bool RemoveStashItem(const FGameplayTag& ItemTag, int32 Count);

	UFUNCTION(BlueprintPure, Category = "Loadout")
	int32 GetStashCount(const FGameplayTag& ItemTag) const;

	const TArray<FOBItemStack>& GetStashItems() const { return CurrentLoadout.StashItems; }

	// 현재 Loadout 읽기.
	const FOBLoadout& GetLoadout() const { return CurrentLoadout; }
	
	// --- 반입 ---

	// 창고 → 반입 목록. 창고 수량이 모자라면 아무것도 안 옮기고 false.
	UFUNCTION(BlueprintCallable, Category = "Loadout")
	bool AddCarryItem(const FGameplayTag& ItemTag, int32 Count);

	// 반입 목록 → 창고(선택 취소).
	UFUNCTION(BlueprintCallable, Category = "Loadout")
	bool RemoveCarryItem(const FGameplayTag& ItemTag, int32 Count);

	// 탐사 종료 시 항상 비운다. 사망이면 손실, 탈출이면 이미 정산으로 창고에 들어갔다.
	UFUNCTION(BlueprintCallable, Category = "Loadout")
	void ClearCarryItems();

	UFUNCTION(BlueprintPure, Category = "Loadout")
	int32 GetCarryCount(const FGameplayTag& ItemTag) const;

	const TArray<FOBItemStack>& GetCarryItems() const { return CurrentLoadout.CarryItems; }

	// --- 스타터킷 ---

	// 무기를 하나도 안 가진 상태. 창고에 소모품만 남은 것도 '없음'으로 본다
	// (안 그러면 붕대만 남은 플레이어가 스타터킷을 못 받아 소프트락).
	UFUNCTION(BlueprintPure, Category = "Loadout")
	bool HasAnyWeapon() const;

	// 무기가 하나도 없을 때만 스타터 무기를 창고에 무료 지급.
	UFUNCTION(BlueprintCallable, Category = "Loadout")
	void GrantStarterIfEmpty(UOBWeaponCatalog* Catalog);

	// 비어 있는 슬롯만 메운다. 반환값 = 실제 지급 개수(0이면 줄 게 없었다는 뜻).
	int32 GrantMissingStarters(const TArray<TSubclassOf<AOBWeaponBase>>& Weapons);

	// --- 통화 ---
	UFUNCTION(BlueprintPure, Category = "Currency")
	int32 GetCurrency() const { return CurrentCurrency; }

	// 재화 증감. 결과가 음수면 실패 반환하고 변경 안 함.
	UFUNCTION(BlueprintCallable, Category = "Currency")
	bool TrySpend(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Currency")
	void AddCurrency(int32 Amount);

	// --- 상점 ---

	// 탭별 뷰 구성. 구매/판매 모두 ItemDefinition 기반이라 무기 카탈로그를 쓰지 않는다.
	UFUNCTION(BlueprintCallable, Category = "Shop")
	FShopWindowViewData BuildShopView(EShopTab Tab) const;

	// 아이템 구매. 비매품(BuyPrice<=0)이거나 잔액이 모자라면 false.
	UFUNCTION(BlueprintCallable, Category = "Shop")
	bool TryPurchaseItem(const FGameplayTag& ItemTag, int32 Count);

	// 창고의 아이템을 판다. 비매품(SellPrice<=0)이거나 수량이 모자라면 false.
	// 장착 중인 무기는 창고에 없으므로 자동으로 걸러진다(창고/슬롯 불변식).
	UFUNCTION(BlueprintCallable, Category = "Shop")
	bool TrySell(const FGameplayTag& ItemTag, int32 Count);

	// 아이템 태그 → 무기 클래스(동기 로드). 무기 아이템이 아니면 null.
	UFUNCTION(BlueprintPure, Category = "Loadout")
	static TSubclassOf<AOBWeaponBase> ResolveWeaponClass(const FGameplayTag& ItemTag);

	// 디스크 저장/로드.
	void SaveToDisk();
	void LoadFromDisk();

private:
	void AppendBuyItems(FShopWindowViewData& View, TSet<EOBItemCategory>& OutCategories) const;
	void AppendSellItems(FShopWindowViewData& View, TSet<EOBItemCategory>& OutCategories) const;
	
	// 저장 없이 메모리만 갱신. 실제로 늘었으면 true.
	bool AddStashItemInternal(const FGameplayTag& ItemTag, int32 Count);
	
private:
	// 런타임 권위값(로드/편집 대상).
	FOBLoadout CurrentLoadout;

	// SaveGame 슬롯 이름(단일 프로필. 멀티프로필 시 확장).
	static const FString SlotName;
	static const int32 UserIndex = 0;
	
	int32 CurrentCurrency = 0;
	
};
