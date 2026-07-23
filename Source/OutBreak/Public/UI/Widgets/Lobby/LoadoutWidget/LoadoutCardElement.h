// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoadOut/OBLoadoutTypes.h"
#include "LoadoutCardElement.generated.h"


class UTextBlock;
/**
 * 
 */
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
	
	
	
	void SetLoadoutCard(FText& inWeaponName, EOBWeaponSlot inType, FString& inDesc);
	
protected:
	virtual void NativeConstruct() override;
	void SetWeaponName(FText& inWeaponName);
	void SetWeaponType(EOBWeaponSlot inType);
	void SetWeaponDesc(FString& inDesc);
	
	
};
