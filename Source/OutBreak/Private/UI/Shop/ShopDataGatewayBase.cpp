// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Shop/ShopDataGatewayBase.h"

void UShopDataGatewayBase::RequestInitialShopState_Implementation(const FShopInitializationData& InitializationData)
{
}

void UShopDataGatewayBase::RequestCategoryItems_Implementation(const FShopCategoryRequest& Request)
{
}

void UShopDataGatewayBase::RequestItemDetail_Implementation(const FShopItemSelectionRequest& Request)
{
}

void UShopDataGatewayBase::SubmitPurchase_Implementation(const FShopPurchaseRequest& Request)
{
}

void UShopDataGatewayBase::SubmitBarter_Implementation(const FShopBarterRequest& Request)
{
}

void UShopDataGatewayBase::RequestStockRefresh_Implementation(const FShopStockRefreshRequest& Request)
{
}

void UShopDataGatewayBase::CancelPendingRequests_Implementation()
{
}

void UShopDataGatewayBase::ShutdownGateway_Implementation()
{
}
