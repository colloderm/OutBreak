// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/Lobby/LoadoutWidget/LoadoutSelectionList.h"

#include "UI/Widgets/Lobby/LoadoutWidget/WeaponElement.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"

UWeaponElement* ULoadoutSelectionList::AddWeaponElement(APlayerController* OwnerController, TSubclassOf<AOBWeaponBase> WeaponClass, 
	UTexture2D* inIcon, FText& inName, FText& inCategory)
{	
	if (!WeaponElementClass)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: WeaponElementClass 미지정 → WBP_LoadoutSelectionList의 Class Defaults에 WBP_WeaponElement를 지정하세요."), *GetName());
		return nullptr;
	}

	UWeaponElement* NewElement = CreateWidget<UWeaponElement>(OwnerController, WeaponElementClass);
	if (!IsValid(NewElement))
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s: Failed Create WeaponElement."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return nullptr;
	}
	NewElement->SetWeaponClass(WeaponClass);
	NewElement->SetElementData(inIcon, inName, inCategory);
	VBX_WeaponElementList->AddChildToVerticalBox(NewElement);
	
	return NewElement;
}

void ULoadoutSelectionList::ClearElements()
{
	if (VBX_WeaponElementList)
	{
		VBX_WeaponElementList->ClearChildren();
	}
}

void ULoadoutSelectionList::RemoveWeaponElement(UWeaponElement* inWidgetPtr)
{
	inWidgetPtr->RemoveFromParent();
}

void ULoadoutSelectionList::NativeConstruct()
{
	Super::NativeConstruct();
}
