// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Widgets/Lobby/LoadoutWidget/WeaponElement.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Weapon/OBWeaponBase.h"

void UWeaponElement::SetElementData(UTexture2D* inIcon, FText& inName, FText& inCategory)
{
	SetIcon(inIcon);
	SetWeaponName(inName);
	SetCategory(inCategory);
}

void UWeaponElement::SetIcon(UTexture2D* inIcon)
{
	if (IMG_WeaponIcon)
		IMG_WeaponIcon->SetBrushFromTexture(inIcon);
}

void UWeaponElement::SetWeaponName(FText& inName)
{
	if (TXT_WeaponName)
		TXT_WeaponName->SetText(inName);
}

void UWeaponElement::SetCategory(FText& inCategory)
{
	if (TXT_Category)
		TXT_Category->SetText(inCategory);
}

void UWeaponElement::NativeConstruct()
{
	Super::NativeConstruct();
	
	SetVisibility(ESlateVisibility::Visible);
	
	if (WidgetTree)
	{
		WidgetTree->ForEachWidget([this](UWidget* W)
		{
			if (UButton* Btn = Cast<UButton>(W))
			{
				Btn->OnClicked.RemoveDynamic(this, &UWeaponElement::HandleButtonClicked);
				Btn->OnClicked.AddDynamic(this, &UWeaponElement::HandleButtonClicked);
			}
		});
	}
}

FReply UWeaponElement::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	UE_LOG(LogTemp, Warning, TEXT("[WeaponElement] Clicked=%s"), *GetNameSafe(WeaponClass));
	
	OnClicked.Broadcast(WeaponClass);
	return FReply::Handled();
}

void UWeaponElement::HandleButtonClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("[WeaponElement] BtnClicked=%s"), *GetNameSafe(WeaponClass));
	OnClicked.Broadcast(WeaponClass);
}
