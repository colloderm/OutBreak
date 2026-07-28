// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoadoutSelectionList.generated.h"

class UVerticalBox;
class UWeaponElement;
class AOBWeaponBase;

UCLASS()
class OUTBREAK_API ULoadoutSelectionList : public UUserWidget
{
	GENERATED_BODY()
	
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta =(BindWidget))
	TObjectPtr<UVerticalBox> VBX_WeaponElementList;
	
	// 리스트에 생성할 무기 요소 위젯(WBP_WeaponElement 지정). 비면 C++ 기본.
	UPROPERTY(EditAnywhere, Category = "Loadout")
	TSubclassOf<UWeaponElement> WeaponElementClass;
	
	// 무기 요소 1개 추가 후 반환(호출측이 OnClicked 바인딩).
	UWeaponElement* AddWeaponElement(APlayerController* OwnerController, TSubclassOf<AOBWeaponBase> WeaponClass,
		UTexture2D* inIcon, FText& inName, FText& inCategory);
	
	// 리스트 비우기(재빌드용).
	void ClearElements();
	
	/* 미완성 기능 사용 여부 불확실. */
	void RemoveWeaponElement(UWeaponElement* inWidgetPtr);
	
protected:
	virtual void NativeConstruct() override;
	
	
private:
	// Weapon 관련 데이터 베이스 참조가 필요할듯.
};
