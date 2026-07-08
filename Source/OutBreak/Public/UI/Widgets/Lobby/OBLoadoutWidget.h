// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Weapon/Data/OBWeaponData.h"
#include "OBLoadoutWidget.generated.h"

class UImage; 
class UTextBlock; 
class UProgressBar; 
class AOBWeaponBase;

UCLASS()
class OUTBREAK_API UOBLoadoutWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void RefreshLoadout(const TArray<TSubclassOf<AOBWeaponBase>>& Selected);
	void ShowStats(TSubclassOf<AOBWeaponBase> WeaponClass);

protected:
	UOBWeaponData* GetData(TSubclassOf<AOBWeaponBase> W) const;

	UPROPERTY(meta=(BindWidget)) 
	TObjectPtr<UImage> IconPrimary;
	
	UPROPERTY(meta=(BindWidget)) 
	TObjectPtr<UTextBlock> NamePrimary;
	
	UPROPERTY(meta=(BindWidget)) 
	TObjectPtr<UImage> IconSecondary;
	
	UPROPERTY(meta=(BindWidget)) 
	TObjectPtr<UTextBlock> NameSecondary;
	
	UPROPERTY(meta=(BindWidget)) 
	TObjectPtr<UImage> IconMelee;
	
	UPROPERTY(meta=(BindWidget)) 
	TObjectPtr<UTextBlock> NameMelee;

	
	UPROPERTY(meta=(BindWidget)) 
	TObjectPtr<UTextBlock> StatName;
	
	UPROPERTY(meta=(BindWidget)) 
	TObjectPtr<UProgressBar> BarDamage;
	
	UPROPERTY(meta=(BindWidget)) 
	TObjectPtr<UProgressBar> BarFireRate;
	
	UPROPERTY(meta=(BindWidget)) 
	TObjectPtr<UProgressBar> BarAccuracy;
	
	UPROPERTY(meta=(BindWidget)) 
	TObjectPtr<UProgressBar> BarRecoil;
	
	UPROPERTY(meta=(BindWidget)) 
	TObjectPtr<UProgressBar> BarMobility;
	
	UPROPERTY(meta=(BindWidget)) 
	TObjectPtr<UTextBlock> AmmoText;

	UPROPERTY(EditAnywhere, Category="Lobby") 
	float MaxDamage = 100.f;
	
	UPROPERTY(EditAnywhere, Category="Lobby") 
	float MaxRPM = 1000.f;
	
	UPROPERTY(EditAnywhere, Category="Lobby") 
	float MaxRecoil = 3.f;
	
	UPROPERTY(EditAnywhere, Category="Lobby") 
	float MaxSpread = 5.f;
};
