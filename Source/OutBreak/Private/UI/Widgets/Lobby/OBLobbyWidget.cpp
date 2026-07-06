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
#include "LoadOut/OBLoadoutSubsystem.h"

void UOBLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (WeaponSelect && WeaponCatalog)
	{
		WeaponSelect->BuildList(WeaponCatalog);
		WeaponSelect->OnWeaponChosen.AddUObject(this, &UOBLobbyWidget::HandleWeaponChosen);
	}
	if (ReadyButton)
		ReadyButton->OnClicked.AddDynamic(this, &UOBLobbyWidget::HandleReadyClicked);
	
	if (StartButton)
		StartButton->OnClicked.AddDynamic(this, &UOBLobbyWidget::HandleStartClicked);
	
	if (CloseButton)
		CloseButton->OnClicked.AddDynamic(this, &UOBLobbyWidget::HandleCloseClicked);

	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().SetTimer(RefreshTimer, this, &UOBLobbyWidget::RefreshDynamic, 0.3f, true);
		RefreshDynamic();
	}
}

void UOBLobbyWidget::NativeDestruct()
{
	if (UWorld* W = GetWorld()) 
		W->GetTimerManager().ClearTimer(RefreshTimer);
	
	Super::NativeDestruct();
}

void UOBLobbyWidget::HandleWeaponChosen(TSubclassOf<AOBWeaponBase> WeaponClass, EOBWeaponSlot WeaponSlot)
{
	// 1) 로컬 영속(즉시 저장) — 요구사항 "선택 시 바로 Loadout 저장".
	if (UGameInstance* GI = GetGameInstance())
		if (UOBLoadoutSubsystem* LoadoutSys = GI->GetSubsystem<UOBLoadoutSubsystem>())
			LoadoutSys ->SetWeapon(WeaponSlot, WeaponClass);

	// 2) 현재 맵 서버 PlayerState에도 반영(로비가 세션과 같은 서버일 때).
	if (AOBPlayerController* PC = GetOwningPlayer<AOBPlayerController>())
		PC->Server_SetWeaponSlot(WeaponSlot, WeaponClass);
	
	if (Loadout) 
		Loadout->ShowStats(WeaponClass);
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
		if (WeaponSelect)
			WeaponSelect->RefreshChecks(PS->GetSelectedWeapons());
		
		if (Loadout)      
			Loadout->RefreshLoadout(PS->GetSelectedWeapons());
		
		if (StartButton) 
			StartButton->SetIsEnabled(PS->IsPartyLeader()); // 팀원=비활성
		
		bMyReady = PS->IsReady();
	}
	
	if (PlayerListW) 
		PlayerListW->RefreshPlayers();
}

void UOBLobbyWidget::HandleCloseClicked()
{
	if (AOBPlayerController* PC = GetOwningPlayer<AOBPlayerController>())
		PC->CloseInteractionWidget();   // 커서/이동잠금 복구 + 위젯 제거
}
