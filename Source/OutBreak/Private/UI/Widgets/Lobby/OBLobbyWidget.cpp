// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/Lobby/OBLobbyWidget.h"

#include "UI/Widgets/Lobby/OBWeaponSelectWidget.h"
#include "UI/Widgets/Lobby/OBLoadoutWidget.h"
#include "UI/Widgets/Lobby/OBPlayerListWidget.h"
#include "Player/Controller/OBPlayerController.h"
#include "Player/State/OBPlayerStateBase.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "TimerManager.h"

void UOBLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (WeaponSelect && WeaponCatalog)
	{
		WeaponSelect->BuildList(WeaponCatalog);
		WeaponSelect->OnWeaponChosen.AddUObject(this, &UOBLobbyWidget::HandleWeaponChosen);
	}
	if (ReadyButton) ReadyButton->OnClicked.AddDynamic(this, &UOBLobbyWidget::HandleReadyClicked);
	if (StartButton)
	{
		StartButton->OnClicked.AddDynamic(this, &UOBLobbyWidget::HandleStartClicked);
		const bool bHost = GetOwningPlayer() && GetOwningPlayer()->HasAuthority();
		StartButton->SetVisibility(bHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeUIOnly Mode;
		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(true);
	}

	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().SetTimer(RefreshTimer, this, &UOBLobbyWidget::RefreshDynamic, 0.3f, true);
		RefreshDynamic();
	}
}

void UOBLobbyWidget::NativeDestruct()
{
	if (UWorld* W = GetWorld()) W->GetTimerManager().ClearTimer(RefreshTimer);
	Super::NativeDestruct();
}

void UOBLobbyWidget::HandleWeaponChosen(TSubclassOf<AOBWeaponBase> WeaponClass, EOBWeaponSlot WeaponSlot)
{
	if (AOBPlayerController* PC = GetOwningPlayer<AOBPlayerController>())
		PC->Server_SetWeaponSlot(WeaponSlot, WeaponClass);
	if (Loadout) Loadout->ShowStats(WeaponClass);
}

void UOBLobbyWidget::HandleReadyClicked()
{
	bMyReady = !bMyReady;
	if (AOBPlayerController* PC = GetOwningPlayer<AOBPlayerController>())
		PC->Server_SetReady(bMyReady);
}

void UOBLobbyWidget::HandleStartClicked()
{
	if (AOBPlayerController* PC = GetOwningPlayer<AOBPlayerController>())
		PC->Server_StartGame();
}

void UOBLobbyWidget::RefreshDynamic()
{
	APlayerController* PC = GetOwningPlayer();
	AOBPlayerStateBase* PS = PC ? PC->GetPlayerState<AOBPlayerStateBase>() : nullptr;
	if (PS)
	{
		if (WeaponSelect) WeaponSelect->RefreshChecks(PS->GetSelectedWeapons());
		if (Loadout)      Loadout->RefreshLoadout(PS->GetSelectedWeapons());
		bMyReady = PS->IsReady();
	}
	if (PlayerListW) PlayerListW->RefreshPlayers();
}