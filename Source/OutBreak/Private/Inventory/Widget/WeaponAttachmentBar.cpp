// Fill out your copyright notice in the Description page of Project Settings.

#include "Inventory/Widget/WeaponAttachmentBar.h"

#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Data/OBGameDataSubsystem.h"
#include "Inventory/Components/PlayerInventoryComponent.h"
#include "Inventory/Widget/InventorySlot.h"
#include "Weapon/Data/OBWeaponDefinition.h"

void UWeaponAttachmentBar::SetInventorySource(UPlayerInventoryComponent* InInventory)
{
	InventoryComponent = InInventory;
	RefreshAttachmentBar();
}

void UWeaponAttachmentBar::NativePreConstruct()
{
	Super::NativePreConstruct();
	RefreshAttachmentBar();
}

void UWeaponAttachmentBar::RefreshAttachmentBar()
{
	if (!IsValid(AttachmentSlotBox) || !AttachmentSlotClass) return;

	AttachmentSlotBox->ClearChildren();

	FInventoryData Weapon;
	if (!InventoryComponent || !InventoryComponent->GetEquippedItem(TargetWeaponSlot, Weapon) || !Weapon.ItemTag.IsValid())
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const UOBGameDataSubsystem* GameData = UOBGameDataSubsystem::Get();
	const FOBWeaponDefinitionRow* Definition = GameData ? GameData->FindWeapon(Weapon.ItemTag) : nullptr;
	if (!Definition || Definition->AttachmentSlots.IsEmpty())
	{
		// 근접무기처럼 부착 슬롯이 없는 무기는 줄 자체를 숨긴다.
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	for (const FOBAttachmentSlotSpec& Spec : Definition->AttachmentSlots)
	{
		UInventorySlot* SlotWidget = CreateWidget<UInventorySlot>(this, AttachmentSlotClass);
		if (!SlotWidget) continue;

		// 설치된 게 없으면 빈 FInventoryData → UInventorySlot이 알아서 비운다.
		FInventoryData Display;
		if (const FOBInstalledAttachment* Installed = Weapon.Attachments.FindByPredicate(
			[&Spec](const FOBInstalledAttachment& Candidate)
			{
				return Candidate.SlotTag == Spec.SlotTag;
			}))
		{
			Display.ItemTag    = Installed->ItemTag;
			Display.InstanceId = Installed->InstanceId;
			Display.ItemStack  = 1;
		}

		// 우클릭 해제를 받으려면 컨텍스트와 히트 테스트가 둘 다 필요하다.
		SlotWidget->SetAttachmentContext(InventoryComponent, Weapon.InstanceId, Spec.SlotTag, Display);
		SlotWidget->SetVisibility(ESlateVisibility::Visible);

		// 칸마다 좌우 여백을 줘야 개별 박스로 보인다. 여백 0이면 배경이 이어져 한 덩어리가 된다.
		UPanelSlot* PanelSlot = AttachmentSlotBox->AddChild(SlotWidget);
		if (UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(PanelSlot))
		{
			BoxSlot->SetPadding(FMargin(SlotSpacing * 0.5f, 0.f));
			BoxSlot->SetHorizontalAlignment(HAlign_Center);
			BoxSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
}