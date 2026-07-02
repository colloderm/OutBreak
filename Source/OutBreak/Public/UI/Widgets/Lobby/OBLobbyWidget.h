// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Weapon/Data/OBWeaponData.h"
#include "OBLobbyWidget.generated.h"

class UButton; 
class UTextBlock; 
class AOBWeaponBase; 
class UOBWeaponCatalog;
class UOBWeaponSelectWidget; 
class UOBLoadoutWidget; 
class UOBPlayerListWidget;

UCLASS()
class OUTBREAK_API UOBLobbyWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void InitLobby(UOBWeaponCatalog* InCatalog) { WeaponCatalog = InCatalog; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	void HandleWeaponChosen(TSubclassOf<AOBWeaponBase> WeaponClass, EOBWeaponSlot WeaponSlot);
	
	UFUNCTION() 
	void HandleReadyClicked();
	
	UFUNCTION() 
	void HandleStartClicked();
	
	void RefreshDynamic();

	UPROPERTY(meta=(BindWidget)) 
	TObjectPtr<UOBWeaponSelectWidget> WeaponSelect;
	
	UPROPERTY(meta=(BindWidget)) 
	TObjectPtr<UOBLoadoutWidget> Loadout;
	
	UPROPERTY(meta=(BindWidget)) 
	TObjectPtr<UOBPlayerListWidget> PlayerListW;
	
	UPROPERTY(meta=(BindWidget)) 
	TObjectPtr<UButton> ReadyButton;
	
	UPROPERTY(meta=(BindWidget)) 
	TObjectPtr<UButton> StartButton;
	
	UPROPERTY(meta=(BindWidgetOptional)) 
	TObjectPtr<UTextBlock> HostCountText;

	UPROPERTY(EditAnywhere, Category="Lobby") 
	TObjectPtr<UOBWeaponCatalog> WeaponCatalog;

	bool bMyReady = false;
	FTimerHandle RefreshTimer;
};
