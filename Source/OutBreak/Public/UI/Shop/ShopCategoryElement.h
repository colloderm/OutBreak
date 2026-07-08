// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Shop/ShopWidgetTypes.h"
#include "ShopCategoryElement.generated.h"

class UButton;
class UTextBlock;
class UWidget;

UCLASS()
class OUTBREAK_API UShopCategoryElement : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Shop")
	void SetCategoryData(const FShopCategoryViewData& InData);

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void SetInteractionEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Shop")
	FName GetCategoryId() const;

	UPROPERTY(BlueprintAssignable, Category = "Shop")
	FShopCategorySelectedSignature OnCategorySelected;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_Category_Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_Category_Count;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

protected:
	UFUNCTION()
	void HandleClicked();

	void BroadcastSelected();
	UButton* GetClickButton() const;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BTN_Category_All;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> BG_Category_All_Selected;

	UPROPERTY(Transient)
	FShopCategoryViewData CategoryData;

	bool bSelected = false;
	bool bInteractionEnabled = true;
};
