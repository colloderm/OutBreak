// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/Lobby/LoadoutWidget/LoadoutCardElement.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

void ULoadoutCardElement::SetLoadoutCard(FText& inWeaponName, EOBWeaponSlot inType, FString& inDesc, UTexture2D* inIcon)
{
	SetWeaponName(inWeaponName);
	SetWeaponType(inType);
	SetWeaponDesc(inDesc);
	SetIcon(inIcon);
}

void ULoadoutCardElement::SetWeaponName(FText& inWeaponName)
{
	TXT_WeaponName->SetText(inWeaponName);
}

void ULoadoutCardElement::SetWeaponType(EOBWeaponSlot inType)
{
	CardSlot = inType;
	const FText DisplayName = UEnum::GetDisplayValueAsText(inType);
	TXT_WeaponType->SetText(DisplayName);
}

void ULoadoutCardElement::SetWeaponDesc(FString& inDesc)
{
	TXT_WeaponDesc->SetText(FText::FromString(inDesc));
}

void ULoadoutCardElement::SetIcon(UTexture2D* inIcon)
{
	if (!IMG_WeaponIcon) return;
	if (inIcon)
	{
		IMG_WeaponIcon->SetBrushFromTexture(inIcon);
		IMG_WeaponIcon->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		IMG_WeaponIcon->SetVisibility(ESlateVisibility::Hidden); // 미선택 슬롯
	}
}

FReply ULoadoutCardElement::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	UE_LOG(LogTemp, Warning, TEXT("[LoadoutCard] Clicked slot=%d"), (int32)CardSlot);
	OnClicked.Broadcast(CardSlot);
	
	return FReply::Handled();
}

void ULoadoutCardElement::HandleButtonClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("[LoadoutCard] BtnClicked slot=%d"), (int32)CardSlot);
	OnClicked.Broadcast(CardSlot);
}

void ULoadoutCardElement::NativeConstruct()
{
	Super::NativeConstruct();
	
	SetVisibility(ESlateVisibility::Visible);

	if (WidgetTree)
	{
		WidgetTree->ForEachWidget([this](UWidget* W)
		{
			if (UButton* Btn = Cast<UButton>(W))
			{
				Btn->OnClicked.RemoveDynamic(this, &ULoadoutCardElement::HandleButtonClicked);
				Btn->OnClicked.AddDynamic(this, &ULoadoutCardElement::HandleButtonClicked);
			}
		});
	}
}
