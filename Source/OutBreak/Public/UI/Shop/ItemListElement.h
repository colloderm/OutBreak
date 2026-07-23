// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Shop/ShopWidgetTypes.h"
#include "ItemListElement.generated.h"

class UBorder;
class UButton;
class UTextBlock;
class UWidget;

UCLASS()
class OUTBREAK_API UItemListElement : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Shop")
	void SetItemData(const FShopItemViewData& InData);

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void SetInteractionEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Shop")
	FName GetItemId() const;

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void ResetElement();

	UPROPERTY(BlueprintAssignable, Category = "Shop")
	FShopItemSelectedSignature OnItemSelected;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_ItemName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_ItemPrice;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_ItemQty;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_ItemMeta;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UBorder> IMG_ItemThumb_Placeholder;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

protected:
	UFUNCTION()
	void HandleClicked();

	void BroadcastSelected();
	UButton* GetClickButton() const;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BTN_ItemRow;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BTN_ItemRow_0;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> BG_ItemRow_0_Outline;

	UPROPERTY(Transient)
	FShopItemViewData ItemData;

	bool bSelected = false;
	bool bInteractionEnabled = true;
};
