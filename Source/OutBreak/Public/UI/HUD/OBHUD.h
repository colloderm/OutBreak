// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "OBHUD.generated.h"

class UUserWidget;
class UOBHealthViewModel;
class AOBCharacterBase;
/**
 * 
 */
UCLASS()
class OUTBREAK_API AOBHUD : public AHUD
{
	GENERATED_BODY()
protected:
	virtual void BeginPlay() override;
	
	/*
	왜 호출되는가? - 조종 폰이 정해지거나 바뀔 때 체력 UI 초기화를 시도.
	언제 호출되는가? - 컨트롤러의 OnPossessedPawnChanged 발화 시.
	서버/클라? - 로컬 클라이언트.
	*/
	UFUNCTION()
	void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn);
	
	// ASC 준비 여부에 따라 즉시 초기화하거나 델리게이트를 기다린다.
	void TryInitHealthWidget(AOBCharacterBase* Character);

	// 실제 위젯/ViewModel 생성 + 주입.
	void InitHealthWidget(AOBCharacterBase* Character);
	
protected:
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> HealthBarWidgetClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> HealthWidget;

	UPROPERTY()
	TObjectPtr<UOBHealthViewModel> HealthViewModel;
};
