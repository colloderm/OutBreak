// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/Lobby/OBWeaponSelectWidget.h"

#include "UI/Widgets/Lobby/OBWeaponEntryWidget.h"
#include "Weapon/Data/OBWeaponCatalog.h"
#include "Components/ScrollBox.h"
#include "Weapon/OBWeaponBase.h"

void UOBWeaponSelectWidget::BuildList(UOBWeaponCatalog* Catalog)
{
	if (!Catalog || !EntryWidgetClass) return;
	if (PrimaryBox) PrimaryBox->ClearChildren();
	if (SecondaryBox) SecondaryBox->ClearChildren();
	if (MeleeBox) MeleeBox->ClearChildren();
	AllEntries.Reset();

	for (const TSubclassOf<AOBWeaponBase>& W : Catalog->AvailableWeapons)
	{
		if (!W) continue;
		UOBWeaponEntryWidget* Entry = CreateWidget<UOBWeaponEntryWidget>(this, EntryWidgetClass);
		if (!Entry) continue;
		Entry->Setup(W);
		Entry->OnEntryClicked.AddUObject(this, &UOBWeaponSelectWidget::HandleEntryClicked);
		AllEntries.Add(Entry);

		switch (Entry->GetSlotType())
		{
		case EOBWeaponSlot::Primary:   if (PrimaryBox)   PrimaryBox->AddChild(Entry);   break;
		case EOBWeaponSlot::Secondary: if (SecondaryBox) SecondaryBox->AddChild(Entry); break;
		case EOBWeaponSlot::Melee:     if (MeleeBox)     MeleeBox->AddChild(Entry);     break;
		}
	}
}

void UOBWeaponSelectWidget::RefreshChecks(const TArray<TSubclassOf<AOBWeaponBase>>& Selected)
{
	for (UOBWeaponEntryWidget* E : AllEntries)
	{
		if (E) E->SetSelected(Selected.Contains(E->GetWeaponClass()));
	}
}

void UOBWeaponSelectWidget::HandleEntryClicked(TSubclassOf<AOBWeaponBase> WeaponClass, EOBWeaponSlot WeaponSlot)
{
	OnWeaponChosen.Broadcast(WeaponClass, WeaponSlot);
}