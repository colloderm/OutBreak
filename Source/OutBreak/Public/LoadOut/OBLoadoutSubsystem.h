// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Loadout/OBLoadoutTypes.h"
#include "OBLoadoutSubsystem.generated.h"

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
	
	// 사망 페널티: 전체 비우기 + 즉시 저장.
	UFUNCTION(BlueprintCallable, Category = "Loadout")
	void ClearLoadout();

	// 빈 로드아웃 여부(스타터킷 지급 판정).
	UFUNCTION(BlueprintPure, Category = "Loadout")
	bool IsEmpty() const { return CurrentLoadout.SlotWeapons.IsEmpty(); }

	// 디스크 저장/로드.
	void SaveToDisk();
	void LoadFromDisk();

private:
	// 런타임 권위값(로드/편집 대상).
	FOBLoadout CurrentLoadout;

	// SaveGame 슬롯 이름(단일 프로필. 멀티프로필 시 확장).
	static const FString SlotName;
	static const int32 UserIndex = 0;
};
