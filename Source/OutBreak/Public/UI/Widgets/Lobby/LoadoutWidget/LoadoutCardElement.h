// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoadOut/OBLoadoutTypes.h"
#include "LoadoutCardElement.generated.h"

class UImage;
class UTexture2D;
class UTextBlock;
class AOBWeaponBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLoadoutCardClicked, EOBWeaponSlot, Slot);

UCLASS()
class OUTBREAK_API ULoadoutCardElement : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<UTextBlock> TXT_WeaponName;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<UTextBlock> TXT_WeaponType;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<UTextBlock> TXT_WeaponDesc;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidgetOptional))
	TObjectPtr<UImage> IMG_WeaponIcon;
	
	UPROPERTY(BlueprintAssignable, Category = "Loadout")
	FOnLoadoutCardClicked OnClicked;
	
public:
	void SetLoadoutCard(FText& inWeaponName, EOBWeaponSlot inType, FString& inDesc, UTexture2D* inIcon);	
	
protected:
	virtual void NativeConstruct() override;
	void SetWeaponName(FText& inWeaponName);
	void SetWeaponType(EOBWeaponSlot inType);
	void SetWeaponDesc(FString& inDesc);
	
	void SetIcon(UTexture2D* inIcon);
	
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	
protected:
	EOBWeaponSlot CardSlot = EOBWeaponSlot::Primary;
	
	UFUNCTION()
	void HandleButtonClicked();
};
