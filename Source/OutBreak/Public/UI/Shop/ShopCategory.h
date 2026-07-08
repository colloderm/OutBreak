// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Shop/ShopWidgetTypes.h"
#include "ShopCategory.generated.h"

class UShopCategoryElement;
class UTextBlock;
class UVerticalBox;

UCLASS()
class OUTBREAK_API UShopCategory : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Shop")
	void SetCategories(const TArray<FShopCategoryViewData>& InCategories);

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void ClearCategories();

	UFUNCTION(BlueprintCallable, Category = "Shop")
	bool SelectCategory(FName CategoryId);

	UFUNCTION(BlueprintPure, Category = "Shop")
	FName GetSelectedCategoryId() const;

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void SetNewStockTime(const FText& InNewStockText);

	UPROPERTY(BlueprintAssignable, Category = "Shop")
	FShopCategorySelectedSignature OnCategorySelected;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<UVerticalBox> VBX_CategoryList;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<UTextBlock> TXT_NewStockTime;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop")
	TSubclassOf<UShopCategoryElement> CategoryElementClass;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

protected:
	UFUNCTION()
	void HandleCategoryElementSelected(FName CategoryId);

	void ResolveCategoryElementClass();
	void ClearCategoryElements();

	UPROPERTY(Transient)
	TArray<FShopCategoryViewData> Categories;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UShopCategoryElement>> CategoryElements;

	FName SelectedCategoryId = NAME_None;
};
