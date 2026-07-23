// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WeaponStatView.generated.h"


class UTextBlock;
class UWeaponStatElement;

UENUM(BlueprintType)
enum class EStatTypes : uint8
{
	Damage UMETA(DisplayName = "Damage"),
	FireRate UMETA(DisplayName = "FireRate"),
	Accuracy UMETA(DisplayName = "Accuracy"),
	Recoil UMETA(DisplayName = "Recoil"),
	Mobility UMETA(DisplayName = "Mobility"),
};
/**
 * 
 */
UCLASS()
class OUTBREAK_API UWeaponStatView : public UUserWidget
{
	GENERATED_BODY()
	
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<UTextBlock> TXT_SelectedWeaponName;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<UWeaponStatElement> Stat_Damage;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<UWeaponStatElement> Stat_FireRate;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<UWeaponStatElement> Stat_Accuracy;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<UWeaponStatElement> Stat_Recoil;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<UWeaponStatElement> Stat_Mobility;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<UTextBlock> TXT_AmmoCount;
	
	
	void SetViewData(FText& inSelectedWeaponName,
		float inDamage, float inFireRate, float inAccuracy, float inRecoil, float inMobility, FText& inAmmoInfo);
	void SetStat(EStatTypes inStatType, float inPercent);
	
	
protected:
	virtual void NativeConstruct() override;
	
};
