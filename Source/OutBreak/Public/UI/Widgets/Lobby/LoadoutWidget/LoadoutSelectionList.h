// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoadoutSelectionList.generated.h"


class UVerticalBox;
class UWeaponElement;
/**
 * 
 */
UCLASS()
class OUTBREAK_API ULoadoutSelectionList : public UUserWidget
{
	GENERATED_BODY()
	
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<UVerticalBox> VBX_WeaponElementList;
	
	void AddWeaponElement(APlayerController* OwnerController, UTexture2D* inIcon, FText& inName, FText& inCategory);
	/* 미완성 기능 사용 여부 불확실. */
	void RemoveWeaponElement(UWeaponElement* inWidgetPtr);
	
protected:
	virtual void NativeConstruct() override;
	
	
private:
	// Weapon 관련 데이터 베이스 참조가 필요할듯.
};
