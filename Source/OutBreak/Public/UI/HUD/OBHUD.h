// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "OBHUD.generated.h"

class UOBWorldMapWidget;
class UOBConsumableWidget;
class UOBAmmoViewModel;
class AOBWeaponBase;
class UUserWidget;
class UOBHealthViewModel;
class AOBCharacterBase;

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
	
	void BindAmmoToCharacter(AOBCharacterBase* Character);
	
	// 무기 교체 시 VM 재바인딩.
	void HandleWeaponChanged(AOBWeaponBase* NewWeapon);
	
	void BindConsumablesToCharacter(AOBCharacterBase* Character);
	
protected:
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> HealthBarWidgetClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> HealthWidget;

	UPROPERTY()
	TObjectPtr<UOBHealthViewModel> HealthViewModel;
	
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> AmmoWidgetClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> AmmoWidget;

	UPROPERTY()
	TObjectPtr<UOBAmmoViewModel> AmmoViewModel;
	
	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	TSubclassOf<UOBConsumableWidget> ConsumableWidgetClass;
	
	UPROPERTY()
	TObjectPtr<UOBConsumableWidget> ConsumableWidget;
	
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> SessionTimerWidgetClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> SessionTimerWidget;
	
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> CrosshairWidgetClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> CrosshairWidget;
	
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UOBWorldMapWidget> WorldMapWidgetClass;

	UPROPERTY()
	TObjectPtr<UOBWorldMapWidget> WorldMapWidget;
	
public:
	// PC의 M키가 호출.
	void ToggleWorldMap();

	/** Creates the configured map widget on demand and returns it. */
	UOBWorldMapWidget* EnsureWorldMapWidget();

	/** Opens the map in insertion selection/read-only mode. */
	bool OpenInsertionMap(bool bCanSelectTarget);

	/** Closes the map and clears insertion-specific selection state. */
	void CloseInsertionMap();

	UOBWorldMapWidget* GetWorldMapWidget() const { return WorldMapWidget; }

	/**
	 * 헬기 삽입 중에는 게임플레이 HUD(체력·탄약·소모품·타이머·크로스헤어)를 감춘다.
	 * 전체 지도는 제외한다 — 삽입 지점 선택에 그대로 써야 한다.
	 * PC가 트랜짓 잠금/해제 시점에 호출한다.
	 */
	void SetGameplayHUDVisible(bool bVisible);

	bool IsGameplayHUDVisible() const { return bGameplayHUDVisible; }

private:
	void ApplyGameplayHUDVisibility();

	bool bGameplayHUDVisible = true;
};
