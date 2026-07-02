// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ShopWidgetCommands.generated.h"

/**
 * 상점 사용자 요청 구조체들의 공통 초기화를 제공한다.
 * 각 요청은 비동기 응답 순서를 구분할 수 있도록 생성 시 RequestId를 가진다.
 */
struct FShopRequestIdFactory
{
	static FGuid MakeRequestId()
	{
		return FGuid::NewGuid();
	}
};

/** 탭 변경 요청이다. ShopWindow가 현재 VendorId를 채워 외부 시스템으로 전달한다. */
USTRUCT(BlueprintType)
struct OUTBREAK_API FShopTabRequest
{
	GENERATED_BODY()

	FShopTabRequest()
		: RequestId(FShopRequestIdFactory::MakeRequestId())
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Request")
	FGuid RequestId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Request")
	FName VendorId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Request")
	FName TabId;
};

/** 카테고리 선택 후 아이템 목록을 요청하기 위한 Command다. */
USTRUCT(BlueprintType)
struct OUTBREAK_API FShopCategoryRequest
{
	GENERATED_BODY()

	FShopCategoryRequest()
		: RequestId(FShopRequestIdFactory::MakeRequestId())
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Request")
	FGuid RequestId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Request")
	FName VendorId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Request")
	FName TabId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Request")
	FName CategoryId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Request")
	FName SortId;
};

/** 중앙 목록에서 아이템을 선택했을 때 상세 정보를 요청하는 Command다. */
USTRUCT(BlueprintType)
struct OUTBREAK_API FShopItemSelectionRequest
{
	GENERATED_BODY()

	FShopItemSelectionRequest()
		: RequestId(FShopRequestIdFactory::MakeRequestId())
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Request")
	FGuid RequestId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Request")
	FName VendorId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Request")
	FName CategoryId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Request")
	FName ItemId;
};

/** 정렬 조건 변경을 외부 상점 시스템으로 전달하기 위한 Command다. */
USTRUCT(BlueprintType)
struct OUTBREAK_API FShopSortRequest
{
	GENERATED_BODY()

	FShopSortRequest()
		: RequestId(FShopRequestIdFactory::MakeRequestId())
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Request")
	FGuid RequestId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Request")
	FName VendorId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Request")
	FName CategoryId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Request")
	FName SortId;
};

/** 구매 버튼 입력을 거래 시스템으로 넘기기 위한 Command다. */
USTRUCT(BlueprintType)
struct OUTBREAK_API FShopPurchaseRequest
{
	GENERATED_BODY()

	FShopPurchaseRequest()
		: RequestId(FShopRequestIdFactory::MakeRequestId())
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Request")
	FGuid RequestId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Request")
	FName VendorId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Request")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Request")
	FName CurrencyId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Request")
	int32 Quantity = 1;
};

/** 교환 버튼 입력을 거래 시스템으로 넘기기 위한 Command다. */
USTRUCT(BlueprintType)
struct OUTBREAK_API FShopBarterRequest
{
	GENERATED_BODY()

	FShopBarterRequest()
		: RequestId(FShopRequestIdFactory::MakeRequestId())
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Request")
	FGuid RequestId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Request")
	FName VendorId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Request")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Request")
	int32 Quantity = 1;
};

/** 재고 새로고침 버튼 입력을 외부 시스템으로 넘기기 위한 Command다. */
USTRUCT(BlueprintType)
struct OUTBREAK_API FShopStockRefreshRequest
{
	GENERATED_BODY()

	FShopStockRefreshRequest()
		: RequestId(FShopRequestIdFactory::MakeRequestId())
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Request")
	FGuid RequestId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Request")
	FName VendorId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Request")
	FName CategoryId;
};
