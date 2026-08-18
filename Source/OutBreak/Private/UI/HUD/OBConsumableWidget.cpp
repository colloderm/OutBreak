// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/HUD/OBConsumableWidget.h"

#include "Inventory/Components/PlayerInventoryComponent.h"
#include "Ability/Tags/OBGameplayTags.h"
#include "Components/Image.h"
#include "Item/OBItemRegistry.h"
#include "Components/TextBlock.h"

void UOBConsumableWidget::SetInventory(UPlayerInventoryComponent* InInventory)
{
	if (Inventory.IsValid() && ChangedHandle.IsValid())
		Inventory->OnInventoryChanged.Remove(ChangedHandle);
	ChangedHandle.Reset();

	Inventory = InInventory;
	BindInventoryChanged();

	Refresh();
}

void UOBConsumableWidget::BindInventoryChanged()
{
	if (Inventory.IsValid() && !ChangedHandle.IsValid())
	{
		ChangedHandle = Inventory->OnInventoryChanged.AddUObject(this, &UOBConsumableWidget::Refresh);
	}
}

void UOBConsumableWidget::Refresh()
{
	UPlayerInventoryComponent* Inv = Inventory.Get();
	const int32 Bandages = Inv ? Inv->GetItemCount(OBGameplayTags::Item_Bandage) : 0;
	const int32 Grenades = Inv ? Inv->GetItemCount(OBGameplayTags::Item_Grenade) : 0;

	if (BandageCountText) 
		BandageCountText->SetText(FText::AsNumber(Bandages));
	
	if (GrenadeCountText) 
		GrenadeCountText->SetText(FText::AsNumber(Grenades));
}

void UOBConsumableWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyIcons();

	// NativeDestruct가 구독을 끊는다. 뷰포트에서 뺐다 다시 넣는 경로(HUD 감춤/복귀,
	// 리스폰 등)에서 여기가 다시 붙이지 않으면 수량이 영영 갱신되지 않는다.
	BindInventoryChanged();
	Refresh();
}

void UOBConsumableWidget::ApplyIcons()
{
	// 아이콘 원본은 DT_Items 한 곳이다. BP에 박아두면 CSV와 어긋난다.
	auto Apply = [](UImage* Target, const FGameplayTag& ItemTag)
	{
		if (!Target) return;

		FText UnusedName;
		UTexture2D* Icon = nullptr;
		UOBItemRegistry::GetItemDisplay(ItemTag, UnusedName, Icon);

		if (Icon)
		{
			Target->SetBrushFromTexture(Icon);
			return;
		}

		// 조용히 빈칸으로 두면 원인을 못 찾는다.
		UE_LOG(LogTemp, Warning, TEXT("[Consumable] %s 의 아이콘을 DT_Items에서 못 찾았다. 행 존재 여부와 Icon 열을 확인할 것."), *ItemTag.ToString());
	};

	Apply(BandageIcon, OBGameplayTags::Item_Bandage);
	Apply(GrenadeIcon, OBGameplayTags::Item_Grenade);
}

void UOBConsumableWidget::NativeDestruct()
{
	if (Inventory.IsValid() && ChangedHandle.IsValid())
		Inventory->OnInventoryChanged.Remove(ChangedHandle);

	// 리셋하지 않으면 핸들이 유효한 채로 남아 BindInventoryChanged의 가드가 재구독을 막는다.
	ChangedHandle.Reset();

	Super::NativeDestruct();
}
