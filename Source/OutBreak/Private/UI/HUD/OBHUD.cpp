// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/HUD/OBHUD.h"
#include "Game/Expedition/OBHelicopterSpawnLog.h"

#include "UI/ViewModels/OBHealthViewModel.h"
#include "Character/OBCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "UI/ViewModels/OBAmmoViewModel.h"
#include "Equipment/Components/OBEquipmentComponent.h"
#include "Inventory/Components/PlayerInventoryComponent.h"
#include "UI/HUD/OBConsumableWidget.h"
#include "UI/Widgets/Expedition/OBWorldMapWidget.h"
#include "Player/Controller/OBPlayerController.h"
#include "View/MVVMView.h"

DEFINE_LOG_CATEGORY_STATIC(LogOBInsertionUI, Log, All);

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
	
	// 세션 타이머(12시 방향). 상시 존재, Visibility 바인딩이 InProgress 때만 표시.
	if (SessionTimerWidgetClass)
	{
		SessionTimerWidget = CreateWidget<UUserWidget>(PC, SessionTimerWidgetClass);
		if (SessionTimerWidget) SessionTimerWidget->AddToViewport();
	}
	
	// 크로스헤어(화면 중앙). 상시 존재, 표시 여부는 위젯 바인딩이 판단.
	if (CrosshairWidgetClass)
	{
		CrosshairWidget = CreateWidget<UUserWidget>(PC, CrosshairWidgetClass);
		if (CrosshairWidget) CrosshairWidget->AddToViewport();
	}
	
	// 전체 지도. 상시 생성해 두고 숨긴다(M키가 토글).
	EnsureWorldMapWidget();

	// 이 시점에 이미 헬기 트랜짓이 걸려 있으면(상태가 먼저 도착한 경우) 처음부터 감춘다.
	// 늦게 도착하면 PC가 잠금 적용 시 다시 호출하므로 결과는 같다.
	if (const AOBPlayerController* OBPC = Cast<AOBPlayerController>(PC))
	{
		bGameplayHUDVisible = !OBPC->IsHelicopterTransitLocked();
	}
	ApplyGameplayHUDVisibility();
}

void AOBHUD::SetGameplayHUDVisible(const bool bVisible)
{
	bGameplayHUDVisible = bVisible;
	ApplyGameplayHUDVisibility();
}

void AOBHUD::ApplyGameplayHUDVisibility()
{
	// RemoveFromParent()를 쓰면 안 된다. UUserWidget::NativeDestruct가 불려서
	// 위젯이 걸어 둔 구독이 끊긴다 — OBConsumableWidget은 거기서 OnInventoryChanged를
	// 해제하고, 다시 넣어도 NativeConstruct가 재구독하지 않아 수량이 0으로 굳는다.
	// 파괴/재생성이 없는 SetVisibility가 맞다. 깜빡임도 그래서 생겼다.
	const ESlateVisibility Target = bGameplayHUDVisible
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed;

	UUserWidget* const Widgets[] =
	{
		HealthWidget.Get(),
		AmmoWidget.Get(),
		ConsumableWidget.Get(),
		SessionTimerWidget.Get(),
		CrosshairWidget.Get()
	};

	for (UUserWidget* Widget : Widgets)
	{
		if (IsValid(Widget))
		{
			Widget->SetVisibility(Target);
		}
	}
}

void AOBHUD::HandlePawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	if (AOBCharacterBase* Character = Cast<AOBCharacterBase>(NewPawn))
	{
		TryInitHealthWidget(Character);
		BindAmmoToCharacter(Character);
		BindConsumablesToCharacter(Character);

		// 위젯들은 지연 생성된다. 삽입 중에 폰을 잡으면 방금 만든 것들이
		// 뷰포트에 그대로 남으므로 현재 상태를 다시 적용한다.
		ApplyGameplayHUDVisibility();
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

	// ASC가 늦게 준비되면 이 함수가 HandlePawnChanged 밖에서 불린다.
	ApplyGameplayHUDVisibility();
}

void AOBHUD::BindAmmoToCharacter(AOBCharacterBase* Character)
{
	if (!Character || !AmmoWidgetClass) return;

	UOBEquipmentComponent* Equipment = Character->FindComponentByClass<UOBEquipmentComponent>();
	UPlayerInventoryComponent* Inventory = Character->FindComponentByClass<UPlayerInventoryComponent>();
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
		//Equipment->OnWeaponChanged.AddUObject(this, &AOBHUD::HandleWeaponChanged);
		// 폰 변경마다 재호출되므로 중복 구독을 막는다.
		Equipment->OnWeaponChanged.RemoveAll(this);
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
	
	UPlayerInventoryComponent* Inventory = Character->GetPlayerInventoryComponent();
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

void AOBHUD::ToggleWorldMap()
{
	if (UOBWorldMapWidget* MapWidget = EnsureWorldMapWidget())
	{
		const bool bOpen = !MapWidget->IsMapOpen();
		OB_HELICOPTER_SPAWN_LOG(LogOBInsertionUI, Log,
			TEXT("[InsertionUI] ToggleWorldMap HUD=%s Widget=%s Open=%s"),
			*GetName(), *MapWidget->GetName(), bOpen ? TEXT("true") : TEXT("false"));
		MapWidget->SetMapOpen(bOpen);
	}
	else
	{
		OB_HELICOPTER_SPAWN_LOG(LogOBInsertionUI, Error,
			TEXT("[InsertionUI] ToggleWorldMap failed HUD=%s Owner=%s WidgetClass=%s"),
			*GetName(), *GetNameSafe(GetOwningPlayerController()), *GetNameSafe(WorldMapWidgetClass));
	}
}

UOBWorldMapWidget* AOBHUD::EnsureWorldMapWidget()
{
	if (IsValid(WorldMapWidget))
	{
		return WorldMapWidget;
	}

	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !WorldMapWidgetClass)
	{
		OB_HELICOPTER_SPAWN_LOG(LogOBInsertionUI, Error,
			TEXT("[InsertionUI] WorldMap ensure failed HUD=%s Owner=%s WidgetClass=%s"),
			*GetName(), *GetNameSafe(PC), *GetNameSafe(WorldMapWidgetClass));
		return nullptr;
	}

	WorldMapWidget = CreateWidget<UOBWorldMapWidget>(PC, WorldMapWidgetClass);
	if (!WorldMapWidget)
	{
		OB_HELICOPTER_SPAWN_LOG(LogOBInsertionUI, Error,
			TEXT("[InsertionUI] WorldMap CreateWidget failed HUD=%s Owner=%s WidgetClass=%s"),
			*GetName(), *GetNameSafe(PC), *GetNameSafe(WorldMapWidgetClass));
		return nullptr;
	}

	WorldMapWidget->AddToViewport(50);
	OB_HELICOPTER_SPAWN_LOG(LogOBInsertionUI, Log,
		TEXT("[InsertionUI] WorldMap ensured HUD=%s Widget=%s WidgetClass=%s"),
		*GetName(), *WorldMapWidget->GetName(), *GetNameSafe(WorldMapWidgetClass));
	return WorldMapWidget;
}

bool AOBHUD::OpenInsertionMap(bool bCanSelectTarget)
{
	UOBWorldMapWidget* MapWidget = EnsureWorldMapWidget();
	if (!MapWidget)
	{
		return false;
	}

	MapWidget->SetInsertionSelectionMode(true, bCanSelectTarget);
	MapWidget->SetMapOpen(true);
	OB_HELICOPTER_SPAWN_LOG(LogOBInsertionUI, Log,
		TEXT("[InsertionUI] Insertion map opened HUD=%s CanSelect=%s"),
		*GetName(), bCanSelectTarget ? TEXT("true") : TEXT("false"));
	return true;
}

void AOBHUD::CloseInsertionMap()
{
	if (UOBWorldMapWidget* MapWidget = EnsureWorldMapWidget())
	{
		const bool bWasOpen = MapWidget->IsMapOpen();
		MapWidget->SetMapOpen(false);
		MapWidget->SetInsertionSelectionMode(false, false);
		OB_HELICOPTER_SPAWN_LOG(LogOBInsertionUI, Log,
			TEXT("[InsertionUI] Insertion map closed HUD=%s WasOpen=%s"),
			*GetName(), bWasOpen ? TEXT("true") : TEXT("false"));
	}
}
