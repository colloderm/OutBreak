// Fill out your copyright notice in the Description page of Project Settings.

#include "Interaction/OBStarterKitNPC.h"

#include "Dialogue/OBDialogueWidget.h"
#include "LoadOut/OBLoadoutSubsystem.h"
#include "Weapon/OBWeaponBase.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void AOBStarterKitNPC::HandleAction(EOBDialogueAction Action)
{
	if (Action != EOBDialogueAction::GiveStarterKit) return;

	UOBLoadoutSubsystem* Loadout = GetGameInstance() ? GetGameInstance()->GetSubsystem<UOBLoadoutSubsystem>() : nullptr;
	if (!Loadout || !ActiveDialogue) return;

	// 빈 슬롯만 채운다. 권총 하나 있다고 주무기까지 막히면 안 된다.
	TArray<TSubclassOf<AOBWeaponBase>> Kit;
	Kit.Add(StarterPrimary);
	Kit.Add(StarterSecondary);
	Kit.Add(StarterMelee);

	const int32 Granted = Loadout->GrantMissingStarters(Kit);

	// 하나도 못 줬을 때만 "이미 가지고 있음" 분기.
	ActiveDialogue->GoToNode(Granted > 0 ? GaveNode : AlreadyHaveNode);
}
