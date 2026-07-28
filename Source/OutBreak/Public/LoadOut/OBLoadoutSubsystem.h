// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Loadout/OBLoadoutTypes.h"
#include "UI/Shop/ShopWidgetTypes.h"
#include "OBLoadoutSubsystem.generated.h"

class UOBWeaponCatalog;

/**
 * 개인 Loadout의 런타임 소유자(GameInstance 수명 = 세션 내내, 맵 전환에도 생존).
 * - Home에서 무기 선택 → SetWeapon() → 즉시 디스크 저장.
 * - 세션 맵 진입 시 클라 컨트롤러가 GetSelectedClasses()를 서버로 push.
 */
UCLASS(BlueprintType)
class OUTBREAK_API UOBLoadoutSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// 슬롯 무기 지정(로컬 갱신 + 즉시 저장). Home UI에서 호출.
	UFUNCTION(BlueprintCallable, Category = "Loadout")
	void SetWeapon(EOBWeaponSlot Slot, TSubclassOf<AOBWeaponBase> WeaponClass);

	// 현재 Loadout 읽기.
	const FOBLoadout& GetLoadout() const { return CurrentLoadout; }

	// 서버 push용: 소프트 클래스들을 로드해 하드 클래스 배열로 변환.
	UFUNCTION(BlueprintCallable, Category = "Loadout")
	TArray<TSubclassOf<AOBWeaponBase>> GetSelectedClasses() const;
	
	// 창고(미장착 보유) 무기 클래스들(로드 후 하드 클래스). 작업대 리스트용.
	UFUNCTION(BlueprintCallable, Category = "Loadout")
	TArray<TSubclassOf<AOBWeaponBase>> GetOwnedClasses() const;

	// 창고의 무기를 해당 슬롯에 장착(창고→슬롯, 기존 슬롯 무기는 창고로 반환).
	UFUNCTION(BlueprintCallable, Category = "Loadout")
	void EquipFromStash(TSubclassOf<AOBWeaponBase> WeaponClass);

	// 이미 보유(창고) 또는 장착 중인지.
	UFUNCTION(BlueprintPure, Category = "Loadout")
	bool IsOwnedOrEquipped(TSubclassOf<AOBWeaponBase> WeaponClass) const;
	
	// 사망 페널티: 장착 슬롯만 비움(창고는 유지) + 즉시 저장.
	UFUNCTION(BlueprintCallable, Category = "Loadout")
	void ClearLoadout();

	// 빈 로드아웃 여부(스타터킷 지급 판정).
	UFUNCTION(BlueprintPure, Category = "Loadout")
	bool IsEmpty() const { return CurrentLoadout.SlotWeapons.IsEmpty() && CurrentLoadout.OwnedWeapons.IsEmpty(); }
	
	// 창고+슬롯이 모두 비었을 때만 스타터 무기를 창고에 무료 지급. 이미 있으면 무동작.
	UFUNCTION(BlueprintCallable, Category = "Loadout")
	void GrantStarterIfEmpty(UOBWeaponCatalog* Catalog);
	
	// --- 통화 ---
	UFUNCTION(BlueprintPure, Category = "Currency")
	int32 GetCurrency() const { return CurrentCurrency; }

	// 재화 증감(음수 가능). 결과가 음수면 실패 반환하고 변경 안 함.
	UFUNCTION(BlueprintCallable, Category = "Currency")
	bool TrySpend(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Currency")
	void AddCurrency(int32 Amount);
	
	// 무기 카탈로그 + 현재 통화를 상점 뷰데이터로 변환.
	UFUNCTION(BlueprintCallable, Category = "Shop")
	FShopWindowViewData BuildShopView(UOBWeaponCatalog* Catalog) const;
	
	// 헤더
	UFUNCTION(BlueprintCallable, Category = "Shop")
	bool TryPurchase(UOBWeaponCatalog* Catalog, FName ItemId);
	
	// 헤더 public:
	UFUNCTION(BlueprintPure, Category = "Shop")
	static int32 GetWeaponPrice(TSubclassOf<AOBWeaponBase> WeaponClass);

	// 디스크 저장/로드.
	void SaveToDisk();
	void LoadFromDisk();

private:
	// 런타임 권위값(로드/편집 대상).
	FOBLoadout CurrentLoadout;

	// SaveGame 슬롯 이름(단일 프로필. 멀티프로필 시 확장).
	static const FString SlotName;
	static const int32 UserIndex = 0;
	
	int32 CurrentCurrency = 0;
};
