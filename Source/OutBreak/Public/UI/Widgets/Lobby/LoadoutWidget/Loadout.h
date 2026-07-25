// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Weapon/Data/OBWeaponTypes.h"
#include "Loadout.generated.h"

class ULoadoutSelectionList;
class ULoadoutSelectionView;
class UOBLoadoutSubsystem;
class AOBWeaponBase;

UCLASS()
class OUTBREAK_API ULoadout : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<ULoadoutSelectionList> LoadoutSelectionList;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<ULoadoutSelectionView> LoadoutSelectionView;
	
	// 스탯 바 정규화 최대값(밸런스에 맞춰 조정).
	UPROPERTY(EditAnywhere, Category = "Loadout") 
	float MaxDamage = 100.f;
	
	UPROPERTY(EditAnywhere, Category = "Loadout") 
	float MaxRPM = 1000.f;
	
	UPROPERTY(EditAnywhere, Category = "Loadout") 
	float MaxRecoil = 3.f;
	
	UPROPERTY(EditAnywhere, Category = "Loadout") 
	float MaxSpread = 5.f;
	
protected:
	virtual void NativeConstruct() override;
	
	UFUNCTION()
	void HandleWeaponClicked(TSubclassOf<AOBWeaponBase> WeaponClass);

	void RebuildStash();
	void RebuildSlots();
	void ShowStats(TSubclassOf<AOBWeaponBase> WeaponClass);
	
	UOBLoadoutSubsystem* GetLoadout() const;
	
	void ShowDefaultStats();
	
	void BindCardClicks();
	
	UFUNCTION()
	void HandleCardClicked(EOBWeaponSlot WeaponSlot);
};
