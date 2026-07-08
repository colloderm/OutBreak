// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/HUD/OBHUD.h"

#include "UI/ViewModels/OBHealthViewModel.h"
#include "Character/OBCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "UI/ViewModels/OBAmmoViewModel.h"
#include "Equipment/Components/OBEquipmentComponent.h"
#include "Inventory/Components/OBInventoryComponent.h"
#include "UI/HUD/OBConsumableWidget.h"
#include "View/MVVMView.h"

void AOBHUD::BeginPlay()
{
	Super::BeginPlay();
	
	APlayerController* PC = GetOwningPlayerController();
	if (!PC) return;
	
	// 폰 변경 구독(클라에서 폰이 늦게 도착하는 경우 대비).
	PC->OnPossessedPawnChanged.AddDynamic(this, &AOBHUD::HandlePawnChanged);
	
	// 이미 폰이 있으면 즉시 처리.
	if (APawn* CurrentPawn = PC->GetPawn())
	{
		HandlePawnChanged(nullptr, CurrentPawn);
	}
}

void AOBHUD::HandlePawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	if (AOBCharacterBase* Character = Cast<AOBCharacterBase>(NewPawn))
	{
		TryInitHealthWidget(Character);
		BindAmmoToCharacter(Character);
		BindConsumablesToCharacter(Character);
	}
}

void AOBHUD::TryInitHealthWidget(AOBCharacterBase* Character)
{
	if (!Character) return;
	
	if (Character->GetAbilitySystemComponent())
	{
		InitHealthWidget(Character);
	}
	else
	{
		// ASC가 아직이면 초기화 완료 시점에 다시 시도(약참조: HUD 파괴 후 호출 방지).
		Character->OnAbilitySystemInitialized.AddWeakLambda(this, [this, Character]()
		{
			InitHealthWidget(Character);
		});
	}
}

void AOBHUD::InitHealthWidget(AOBCharacterBase* Character)
{
	// 중복 생성/유효성 가드
	if (HealthWidget || !Character || !HealthBarWidgetClass) return;
	
	UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	if (!ASC) return;
	
	// 1) ViewModel 생성 + ASC 연결(초기값/구독)
	HealthViewModel = NewObject<UOBHealthViewModel>(this);
	HealthViewModel->SetAbilitySystemComponent(ASC);
	
	// 2) 위젯 생성 + 뷰포트 추가
	HealthWidget = CreateWidget<UUserWidget>(GetOwningPlayerController(), HealthBarWidgetClass);
	if (!HealthWidget) return;
	HealthWidget->AddToViewport();
	
	// 3) 위젯의 MVVM 뷰에 ViewModel 주입(Manual 생성 타입이므로 코드 주입).
	//    "OBHealthViewModel"은 WBP의 Viewmodel Name과 일치해야 한다.
	if (UMVVMView* View = HealthWidget->GetExtension<UMVVMView>())
	{
		View->SetViewModel(FName("OBHealthViewModel"), HealthViewModel);
	}
}

void AOBHUD::BindAmmoToCharacter(AOBCharacterBase* Character)
{
	if (!Character || !AmmoWidgetClass) return;

	UOBEquipmentComponent* Equipment = Character->FindComponentByClass<UOBEquipmentComponent>();
	UOBInventoryComponent* Inventory = Character->FindComponentByClass<UOBInventoryComponent>();
	if (!Equipment) return;
	
	if (!AmmoWidget)
	{
		AmmoViewModel = NewObject<UOBAmmoViewModel>(this);
		AmmoWidget = CreateWidget<UUserWidget>(GetOwningPlayerController(), AmmoWidgetClass);
		if (AmmoWidget)
		{
			AmmoWidget->AddToViewport();
			if (UMVVMView* View = AmmoWidget->GetExtension<UMVVMView>())
			{
				View->SetViewModel(FName("OBAmmoViewModel"), AmmoViewModel);
			}			
		}
	}
	
	// 무기 교체 구독 + 현재 무기로 초기화(아직 null이면 OnWeaponChanged가 채움).
	if (AmmoViewModel)
	{
		AmmoViewModel->SetInventory(Inventory);
		Equipment->OnWeaponChanged.AddUObject(this, &AOBHUD::HandleWeaponChanged);
		AmmoViewModel->SetWeapon(Equipment->GetCurrentWeapon());
	}
}

void AOBHUD::HandleWeaponChanged(AOBWeaponBase* NewWeapon)
{
	if (AmmoViewModel)
	{
		AmmoViewModel->SetWeapon(NewWeapon);
	}
}

void AOBHUD::BindConsumablesToCharacter(AOBCharacterBase* Character)
{
	if (!Character || !ConsumableWidgetClass) return;
	
	UOBInventoryComponent* Inventory = Character->FindComponentByClass<UOBInventoryComponent>();
	if (!Inventory) return;
	
	if (!ConsumableWidget)
	{
		ConsumableWidget = CreateWidget<UOBConsumableWidget>(GetOwningPlayerController(), ConsumableWidgetClass);
		if (ConsumableWidget)
			ConsumableWidget->AddToViewport();
	}
	
	if (ConsumableWidget)
		ConsumableWidget->SetInventory(Inventory); // 리스폰 시 재 바인딩
}
