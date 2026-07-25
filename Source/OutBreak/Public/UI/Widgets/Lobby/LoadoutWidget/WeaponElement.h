// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WeaponElement.generated.h"


class UImage;
class UTextBlock;
class UTexture2D;
class AOBWeaponBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponElementClicked, TSubclassOf<AOBWeaponBase>, WeaponClass);

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
	
	// 클릭 시 이 요소가 대표하는 무기 클래스를 브로드캐스트.
	UPROPERTY(BlueprintAssignable, Category = "Loadout")
	FOnWeaponElementClicked OnClicked;
	
public:
	void SetElementData(UTexture2D* inIcon, FText& inName, FText& inCategory);
	void SetIcon(UTexture2D* inIcon);
	void SetWeaponName(FText& inName);
	void SetCategory(FText& inCategory);

	void SetWeaponClass(TSubclassOf<AOBWeaponBase> In) { WeaponClass = In; }
	
protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	
	UFUNCTION()
	void HandleButtonClicked();
	
protected:
	UPROPERTY()
	TSubclassOf<AOBWeaponBase> WeaponClass;
};
