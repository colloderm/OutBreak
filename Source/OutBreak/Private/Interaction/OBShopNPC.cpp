// Fill out your copyright notice in the Description page of Project Settings.

#include "Interaction/OBShopNPC.h"

#include "Player/Controller/OBPlayerController.h"
#include "LoadOut/OBLoadoutSubsystem.h"
#include "UI/Shop/ShopWindow.h"
#include "Engine/GameInstance.h"

void AOBShopNPC::HandleAction(EOBDialogueAction Action)
{
	if (Action == EOBDialogueAction::OpenShop) 
		OpenShop();
}

void AOBShopNPC::OpenShop()
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	UOBLoadoutSubsystem* Loadout = GetGameInstance() ? GetGameInstance()->GetSubsystem<UOBLoadoutSubsystem>() : nullptr;
	if (!PC || !Loadout || !ShopWindowClass) return;

	// 다이얼로그 먼저 닫기.
	if (AOBPlayerController* OBPC = Cast<AOBPlayerController>(PC)) 
		OBPC->CloseInteractionWidget();

	ActiveShop = CreateWidget<UShopWindow>(PC, ShopWindowClass);
	if (!ActiveShop) return;

	ActiveShop->OnPurchaseRequested.AddDynamic(this, &AOBShopNPC::OnPurchaseRequested);
	ActiveShop->OnShopCloseRequested.AddDynamic(this, &AOBShopNPC::OnShopClosed);

	ActiveShop->AddToViewport();
	Loadout->GrantStarterIfEmpty(WeaponCatalog);
	ActiveShop->InitializeShop(Loadout->BuildShopView(WeaponCatalog));

	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(ActiveShop->TakeWidget());
	PC->SetInputMode(Mode);
	PC->SetShowMouseCursor(true);
}

void AOBShopNPC::OnPurchaseRequested(FName ShopId, FName ItemId, FName ActionId, int32 Quantity)
{
	UOBLoadoutSubsystem* Loadout = GetGameInstance() ? GetGameInstance()->GetSubsystem<UOBLoadoutSubsystem>() : nullptr;
	if (!Loadout || !ActiveShop) return;

	if (Loadout->TryPurchase(WeaponCatalog, ItemId)) 
		ActiveShop->RefreshShop(Loadout->BuildShopView(WeaponCatalog));   // 잔액/보유 갱신
	
	// 실패(잔액 부족)는 위젯이 가격 대비 잔액으로 이미 표시. 필요 시 사운드 추가.
}

void AOBShopNPC::OnShopClosed(FName ShopId)
{
	CloseShop();
}

void AOBShopNPC::CloseShop()
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (PC)
	{
		FInputModeGameOnly Mode;
		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(false);
	}
	
	if (ActiveShop)
	{
		ActiveShop->RemoveFromParent();
		ActiveShop = nullptr;
	}
}
