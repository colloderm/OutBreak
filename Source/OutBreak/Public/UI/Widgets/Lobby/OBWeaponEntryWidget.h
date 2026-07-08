// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Weapon/Data/OBWeaponData.h"
#include "OBWeaponEntryWidget.generated.h"

class UButton; 
class UTextBlock; 
class UImage;
class AOBWeaponBase;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOBOnWeaponEntryClicked, TSubclassOf<AOBWeaponBase>, EOBWeaponSlot);

UCLASS()
class OUTBREAK_API UOBWeaponEntryWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void Setup(TSubclassOf<AOBWeaponBase> InWeaponClass);
	void SetSelected(bool bSelected);
	TSubclassOf<AOBWeaponBase> GetWeaponClass() const { return WeaponClass; }
	EOBWeaponSlot GetSlotType() const { return SlotType; }

	FOBOnWeaponEntryClicked OnEntryClicked;

protected:
	virtual void NativeConstruct() override;
	
	UFUNCTION() 
	void HandleClicked();

	UPROPERTY(meta = (BindWidget)) 
	TObjectPtr<UButton> RootButton;
	
	UPROPERTY(meta = (BindWidget)) 
	TObjectPtr<UImage> IconImage;
	
	UPROPERTY(meta = (BindWidget)) 
	TObjectPtr<UTextBlock> NameText;
	
	UPROPERTY(meta = (BindWidget)) 
	TObjectPtr<UImage> CheckImage;

	TSubclassOf<AOBWeaponBase> WeaponClass;
	EOBWeaponSlot SlotType = EOBWeaponSlot::Primary;
};
