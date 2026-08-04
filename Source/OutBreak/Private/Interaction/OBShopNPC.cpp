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
	ActiveShop->OnTabChanged.AddDynamic(this, &AOBShopNPC::HandleTabChanged);
	ActiveShop->OnShopCloseRequested.AddDynamic(this, &AOBShopNPC::OnShopClosed);

	Loadout->GrantStarterIfEmpty(WeaponCatalog);   // 무일푼 소프트락 방지(카탈로그는 이제 이 용도로만 쓴다)
	RefreshShopView(true);
}

void AOBShopNPC::RefreshShopView(bool bResetSelection)
{
	UOBLoadoutSubsystem* Loadout = GetGameInstance() ? GetGameInstance()->GetSubsystem<UOBLoadoutSubsystem>() : nullptr;
	if (!Loadout || !ActiveShop) return;

	const FShopWindowViewData Data = Loadout->BuildShopView(ActiveShop->GetActiveTab());

	// 탭을 바꾸면 항목이 통째로 달라지므로 선택을 유지하면 안 된다.
	if (bResetSelection) 
		ActiveShop->InitializeShop(Data);
	else 
		ActiveShop->RefreshShop(Data);
}

void AOBShopNPC::HandleTabChanged(FName ShopId, EShopTab Tab)
{
	RefreshShopView(true);
}

void AOBShopNPC::OnPurchaseRequested(FName ShopId, FName ItemId, FName ActionId, int32 Quantity)
{
	UOBLoadoutSubsystem* Loadout = GetGameInstance() ? GetGameInstance()->GetSubsystem<UOBLoadoutSubsystem>() : nullptr;
	if (!Loadout) return;

	// 목록의 ItemId는 아이템 태그 문자열이다.
	const FGameplayTag ItemTag = FGameplayTag::RequestGameplayTag(ItemId, /*ErrorIfNotFound*/ false);
	if (!ItemTag.IsValid()) return;

	if (Loadout->TryPurchaseItem(ItemTag, FMath::Max(1, Quantity)))
		RefreshShopView(false);   // 잔액/보유 갱신
}

void AOBShopNPC::OnSellRequested(FName ShopId, FName ItemId, FName ActionId, int32 Quantity)
{
	UOBLoadoutSubsystem* Loadout = GetGameInstance() ? GetGameInstance()->GetSubsystem<UOBLoadoutSubsystem>() : nullptr;
	if (!Loadout) return;

	const FGameplayTag ItemTag = FGameplayTag::RequestGameplayTag(ItemId, /*ErrorIfNotFound*/ false);
	if (!ItemTag.IsValid()) return;

	if (Loadout->TrySell(ItemTag, FMath::Max(1, Quantity)))
		RefreshShopView(false);
}

void AOBShopNPC::OnShopClosed(FName ShopId)
{
	CloseShop();
}

void AOBShopNPC::CloseShop()
{
	ActiveShop = nullptr;
}
