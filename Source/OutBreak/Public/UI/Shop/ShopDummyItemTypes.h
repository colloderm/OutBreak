// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Shop/ShopWidgetTypes.h"
#include "ShopDummyItemTypes.generated.h"

USTRUCT(BlueprintType)
struct OUTBREAK_API FShopDummyItemStatData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	FName StatId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	FText DisplayValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	int32 SortOrder = 0;

	FShopItemStatViewData ToViewData() const
	{
		FShopItemStatViewData OutData;
		OutData.StatId = StatId;
		OutData.DisplayName = DisplayName;
		OutData.DisplayValue = DisplayValue;
		OutData.SortOrder = SortOrder;
		return OutData;
	}
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FShopDummyItemActionData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	FName ActionId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	FText InputDisplayText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	FKey InputKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	int32 CostOverride = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	bool bCanExecute = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	FText DisabledReason;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	EShopActionType ActionType = EShopActionType::Generic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	int32 Quantity = 1;

	FShopActionViewData ToViewData(int32 DefaultCost) const
	{
		FShopActionViewData OutData;
		OutData.ActionId = ActionId;
		OutData.Label = Label;
		OutData.InputDisplayText = InputDisplayText;
		OutData.InputKey = InputKey;
		OutData.Cost = CostOverride >= 0 ? CostOverride : DefaultCost;
		OutData.bCanExecute = bCanExecute;
		OutData.DisabledReason = DisabledReason;
		OutData.ActionType = ActionType;
		OutData.Quantity = FMath::Max(1, Quantity);
		return OutData;
	}
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FShopDummyItemData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	FName CategoryId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	FText MetaText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	FSlateBrush ListIconBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	FSlateBrush DetailImageBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	int32 Price = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	int32 StockQuantity = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	int32 OwnedQuantity = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	bool bIsEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	int32 SortOrder = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	TArray<FShopDummyItemStatData> Stats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	TArray<FShopDummyItemActionData> Actions;

	FShopItemViewData ToViewData() const
	{
		FShopItemViewData OutData;
		OutData.ItemId = ItemId;
		OutData.CategoryId = CategoryId;
		OutData.DisplayName = DisplayName;
		OutData.MetaText = MetaText;
		OutData.Description = Description;
		OutData.ListIconBrush = ListIconBrush;
		OutData.DetailImageBrush = DetailImageBrush;
		OutData.Price = Price;
		OutData.StockQuantity = StockQuantity;
		OutData.OwnedQuantity = OwnedQuantity;
		OutData.bIsEnabled = bIsEnabled;
		OutData.SortOrder = SortOrder;

		OutData.Stats.Reserve(Stats.Num());
		for (const FShopDummyItemStatData& Stat : Stats)
		{
			OutData.Stats.Add(Stat.ToViewData());
		}

		OutData.Actions.Reserve(Actions.Num());
		for (const FShopDummyItemActionData& Action : Actions)
		{
			OutData.Actions.Add(Action.ToViewData(Price));
		}

		return OutData;
	}
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FShopDummyCategoryData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	FName CategoryId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	FSlateBrush IconBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	bool bIsEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	int32 SortOrder = 0;

	FShopCategoryViewData ToViewData(int32 ItemCount) const
	{
		FShopCategoryViewData OutData;
		OutData.CategoryId = CategoryId;
		OutData.DisplayName = DisplayName;
		OutData.ItemCount = ItemCount;
		OutData.IconBrush = IconBrush;
		OutData.bIsEnabled = bIsEnabled;
		OutData.SortOrder = SortOrder;
		return OutData;
	}
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FShopDummyShopData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	FName ShopId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	FShopkeeperViewData Shopkeeper;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	FUserCurrencyViewData Currency;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	TArray<FShopDummyCategoryData> Categories;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	TArray<FShopDummyItemData> Items;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	FName InitialSelectedCategoryId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	FName InitialSelectedItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Dummy")
	FText NewStockText;

	FShopWindowViewData ToWindowViewData() const
	{
		FShopWindowViewData OutData;
		OutData.ShopId = ShopId;
		OutData.Shopkeeper = Shopkeeper;
		OutData.Currency = Currency;
		OutData.InitialSelectedCategoryId = InitialSelectedCategoryId;
		OutData.InitialSelectedItemId = InitialSelectedItemId;
		OutData.NewStockText = NewStockText;

		OutData.Categories.Reserve(Categories.Num());
		for (const FShopDummyCategoryData& Category : Categories)
		{
			int32 ItemCount = 0;
			for (const FShopDummyItemData& Item : Items)
			{
				if (Item.CategoryId == Category.CategoryId)
				{
					++ItemCount;
				}
			}

			OutData.Categories.Add(Category.ToViewData(ItemCount));
		}

		OutData.Items.Reserve(Items.Num());
		for (const FShopDummyItemData& Item : Items)
		{
			OutData.Items.Add(Item.ToViewData());
		}

		return OutData;
	}
};
