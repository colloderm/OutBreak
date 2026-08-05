// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/Controller/OBPlayerController.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Ability/Components/OBAbilitySystemComponent.h"
#include "Character/OBCharacterBase.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "Blueprint/UserWidget.h"
#include "LyraInspired/Input/OBInputConfig.h"
#include "Camera/PlayerCameraManager.h"
#include "Inventory/Components/PlayerInventoryComponent.h"
#include "Inventory/Data/WorldItem.h"
#include "Inventory/Widget/InventoryWindow.h"
#include "Player/State/OBPlayerStateBase.h"
#include "Game/GameMode/OBLobbyGameMode.h"
#include "Game/GameState/OBExpeditionGameState.h"
#include "Interaction/OBInteractableActor.h"
#include "Kismet/GameplayStatics.h"
#include "LoadOut/OBLoadoutSubsystem.h"
#include "Party/OBPartySubsystem.h"
#include "Weapon/Data/OBWeaponData.h"
#include "TimerManager.h"
#include "Game/GameMode/OBExpeditionGameMode.h"
#include "Item/Loot/OBLootContainer.h"
#include "UI/Widgets/Expedition/OBExpeditionResultWidget.h"


AOBPlayerController::AOBPlayerController()
{
	// Default quick-slot keys are 4..9. The actual key mappings remain in the
	// Enhanced Input Mapping Context; this array exposes the six IA properties.
	QuickSlotActions.SetNum(6);
}

void AOBPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController()) return;

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (DefaultMappingContext)
		{
			Subsystem->AddMappingContext(DefaultMappingContext, InputMappingPriority);
		}
	}

	// 탈출 진행 게이지. 상시 존재하고 Visibility 바인딩이 알아서 숨긴다.
	if (ExtractionProgressWidgetClass)
	{
		if (UUserWidget* W = CreateWidget<UUserWidget>(this, ExtractionProgressWidgetClass))
		{
			W->AddToViewport(10);
		}
	}

	// 세션 종료(결과창) 구독. 다른 위젯 설정과 무관하게 항상 걸어야 한다.
	// 예전에는 ExtractionProgressWidgetClass 검사 안쪽에 있어서, 그게 비면 결과창이 영구히 안 떴다.
	BindToGameStatePhase();

	// GameInstance에 저장된 Loadout을 서버로 push(비seamless travel이라 PS가 새로 생성되므로 필요).
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UOBLoadoutSubsystem* Loadout = GI->GetSubsystem<UOBLoadoutSubsystem>())
		{
			const TArray<TSubclassOf<AOBWeaponBase>> Classes = Loadout->GetSelectedClasses();
			if (Classes.Num() > 0)
			{
				Server_ApplyLoadout(Classes);
				
				const TArray<FOBItemStack>& Carry = Loadout->GetCarryItems();
				if (Carry.Num() > 0)
				{
					Server_ApplyCarryItems(Carry);
				}
			}
		}

		if (UOBPartySubsystem* Party = GI->GetSubsystem<UOBPartySubsystem>())
		{
			Server_SetPartyLeader(Party->IsLocalLeader());
		}
	}

	BindToExpeditionStatus();
}

void AOBPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	BindToExpeditionStatus();
}



void AOBPlayerController::ApplyWeaponRecoil(float PitchKick, float YawKick, float RecoverySpeed, TSubclassOf<UCameraShakeBase> CameraShake, float CameraShakeScale)
{
	// 소유 클라에서만
	if (!IsLocalController()) return;
	
	CurrentRecoilRecoverySpeed = RecoverySpeed;
	
	// 수평은 좌우 랜덤
	const float YawDelta = FMath::FRandRange(-YawKick, YawKick);
	
	// 컨트롤 회전에 직접 반동 적용(위 + 좌우). 컨트롤 회전으 서버로 복제
	FRotator NewControlRotation = GetControlRotation();
	NewControlRotation.Pitch += PitchKick;
	NewControlRotation.Yaw += YawDelta;
	SetControlRotation(NewControlRotation);
	
	AccumulatedRecoilPitch += PitchKick;
	AccumulatedRecoilYaw += YawDelta;
	
	if (CameraShake)
	{
		// 스케일 적용(조준 시 약하게).
		ClientStartCameraShake(CameraShake, CameraShakeScale);
	}
}

void AOBPlayerController::UpdateRecoilRecovery(float DeltaSeconds)
{
	if (FMath::IsNearlyZero(AccumulatedRecoilPitch) && FMath::IsNearlyZero(AccumulatedRecoilYaw)) return;
	
	// 누적 반동을 0으로 보간
	const float NewPitch = FMath::FInterpTo(AccumulatedRecoilPitch, 0.f, DeltaSeconds, CurrentRecoilRecoverySpeed);
	const float NewYaw = FMath::FInterpTo(AccumulatedRecoilYaw, 0.f, DeltaSeconds, CurrentRecoilRecoverySpeed);
	
	// 이번 프레임에 되돌일 양(반동분만)
	const float DeltaPitch = AccumulatedRecoilPitch - NewPitch;
	const float DeltaYaw = AccumulatedRecoilYaw - NewYaw;
	
	// 반동분만 시야에서 차감(플레이어 수동 입력과 안 싸움)
	FRotator NewControlRotation = GetControlRotation();
	NewControlRotation.Pitch -= DeltaPitch;
	NewControlRotation.Yaw -= DeltaYaw;
	SetControlRotation(NewControlRotation);
	
	AccumulatedRecoilPitch = NewPitch;
	AccumulatedRecoilYaw = NewYaw;
}

void AOBPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	if (IsLocalController())
	{
		UpdateRecoilRecovery(DeltaSeconds);
		
		// 누적 입력을 능력 발동/통지로 처리.
		if (UOBAbilitySystemComponent* ASC = GetOBAbilitySystemComponent())
		{
			ASC->ProcessAbilityInput(DeltaSeconds, false);
		}
	}
}

void AOBPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EIC) return;

	if (MoveAction)
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AOBPlayerController::Input_Move);
	}

	if (LookAction)
	{
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &AOBPlayerController::Input_Look);
	}

	if (JumpAction)
	{
		EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &AOBPlayerController::Input_JumpStarted);
		EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &AOBPlayerController::Input_JumpCompleted);
	}

	if (InventoryAction)
	{
		EIC->BindAction(InventoryAction, ETriggerEvent::Started, this, &AOBPlayerController::Input_InventoryKey);
	}

	if (InputConfig)
	{
		for (const FOBInputAction& Action : InputConfig->AbilityInputActions)
		{
			if (Action.InputAction && Action.InputTag.IsValid())
			{
				EIC->BindAction(Action.InputAction, ETriggerEvent::Started, this, &AOBPlayerController::Input_AbilityInputPressed, Action.InputTag);
				EIC->BindAction(Action.InputAction, ETriggerEvent::Completed, this, &AOBPlayerController::Input_AbilityInputReleased, Action.InputTag);
			}
		}
	}
	
	if (SlotPrimaryAction)
	{
		EIC->BindAction(SlotPrimaryAction, ETriggerEvent::Started, this, &AOBPlayerController::Input_EquipSlot, EOBWeaponSlot::Primary);
	}
	
	if (SlotSecondaryAction)
	{
		EIC->BindAction(SlotSecondaryAction, ETriggerEvent::Started, this, &AOBPlayerController::Input_EquipSlot, EOBWeaponSlot::Secondary);
	}
	
	if (SlotMeleeAction)
	{
		EIC->BindAction(SlotMeleeAction, ETriggerEvent::Started, this, &AOBPlayerController::Input_EquipSlot, EOBWeaponSlot::Melee);
	}

	for (int32 QuickSlotIndex = 0;
		 QuickSlotIndex < QuickSlotActions.Num();
		 ++QuickSlotIndex)
	{
		if (QuickSlotActions[QuickSlotIndex])
		{
			EIC->BindAction(
				QuickSlotActions[QuickSlotIndex],
				ETriggerEvent::Started,
				this,
				&AOBPlayerController::Input_UseQuickSlot,
				QuickSlotIndex);
		}
	}
	
	if (InteractAction)
	{
		EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &AOBPlayerController::Input_Interact);
	}
	
	if (PartyToggleAction)
	{
		EIC->BindAction(PartyToggleAction, ETriggerEvent::Started, this, &AOBPlayerController::Input_TogglePartyUI);
	}
}

void AOBPlayerController::Input_Move(const FInputActionValue& Value)
{
	AOBCharacterBase* ControlledCharacter = Cast<AOBCharacterBase>(GetPawn());
	if (!ControlledCharacter) return;

	const FVector2D AxisValue = Value.Get<FVector2D>();

	const FRotator YawRotation(0.f, GetControlRotation().Yaw, 0.f);
	const FVector ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	ControlledCharacter->AddMovementInput(ForwardDir, AxisValue.Y);
	ControlledCharacter->AddMovementInput(RightDir, AxisValue.X);
}

void AOBPlayerController::Input_Look(const FInputActionValue& Value)
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return;
	}

	const FVector2D AxisValue = Value.Get<FVector2D>();

	ControlledPawn->AddControllerYawInput(AxisValue.X);
	ControlledPawn->AddControllerPitchInput(AxisValue.Y);
}

void AOBPlayerController::Input_JumpStarted()
{
	if (ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn()))
	{
		ControlledCharacter->Jump();
	}
}

void AOBPlayerController::Input_JumpCompleted()
{
	if (ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn()))
	{
		ControlledCharacter->StopJumping();
	}
}

void AOBPlayerController::Input_InventoryKey()
{
	if (!bInventoryToggle)
	{
		InventoryStarted();
	}
	else
	{
		InventoryCompleted();
	}
	bInventoryToggle = !bInventoryToggle;
}

void AOBPlayerController::InventoryStarted()
{
	AOBCharacterBase* CharacterBase = Cast<AOBCharacterBase>(GetPawn());
	if (!IsValid(CharacterBase))
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s: Character Base is not the Pawn owned by this controller."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}

	UPlayerInventoryComponent* Inventory =
		CharacterBase->GetPlayerInventoryComponent();
	if (!IsValid(Inventory))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s::%s: PlayerInventoryComponent is invalid."),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__));
		return;
	}

	UInventoryWindow* InventoryWindow = Inventory->OpenInventory();
	if (!IsValid(InventoryWindow))
	{
		return;
	}

	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(InventoryWindow->TakeWidget());
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(
		EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	SetShowMouseCursor(true);
}

void AOBPlayerController::InventoryCompleted()
{
	if (AOBCharacterBase* CharacterBase =
		Cast<AOBCharacterBase>(GetPawn()))
	{
		if (UPlayerInventoryComponent* Inventory =
			CharacterBase->GetPlayerInventoryComponent())
		{
			Inventory->CloseInventory();
		}
	}

	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);
}

void AOBPlayerController::Input_AbilityInputPressed(FGameplayTag InputTag)
{
	if (UOBAbilitySystemComponent* ASC = GetOBAbilitySystemComponent())
	{
		ASC->AbilityInputTagPressed(InputTag);
	}
}

void AOBPlayerController::Input_AbilityInputReleased(FGameplayTag InputTag)
{
	if (UOBAbilitySystemComponent* ASC = GetOBAbilitySystemComponent())
	{
		ASC->AbilityInputTagReleased(InputTag);
	}
}

UOBAbilitySystemComponent* AOBPlayerController::GetOBAbilitySystemComponent() const
{
	return Cast<UOBAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetPawn()));
}

void AOBPlayerController::Input_EquipSlot(EOBWeaponSlot Slot)
{
	if (APawn* P = GetPawn())
	{
		if (UPlayerInventoryComponent* Inv =
			P->FindComponentByClass<UPlayerInventoryComponent>())
		{
			Inv->EquipSlot(Slot);
		}
	}
}

void AOBPlayerController::Input_UseQuickSlot(const int32 QuickSlotIndex)
{
	if (APawn* ControlledPawn = GetPawn())
	{
		if (UPlayerInventoryComponent* Inventory =
			ControlledPawn->FindComponentByClass<UPlayerInventoryComponent>())
		{
			Inventory->TryUseQuickSlot(QuickSlotIndex);
		}
	}
}

void AOBPlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);
	
	if (IsLocalController())
	{
		FInputModeGameOnly Mode;
		SetInputMode(Mode);
		SetShowMouseCursor(false);
	}
}

void AOBPlayerController::Input_Interact()
{
	if (AOBInteractableActor* Target = CurrentInteractable.Get())
		Target->Interact(this);
}

void AOBPlayerController::Input_TogglePartyUI()
{
	if (!IsLocalController() || !PartyWidgetClass) return;

	if (PartyWidget && PartyWidget->IsInViewport())
	{
		PartyWidget->RemoveFromParent();
		PartyWidget = nullptr;
		
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
	}
	else
	{
		PartyWidget = CreateWidget<UUserWidget>(this, PartyWidgetClass);
		if (!PartyWidget) return;
		PartyWidget->AddToViewport();
		
		FInputModeGameAndUI Mode;
		Mode.SetWidgetToFocus(PartyWidget->TakeWidget());
		SetInputMode(Mode);
		SetShowMouseCursor(true);
	}
}

void AOBPlayerController::OBSuicide()
{
#if !UE_BUILD_SHIPPING
	Server_Suicide();   // 콘솔은 클라에서 실행 → 판정은 서버에서
#endif
}

void AOBPlayerController::Server_Suicide_Implementation()
{
	AOBPlayerStateBase* PS = GetPlayerState<AOBPlayerStateBase>();
	AOBExpeditionGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AOBExpeditionGameMode>() : nullptr;
	if (!PS || !GM) return;

	// 다운 상태 → 블리드아웃 포기.
	if (PS->GetExpeditionStatus() == EOBPlayerExpeditionStatus::Downed)
	{
		GM->FinishDownedPlayer(this);
		return;
	}

	if (PS->GetExpeditionStatus() != EOBPlayerExpeditionStatus::Alive) return;

	if (AOBCharacterBase* Char = Cast<AOBCharacterBase>(GetPawn()))
	{
		Char->HandleDeath();   // 기존 경로: 팀 생존자가 있으면 Downed가 된다
	}

	// 디버그 자살은 다운에서 멈추지 않고 사망까지 확정(관전 흐름을 바로 보기 위함).
	if (PS->GetExpeditionStatus() == EOBPlayerExpeditionStatus::Downed)
	{
		GM->FinishDownedPlayer(this);
	}
}

void AOBPlayerController::BindToExpeditionStatus()
{
	if (bExpeditionStatusBound) return;
	
	AOBPlayerStateBase* PS = GetPlayerState<AOBPlayerStateBase>();
	if (!PS) return; // 아직 PS 없음 → OnRep_PlayerState에서 재시도됨
	
	// 비다이나믹 멀티캐스트 → AddUObject 바인딩.
	PS->OnExpeditionStatusChanged.AddUObject(this, &AOBPlayerController::HandleExpeditionStatusChanged);
	bExpeditionStatusBound = true;
	
	HandleExpeditionStatusChanged(); // 바인딩 시점 현재 상태 1회 동기화
}

void AOBPlayerController::HandleExpeditionStatusChanged()
{
	if (!IsLocalController()) return; // 화면 연출은 소유 클라에서만
	
	AOBPlayerStateBase* PS = GetPlayerState<AOBPlayerStateBase>();
	if (!PS) return;

	switch (PS->GetExpeditionStatus())
	{
	case EOBPlayerExpeditionStatus::Dead:
		break;
	case EOBPlayerExpeditionStatus::Extracted:
		ShowExtractScreen();   // 사망화면과 동일 패턴, 위젯만 다름
		break;
	default:
		HideDeathScreen(); // 현재는 리스폰 없음(Alive 복귀 시 대비)
		break;
	}
}

void AOBPlayerController::ShowDeathScreen()
{
	if (ActiveDeathWidget || ActiveResultWidget) return;   // 결과창이 이미 떴으면 되돌아가지 않는다
	
	DisableInput(this); // 로컬 이동/사격 에측 차단(서버는 이미 차단)
	SetShowMouseCursor(true);
	SetInputMode(FInputModeUIOnly());
	
	if (DeathScreenWidgetClass)
	{
		ActiveDeathWidget = CreateWidget<UUserWidget>(this, DeathScreenWidgetClass);
		if (ActiveDeathWidget)
		{
			ActiveDeathWidget->AddToViewport(50); // HUD 위에
		}
	}
}

void AOBPlayerController::ShowExtractScreen()
{
	if (ActiveDeathWidget || ActiveResultWidget) return;
	
	DisableInput(this);
	
	if (ExtractScreenWidgetClass)
	{
		ActiveDeathWidget = CreateWidget<UUserWidget>(this, ExtractScreenWidgetClass);
		if (ActiveDeathWidget)
		{
			ActiveDeathWidget->AddToViewport(50);
		}
	}
}

void AOBPlayerController::HideDeathScreen()
{
	if (ActiveDeathWidget)
	{
		ActiveDeathWidget->RemoveFromParent();
		ActiveDeathWidget = nullptr;
	}
	
	EnableInput(this);
}

void AOBPlayerController::ClientBeginSpectate_Implementation()
{
	ShowSpectatorHUD();
}

void AOBPlayerController::ClientTeamWiped_Implementation()
{
	HideSpectatorHUD();
	ShowDeathScreen();   // 여기서만 홈 복귀 버튼이 있는 화면이 뜬다
}

void AOBPlayerController::ShowSpectatorHUD()
{
	if (ActiveSpectatorWidget) return; // 중복 방지
	
	// 죽었으니 게임플레이 입력은 잠그고 HUD 버튼만 받는다.
	DisableInput(this);
	SetShowMouseCursor(true);
	SetInputMode(FInputModeGameAndUI());
	
	if (SpectatorHUDWidgetClass)
	{
		ActiveSpectatorWidget = CreateWidget<UUserWidget>(this, SpectatorHUDWidgetClass);
		if (ActiveSpectatorWidget)
		{
			ActiveSpectatorWidget->AddToViewport(50);
		}
	}
}

void AOBPlayerController::HideSpectatorHUD()
{
	if (ActiveSpectatorWidget)
	{
		ActiveSpectatorWidget->RemoveFromParent();
		ActiveSpectatorWidget = nullptr;
	}
}

void AOBPlayerController::ServerCycleSpectateTarget_Implementation(int32 Direction)
{
	AOBPlayerStateBase* MyPS = GetPlayerState<AOBPlayerStateBase>();
	AOBExpeditionGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AOBExpeditionGameMode>() : nullptr;
	if (!MyPS || !GM) return;

	// 살아있는 플레이어의 시점 조작 요청은 무시(클라 RPC = 신뢰 경계).
	if (MyPS->GetExpeditionStatus() != EOBPlayerExpeditionStatus::Dead) return;

	const TArray<AOBPlayerStateBase*> Living = GM->GetLivingTeammates(MyPS->GetTeamId());
	if (Living.IsEmpty()) return;

	// 조작된 Direction으로 음수 모듈러가 나오면 배열 범위를 벗어난다. 반드시 클램프.
	const int32 Step = FMath::Clamp(Direction, -1, 1);

	const AActor* Current = GetViewTarget();
	int32 Index = Living.IndexOfByPredicate(
		[Current](const AOBPlayerStateBase* P)
		{
			return P->GetPawn() == Current;
		});

	Index = (Index == INDEX_NONE) ? 0 : (Index + Step + Living.Num()) % Living.Num();

	if (APawn* NextPawn = Living[Index]->GetPawn())
	{
		SetSpectateViewTarget(NextPawn);
	}
}

void AOBPlayerController::SetSpectateViewTarget(AActor* NewTarget)
{
	if (!NewTarget || !HasAuthority()) return;

	SetViewTarget(NewTarget);         // 서버: 네트워크 렐러번시가 관전 대상을 따라오게 한다
	ClientSetViewTarget(NewTarget);   // 클라: 실제 카메라 전환(위 호출은 복제 안 됨)
}

FString AOBPlayerController::GetSpectateTargetName() const
{
	const APawn* P = Cast<APawn>(GetViewTarget());
	const APlayerState* PS = P ? P->GetPlayerState() : nullptr;
	return PS ? PS->GetPlayerName() : FString();
}

void AOBPlayerController::BindToGameStatePhase()
{
	if (bPhaseBound) return;
	
	AOBExpeditionGameState* GS = GetWorld() ? GetWorld()->GetGameState<AOBExpeditionGameState>() : nullptr;
	if (!GS)
	{
		if (++PhaseBindAttempts > MaxPhaseBindAttempts)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Expedition] OBExpeditionGameState를 %.0f초간 못 찾아 결과창 구독을 포기한다. "
					 "(홈/로비 맵이면 정상. 원정 맵이면 GameMode의 GameState Class 확인)"),
				MaxPhaseBindAttempts * 0.25f);
			return;
		}

		// GameState 아직 복제 전 -> 잠시 후 재시도
		GetWorldTimerManager().SetTimer(PhaseBindRetryTimer, this, &AOBPlayerController::BindToGameStatePhase, 0.25f, false);
		return;
	}
	
	GS->OnPhaseChanged.AddDynamic(this, &AOBPlayerController::HandleExpeditionPhaseChanged);
	bPhaseBound = true;
	
	// 늦게 붙어 이미 Ended면 즉시 반영
	if (GS->GetPhase() == EOBExpeditionPhase::Ended)
	{
		HandleExpeditionPhaseChanged(EOBExpeditionPhase::Ended);
	}
}

void AOBPlayerController::HandleExpeditionPhaseChanged(EOBExpeditionPhase NewPhase)
{
	if (!IsLocalController()) return;
	if (NewPhase != EOBExpeditionPhase::Ended) return;

	// 종료 판정은 사망/탈출과 같은 프레임에 온다. 바로 띄우면 사망화면이 1프레임 만에 지워진다.
	if (ResultDelaySeconds > 0.f)
	{
		GetWorldTimerManager().SetTimer(
			ResultDelayTimer, this, &AOBPlayerController::ShowResultScreen, ResultDelaySeconds, false);
	}
	else
	{
		ShowResultScreen();
	}
}

void AOBPlayerController::ShowResultScreen()
{
	if (ActiveResultWidget) return;
	
	HideDeathScreen(); // 사망/탈출 화면 있으면 결과창으로 대체
	
	// 입력 잠금 + UI 모드 + 커서
	DisableInput(this);
	SetShowMouseCursor(true);
	SetInputMode(FInputModeUIOnly());
	
	if (ResultWidgetClass)
	{
		ActiveResultWidget = CreateWidget<UUserWidget>(this, ResultWidgetClass);
		if (ActiveResultWidget)
		{
			ActiveResultWidget->AddToViewport(100); // 최상단

			// 사망 시에는 LastExtractionHaul이 비어 있어서 "가져온 것 없음"이 뜬다.
			if (UOBExpeditionResultWidget* Result = Cast<UOBExpeditionResultWidget>(ActiveResultWidget))
			{
				Result->SetHaul(LastExtractionHaul);
			}
		}
	}
	
	// 자동 복귀(버튼 안 눌러도)
	if (AutoReturnSeconds > 0.f)
	{
		GetWorldTimerManager().SetTimer(AutoReturnTimer, this, &AOBPlayerController::ReturnToHome, AutoReturnSeconds, false);
	}
}

void AOBPlayerController::Client_ApplyExtractionResult_Implementation(const TArray<FOBItemStack>& Haul)
{
	LastExtractionHaul = Haul;

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UOBLoadoutSubsystem* Loadout = GI->GetSubsystem<UOBLoadoutSubsystem>())
		{
			Loadout->AddStashItems(Haul);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Expedition] 탈출 정산: %d종 창고 반영"), Haul.Num());
}

void AOBPlayerController::ReturnToHome()
{
	GetWorldTimerManager().ClearTimer(AutoReturnTimer);
	
	if (HomeLevel.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Expedition] HomeLevel 미지정 → 복귀 불가"));
		return;
	}
	
	// 위젯 버튼 콜백 안에서 바로 레벨을 열면 월드/위젯 정리가 콜백 스택 위에서 일어나
	// 호출자(WBP_ExpeditionResult)가 파괴된 채로 반환된다. 다음 틱으로 미뤄 안전하게 나간다.
	GetWorldTimerManager().SetTimerForNextTick(this, &AOBPlayerController::TravelToHome);
}

void AOBPlayerController::TravelToHome()
{
	if (HomeLevel.IsNull()) return;

	// 레벨을 열기 전에 UI를 스스로 정리한다. 월드 정리에 맡기면
	// 참조가 남았을 때 엉뚱한 곳에서 World Leak으로 터진다.
	if (ActiveResultWidget)
	{
		ActiveResultWidget->RemoveFromParent();
		ActiveResultWidget = nullptr;
	}
	CloseInteractionWidget();   // 루팅 창 등이 열린 채 나가는 경우

	UE_LOG(LogTemp, Log, TEXT("[Expedition] 홈 복귀: %s"), *HomeLevel.ToString());

	// 데디 접속 종료 후 각 클라가 로컬 Home 로드(개별 복귀).
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, HomeLevel);
}

int32 AOBPlayerController::GetReturnCountdown() const
{
	// 자동복귀 타이머의 남은 시간(비활성이면 -1) → 0 이상 올림값.
	const float Remaining = GetWorldTimerManager().GetTimerRemaining(AutoReturnTimer);
	return Remaining > 0.f ? FMath::CeilToInt(Remaining) : 0;
}

void AOBPlayerController::SetCurrentInteractable(AOBInteractableActor* Interactable)
{
	AOBInteractableActor* Previous = CurrentInteractable.Get();

	if (Previous != Interactable)
	{
		// 프롬프트와 하이라이트는 항상 한 개만 켜져 있어야 한다.
		if (Previous)     
			Previous->SetHighlighted(false);
		if (Interactable) 
			Interactable->SetHighlighted(true);

		CurrentInteractable = Interactable;
	}
	else if (Interactable)
	{
		// 대상이 그대로여도 문구는 바뀔 수 있다(다 털린 상자 → "비어 있음").
		// RefreshInteractTarget이 0.15초마다 여기로 온다.
		Interactable->RefreshPromptText();
	}
}

AOBInteractableActor* AOBPlayerController::GetCurrentInteractable() const
{
	return CurrentInteractable.Get();
}

UUserWidget* AOBPlayerController::OpenInteractionWidget(TSubclassOf<UUserWidget> WidgetClass)
{
	if (!IsLocalController() || !WidgetClass || ActiveInteractionWidget) return nullptr;

	ActiveInteractionWidget = CreateWidget<UUserWidget>(this, WidgetClass);
	if (!ActiveInteractionWidget) return nullptr;
	
	ActiveInteractionWidget->SetIsFocusable(true);
	ActiveInteractionWidget->AddToViewport();

	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(ActiveInteractionWidget->TakeWidget());
	SetInputMode(Mode);
	SetShowMouseCursor(true);

	// 이동/시점 잠금(UI 연 플레이어만 영향).
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
	
	return ActiveInteractionWidget;   // NPC가 바인딩·초기화용으로 사용
}

void AOBPlayerController::CloseInteractionWidget()
{
	if (ActiveInteractionWidget)
	{
		ActiveInteractionWidget->RemoveFromParent();
		ActiveInteractionWidget = nullptr;
	}

	FInputModeGameOnly Mode;
	SetInputMode(Mode);
	SetShowMouseCursor(false);

	// 잠금 해제(Open의 true와 1:1 균형).
	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);
}

void AOBPlayerController::Server_SetPartyLeader_Implementation(bool bLeader)
{
	if (AOBPlayerStateBase* PS = GetPlayerState<AOBPlayerStateBase>())
		PS->SetPartyLeader(bLeader);
}

void AOBPlayerController::Server_ApplyLoadout_Implementation(const TArray<TSubclassOf<AOBWeaponBase>>& Weapons)
{
	if (AOBPlayerStateBase* PS = GetPlayerState<AOBPlayerStateBase>())
		PS->SetSelectedWeaponsBulk(Weapons);
}

void AOBPlayerController::Server_SetWeaponSlot_Implementation(EOBWeaponSlot Slot, TSubclassOf<AOBWeaponBase> WeaponClass)
{
	if (AOBPlayerStateBase* PS = GetPlayerState<AOBPlayerStateBase>())
		PS->SetWeaponForSlot(Slot, WeaponClass);
}

void AOBPlayerController::Server_SetReady_Implementation(bool bReady)
{
	if (AOBPlayerStateBase* PS = GetPlayerState<AOBPlayerStateBase>())
		PS->SetReady(bReady);
}

void AOBPlayerController::Server_StartGame_Implementation()
{
	if (AOBLobbyGameMode* GM = GetWorld()->GetAuthGameMode<AOBLobbyGameMode>())
		GM->TryStartGame(this);
}

void AOBPlayerController::AddNearbyInteractable(AOBInteractableActor* Interactable)
{
	if (!Interactable) return;

	NearbyInteractables.AddUnique(Interactable);

	// 매 프레임 거리를 재는 건 낭비다. 걸어서 대상이 바뀌는 속도면 0.15초로 충분하다.
	if (!GetWorldTimerManager().IsTimerActive(InteractRefreshTimer))
	{
		GetWorldTimerManager().SetTimer(
			InteractRefreshTimer, this, &AOBPlayerController::RefreshInteractTarget, 0.15f, true);
	}

	RefreshInteractTarget();   // 들어서자마자 프롬프트가 떠야 한다.
}

void AOBPlayerController::RemoveNearbyInteractable(AOBInteractableActor* Interactable)
{
	NearbyInteractables.RemoveAll(
		[Interactable](const TWeakObjectPtr<AOBInteractableActor>& Entry)
		{
			return !Entry.IsValid() || Entry.Get() == Interactable;
		});

	if (NearbyInteractables.IsEmpty())
	{
		GetWorldTimerManager().ClearTimer(InteractRefreshTimer);
		SetCurrentInteractable(nullptr);
		return;
	}

	RefreshInteractTarget();
}

void AOBPlayerController::RefreshInteractTarget()
{
	const APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		SetCurrentInteractable(nullptr);
		return;
	}

	// 3인칭 카메라는 벽을 뚫고 뒤로 빠진다. 캐릭터 눈높이가 기준이어야 한다.
	const FVector ViewLocation = MyPawn->GetPawnViewLocation();

	AOBInteractableActor* Nearest = nullptr;
	float NearestDistSq = TNumericLimits<float>::Max();

	for (int32 i = NearbyInteractables.Num() - 1; i >= 0; --i)
	{
		AOBInteractableActor* Candidate = NearbyInteractables[i].Get();
		if (!Candidate)
		{
			// 다 털려서 사라진 시체 등. EndOverlap이 안 오므로 여기서 청소한다.
			NearbyInteractables.RemoveAtSwap(i);
			continue;
		}

		// 거리부터 본다. 가려짐 판정(트레이스)이 더 비싸다.
		const float DistSq = FVector::DistSquared(ViewLocation, Candidate->GetActorLocation());
		if (DistSq >= NearestDistSq)
		{
			continue;
		}

		if (IsInteractableOccluded(ViewLocation, Candidate))
		{
			continue;
		}

		NearestDistSq = DistSq;
		Nearest = Candidate;
	}

	SetCurrentInteractable(Nearest);
}

bool AOBPlayerController::IsInteractableOccluded(const FVector& ViewLocation, const AOBInteractableActor* Candidate) const
{
	const UWorld* World = GetWorld();
	if (!World || !Candidate) return true;

	// 충돌을 끈 시체 상자도 포함해야 한다(기본값은 충돌 켜진 컴포넌트만 센다).
	const FBox Bounds = Candidate->GetComponentsBoundingBox(true);
	const FVector Target = Bounds.IsValid ? Bounds.GetCenter() : Candidate->GetActorLocation();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(OBInteractOcclusion), false, GetPawn());
	Params.AddIgnoredActor(Candidate);   // 상자 자기 자신에 막히면 영원히 못 본다.

	FHitResult Hit;
	return World->LineTraceSingleByChannel(Hit, ViewLocation, Target, ECC_Visibility, Params);
}

namespace
{
	// 상호작용 반경(기본 200)보다 넉넉히 잡는다. 지연 때문에 서버 위치가 조금 다르다.
	constexpr float OBMaxInteractionDistanceSq = 500.f * 500.f;
}

void AOBPlayerController::Server_TakeLoot_Implementation(AOBLootContainer* Container, FGameplayTag ItemTag, int32 Count)
{
	if (!Container || Count <= 0) return;

	AOBCharacterBase* MyChar = Cast<AOBCharacterBase>(GetPawn());
	if (!MyChar || MyChar->IsDead()) return;

	// 클라가 보낸 걸 믿지 않는다. 실제로 그 앞에 서 있는지 서버가 확인한다.
	if (FVector::DistSquared(MyChar->GetActorLocation(), Container->GetActorLocation()) > OBMaxInteractionDistanceSq)
	{
		return;
	}

	if (UPlayerInventoryComponent* Inv = MyChar->GetPlayerInventoryComponent())
	{
		Container->TryTakeItem(Inv, ItemTag, Count);
	}
}

void AOBPlayerController::Server_TakeAllLoot_Implementation(AOBLootContainer* Container)
{
	if (!Container) return;

	AOBCharacterBase* MyChar = Cast<AOBCharacterBase>(GetPawn());
	if (!MyChar || MyChar->IsDead()) return;

	if (FVector::DistSquared(MyChar->GetActorLocation(), Container->GetActorLocation()) > OBMaxInteractionDistanceSq)
	{
		return;
	}

	if (UPlayerInventoryComponent* Inv = MyChar->GetPlayerInventoryComponent())
	{
		Container->TryTakeAll(Inv);
	}
}

void AOBPlayerController::Server_PickUpWorldItem_Implementation(
	AWorldItem* WorldItem)
{
	if (!IsValid(WorldItem) || !WorldItem->HasItemInstance())
	{
		return;
	}

	AOBCharacterBase* MyChar = Cast<AOBCharacterBase>(GetPawn());
	if (!MyChar || MyChar->IsDead() ||
		FVector::DistSquared(
			MyChar->GetActorLocation(),
			WorldItem->GetActorLocation()) > OBMaxInteractionDistanceSq)
	{
		return;
	}

	if (UPlayerInventoryComponent* Inventory =
		MyChar->GetPlayerInventoryComponent())
	{
		Inventory->PickUpWorldItem(WorldItem);
	}
}

void AOBPlayerController::Server_ApplyCarryItems_Implementation(const TArray<FOBItemStack>& Items)
{
	if (AOBPlayerStateBase* PS = GetPlayerState<AOBPlayerStateBase>())
	{
		PS->SetCarryItemsBulk(Items);
	}
}

void AOBPlayerController::Client_ReturnCarryLeftover_Implementation(const TArray<FOBItemStack>& Items)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UOBLoadoutSubsystem* Loadout = GI->GetSubsystem<UOBLoadoutSubsystem>())
		{
			Loadout->AddStashItems(Items);
			UE_LOG(LogTemp, Warning,
				TEXT("[Carry] 가방이 모자라 %d종을 창고로 되돌렸다."), Items.Num());
		}
	}
}
