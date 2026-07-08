// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Shop/ShopWidgetTypes.h"
#include "ShopItemInspector.generated.h"

class UBorder;
class UItemStatElement;
class UKeyBindableBtn;
class UTextBlock;
class UVerticalBox;

UCLASS()
class OUTBREAK_API UShopItemInspector : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Shop")
	void SetItemData(const FShopItemViewData& InData);

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void ClearItemData();

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void RefreshActionStates(const TArray<FShopActionViewData>& InActions);

	UPROPERTY(BlueprintAssignable, Category = "Shop")
	FShopActionTriggeredSignature OnActionTriggered;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_InspectorTitle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_InspectorQty;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_InspectorMeta;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UBorder> IMG_InspectorPreview_Placeholder;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_InspectorDescription;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UVerticalBox> VBX_ItemStatList;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_InspectorPriceValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UKeyBindableBtn> BTN_0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UKeyBindableBtn> BTN_1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop")
	TSubclassOf<UItemStatElement> ItemStatElementClass;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

protected:
	UFUNCTION()
	void HandleActionTriggered(FName ActionId);

	void ResolveItemStatElementClass();
	void ClearStatElements();
	void RebuildStats(const TArray<FShopItemStatViewData>& InStats);
	TArray<UKeyBindableBtn*> GetActionButtons() const;

	UPROPERTY(Transient)
	FShopItemViewData ItemData;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UItemStatElement>> StatElements;
};
