// Fill out your copyright notice in the Description page of Project Settings.

#include "LoadOut/OBLoadoutSubsystem.h"

#include "SaveGame/OBSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Weapon/OBWeaponBase.h"

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

void UOBLoadoutSubsystem::SaveToDisk()
{
	UOBSaveGame* Save = Cast<UOBSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UOBSaveGame::StaticClass()));
	if (!Save) return;

	Save->Loadout = CurrentLoadout;
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
	}
}