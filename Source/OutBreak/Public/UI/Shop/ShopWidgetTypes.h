// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Styling/SlateBrush.h"
#include "ShopWidgetTypes.generated.h"

UENUM(BlueprintType)
enum class EShopActionType : uint8
{
	Generic,
	Purchase,
	Exchange
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FShopkeeperViewData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FName ShopkeeperId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FText RoleText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FText Subtitle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FSlateBrush PortraitBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FText ReputationName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	int32 ReputationValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	int32 NextReputationValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FText ReputationLevelText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FText NoteText;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FUserCurrencyViewData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	int32 Scrap = 0;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FShopCategoryViewData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FName CategoryId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	int32 ItemCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FSlateBrush IconBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	bool bIsEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	int32 SortOrder = 0;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FShopItemStatViewData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FName StatId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FText DisplayValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	int32 SortOrder = 0;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FShopActionViewData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FName ActionId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FText InputDisplayText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FKey InputKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	int32 Cost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	bool bCanExecute = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FText DisabledReason;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	EShopActionType ActionType = EShopActionType::Generic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	int32 Quantity = 1;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FShopItemViewData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FName CategoryId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FText MetaText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FSlateBrush ListIconBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FSlateBrush DetailImageBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	int32 Price = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	int32 StockQuantity = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	int32 OwnedQuantity = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	bool bIsEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	int32 SortOrder = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	TArray<FShopItemStatViewData> Stats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	TArray<FShopActionViewData> Actions;
};

USTRUCT(BlueprintType)
struct OUTBREAK_API FShopWindowViewData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FName ShopId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FShopkeeperViewData Shopkeeper;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FUserCurrencyViewData Currency;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	TArray<FShopCategoryViewData> Categories;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	TArray<FShopItemViewData> Items;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FName InitialSelectedCategoryId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FName InitialSelectedItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FText NewStockText;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShopCategorySelectedSignature, FName, CategoryId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShopItemSelectedSignature, FName, ItemId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShopActionTriggeredSignature, FName, ActionId);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FShopWindowCategorySelectionChangedSignature, FName, ShopId, FName, CategoryId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FShopWindowItemSelectionChangedSignature, FName, ShopId, FName, ItemId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FShopWindowActionRequestedSignature, FName, ShopId, FName, ItemId, FName, ActionId, int32, Quantity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShopWindowCloseRequestedSignature, FName, ShopId);
