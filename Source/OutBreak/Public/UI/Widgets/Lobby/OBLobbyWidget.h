// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Weapon/Data/OBWeaponData.h"
#include "OBLobbyWidget.generated.h"

class AOBWeaponBase; 
class UOBWeaponCatalog;
class UOBWeaponSelectWidget; 
class UOBLoadoutWidget; 

UCLASS()
class OUTBREAK_API UOBLobbyWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void InitLobby(UOBWeaponCatalog* InCatalog) { WeaponCatalog = InCatalog; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 무기 선택 시: 로컬 저장(GameInstance) + 서버 PlayerState 반영 + 스탯 표시.
	void HandleWeaponChosen(TSubclassOf<AOBWeaponBase> WeaponClass, EOBWeaponSlot WeaponSlot);
	
	// PlayerState.SelectedWeapons 기준으로 체크표시/로드아웃 갱신(0.3s 타이머).
	void RefreshDynamic();
	
protected:
	UPROPERTY(meta=(BindWidget)) 
	TObjectPtr<UOBWeaponSelectWidget> WeaponSelect;
	
	UPROPERTY(meta=(BindWidget)) 
	TObjectPtr<UOBLoadoutWidget> Loadout;

	UPROPERTY(EditAnywhere, Category="Lobby") 
	TObjectPtr<UOBWeaponCatalog> WeaponCatalog;

	FTimerHandle RefreshTimer;
};
