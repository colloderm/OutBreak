// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoadOut/OBLoadoutTypes.h"
#include "LoadoutSelectionView.generated.h"


class UVerticalBox;
class ULoadoutCardElement;
class UWeaponStatView;

/**
 * 
 */
UCLASS()
class OUTBREAK_API ULoadoutSelectionView : public UUserWidget
{
	GENERATED_BODY()
	
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<UVerticalBox> VBX_LoadoutCardList;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<ULoadoutCardElement> LoadoutCardElement_Primary;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<ULoadoutCardElement> LoadoutCardElement_Secondary;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<ULoadoutCardElement> LoadoutCardElement_Melee;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<UWeaponStatView> WeaponStatView;
	
	
	void SetCardInfo(FText& inName, EOBWeaponSlot inType, FString& inDesc);
	void SetStatView(FText& inSelectedWeaponName,
		float inDamage, float inFireRate, float inAccuracy, float inRecoil, float inMobility, FText& inAmmoInfo);
	
protected:
	virtual void NativeConstruct() override;
};
