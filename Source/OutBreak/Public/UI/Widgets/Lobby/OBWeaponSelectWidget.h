// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Weapon/Data/OBWeaponData.h"
#include "OBWeaponSelectWidget.generated.h"

class UScrollBox; 
class AOBWeaponBase; 
class UOBWeaponCatalog; 
class UOBWeaponEntryWidget;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOBOnWeaponChosen, TSubclassOf<AOBWeaponBase>, EOBWeaponSlot);

UCLASS()
class OUTBREAK_API UOBWeaponSelectWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void BuildList(UOBWeaponCatalog* Catalog);
	void RefreshChecks(const TArray<TSubclassOf<AOBWeaponBase>>& Selected);
	
	FOBOnWeaponChosen OnWeaponChosen;

protected:
	void HandleEntryClicked(TSubclassOf<AOBWeaponBase> WeaponClass, EOBWeaponSlot WeaponSlot);

	UPROPERTY(meta = (BindWidget)) 
	TObjectPtr<UScrollBox> PrimaryBox;
	
	UPROPERTY(meta = (BindWidget)) 
	TObjectPtr<UScrollBox> SecondaryBox;
	
	UPROPERTY(meta = (BindWidget)) 
	TObjectPtr<UScrollBox> MeleeBox;

	UPROPERTY(EditAnywhere, Category = "Lobby") 
	TSubclassOf<UOBWeaponEntryWidget> EntryWidgetClass;
	
	UPROPERTY() 
	TArray<TObjectPtr<UOBWeaponEntryWidget>> AllEntries;
};
