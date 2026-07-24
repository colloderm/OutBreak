// Fill out your copyright notice in the Description page of Project Settings.

#include "Interaction/OBStarterKitNPC.h"

#include "Dialogue/OBDialogueWidget.h"
#include "LoadOut/OBLoadoutSubsystem.h"
#include "Weapon/Data/OBWeaponTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void AOBStarterKitNPC::HandleAction(EOBDialogueAction Action)
{
	if (Action != EOBDialogueAction::GiveStarterKit) return;

	UOBLoadoutSubsystem* Loadout = GetGameInstance() ? GetGameInstance()->GetSubsystem<UOBLoadoutSubsystem>() : nullptr;
	if (!Loadout || !ActiveDialogue) return;

	if (!Loadout->IsEmpty())
	{
		ActiveDialogue->GoToNode(AlreadyHaveNode);
		return;
	}

	Loadout->SetWeapon(EOBWeaponSlot::Primary, StarterPrimary);
	Loadout->SetWeapon(EOBWeaponSlot::Secondary, StarterSecondary);
	// Loadout->SetWeapon(EOBWeaponSlot::Melee, StarterMelee);
	ActiveDialogue->GoToNode(GaveNode);
}
