// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WeaponElement.generated.h"


class UImage;
class UTextBlock;
class UTexture2D;

/**
 * 
 */
UCLASS()
class OUTBREAK_API UWeaponElement : public UUserWidget
{
	GENERATED_BODY()
	
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<UImage> IMG_WeaponIcon;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<UTextBlock> TXT_WeaponName;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<UTextBlock> TXT_Category;
	
	void SetElementData(UTexture2D* inIcon, FText& inName, FText& inCategory);
	void SetIcon(UTexture2D* inIcon);
	void SetWeaponName(FText& inName);
	void SetCategory(FText& inCategory);
	
protected:
	virtual void NativeConstruct() override;
};
