// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Shop/ShopWidgetTypes.h"
#include "ShopItemList.generated.h"

class UItemListElement;
class UVerticalBox;

UCLASS()
class OUTBREAK_API UShopItemList : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Shop")
	void SetItems(const TArray<FShopItemViewData>& InItems);

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void ClearItems();

	UFUNCTION(BlueprintCallable, Category = "Shop")
	bool SelectItem(FName ItemId);

	UFUNCTION(BlueprintPure, Category = "Shop")
	FName GetSelectedItemId() const;

	const FShopItemViewData* FindDisplayedItem(FName ItemId) const;

	UPROPERTY(BlueprintAssignable, Category = "Shop")
	FShopItemSelectedSignature OnItemSelected;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UVerticalBox> VBX_ItemList;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop")
	TSubclassOf<UItemListElement> ItemElementClass;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

protected:
	UFUNCTION()
	void HandleItemElementSelected(FName ItemId);

	void ResolveItemElementClass();
	void ClearItemElements();

	UPROPERTY(Transient)
	TArray<FShopItemViewData> DisplayedItems;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UItemListElement>> ItemElements;

	FName SelectedItemId = NAME_None;
};
