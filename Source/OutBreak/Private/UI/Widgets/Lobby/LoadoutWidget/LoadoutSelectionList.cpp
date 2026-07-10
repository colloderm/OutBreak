// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Widgets/Lobby/LoadoutWidget/LoadoutSelectionList.h"


#include "UI/Widgets/Lobby/LoadoutWidget/WeaponElement.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"

void ULoadoutSelectionList::AddWeaponElement(APlayerController* OwnerController, UTexture2D* inIcon, FText& inName, FText& inCategory)
{
	UWeaponElement* NewElement = CreateWidget<UWeaponElement>(OwnerController, UWeaponElement::StaticClass());
	if (!IsValid(NewElement))
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s: Failed Create, Widget is UWeaponElement UserWidget Class. "), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	NewElement->SetElementData(inIcon, inName, inCategory);
	VBX_WeaponElementList->AddChildToVerticalBox(NewElement);
}

void ULoadoutSelectionList::RemoveWeaponElement(UWeaponElement* inWidgetPtr)
{
	inWidgetPtr->RemoveFromParent();
}

void ULoadoutSelectionList::NativeConstruct()
{
	Super::NativeConstruct();
}
