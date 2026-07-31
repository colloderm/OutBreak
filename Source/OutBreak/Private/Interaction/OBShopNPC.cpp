// Fill out your copyright notice in the Description page of Project Settings.

#include "Interaction/OBShopNPC.h"

#include "Player/Controller/OBPlayerController.h"
#include "LoadOut/OBLoadoutSubsystem.h"
#include "UI/Shop/ShopWindow.h"
#include "GameplayTagContainer.h"
#include "Engine/GameInstance.h"

void AOBShopNPC::HandleAction(EOBDialogueAction Action)
{
	if (Action == EOBDialogueAction::OpenShop) 
		OpenShop();
}

void AOBShopNPC::OpenShop()
{
	AOBPlayerController* PC = GetWorld() ? Cast<AOBPlayerController>(GetWorld()->GetFirstPlayerController()) : nullptr;
	UOBLoadoutSubsystem* Loadout = GetGameInstance() ? GetGameInstance()->GetSubsystem<UOBLoadoutSubsystem>() : nullptr;
	if (!PC || !Loadout || !ShopWindowClass) return;

	// 다이얼로그 먼저 닫기.
	PC->CloseInteractionWidget();

	ActiveShop = Cast<UShopWindow>(PC->OpenInteractionWidget(ShopWindowClass));
	if (!ActiveShop) return;

	ActiveShop->OnPurchaseRequested.AddDynamic(this, &AOBShopNPC::OnPurchaseRequested);
	ActiveShop->OnExchangeRequested.AddDynamic(this, &AOBShopNPC::OnSellRequested);
	ActiveShop->OnShopCloseRequested.AddDynamic(this, &AOBShopNPC::OnShopClosed);

	Loadout->GrantStarterIfEmpty(WeaponCatalog);
	ActiveShop->InitializeShop(Loadout->BuildShopView(WeaponCatalog));
}

void AOBShopNPC::OnPurchaseRequested(FName ShopId, FName ItemId, FName ActionId, int32 Quantity)
{
	UOBLoadoutSubsystem* Loadout = GetGameInstance() ? GetGameInstance()->GetSubsystem<UOBLoadoutSubsystem>() : nullptr;
	if (!Loadout || !ActiveShop) return;

	if (Loadout->TryPurchase(WeaponCatalog, ItemId)) 
		ActiveShop->RefreshShop(Loadout->BuildShopView(WeaponCatalog));   // 잔액/보유 갱신
	
	// 실패(잔액 부족)는 위젯이 가격 대비 잔액으로 이미 표시. 필요 시 사운드 추가.
}

void AOBShopNPC::OnSellRequested(FName ShopId, FName ItemId, FName ActionId, int32 Quantity)
{
	UOBLoadoutSubsystem* Loadout = GetGameInstance() ? GetGameInstance()->GetSubsystem<UOBLoadoutSubsystem>() : nullptr;
	if (!Loadout || !ActiveShop) return;

	// 판매 목록의 ItemId는 아이템 태그 문자열이다.
	const FGameplayTag ItemTag = FGameplayTag::RequestGameplayTag(ItemId, /*ErrorIfNotFound*/ false);
	if (!ItemTag.IsValid()) return;

	if (Loadout->TrySell(ItemTag, FMath::Max(1, Quantity)))
		ActiveShop->RefreshShop(Loadout->BuildShopView(WeaponCatalog));   // 잔액/보유 갱신
}

void AOBShopNPC::OnShopClosed(FName ShopId)
{
	CloseShop();
}

void AOBShopNPC::CloseShop()
{
	ActiveShop = nullptr;
}
