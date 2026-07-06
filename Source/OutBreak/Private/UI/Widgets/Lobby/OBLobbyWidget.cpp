// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/Lobby/OBLobbyWidget.h"

#include "UI/Widgets/Lobby/OBWeaponSelectWidget.h"
#include "UI/Widgets/Lobby/OBLoadoutWidget.h"
#include "Player/Controller/OBPlayerController.h"
#include "Player/State/OBPlayerStateBase.h"
#include "LoadOut/OBLoadoutSubsystem.h"
#include "TimerManager.h"

void UOBLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (WeaponSelect && WeaponCatalog)
	{
		WeaponSelect->BuildList(WeaponCatalog);
		WeaponSelect->OnWeaponChosen.AddUObject(this, &UOBLobbyWidget::HandleWeaponChosen);
	}

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
	
	// 3) 스탯 표시(UI 멤버 Loadout).
	if (Loadout) 
		Loadout->ShowStats(WeaponClass);
}

void UOBLobbyWidget::RefreshDynamic()
{
	UWorld* W = GetWorld();
	if (!W || W->bIsTearingDown) return;
	
	APlayerController* PC = GetOwningPlayer();
	AOBPlayerStateBase* PS = PC ? PC->GetPlayerState<AOBPlayerStateBase>() : nullptr;
	if (PS)
	{
		if (WeaponSelect)
			WeaponSelect->RefreshChecks(PS->GetSelectedWeapons());
		
		if (Loadout)      
			Loadout->RefreshLoadout(PS->GetSelectedWeapons());
	}
}
