// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/Controller/OBPlayerController.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Ability/Components/OBAbilitySystemComponent.h"
#include "Character/OBCharacterBase.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "LyraInspired/Input/OBInputConfig.h"
#include "Camera/PlayerCameraManager.h"
#include "Inventory/Components/OBInventoryComponent.h"
#include "Weapon/Data/OBWeaponData.h"

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
		EIC->BindAction(SlotPrimaryAction,   ETriggerEvent::Started, this, &AOBPlayerController::Input_EquipSlot, EOBWeaponSlot::Primary);
	}
	
	if (SlotSecondaryAction)
	{
		EIC->BindAction(SlotSecondaryAction, ETriggerEvent::Started, this, &AOBPlayerController::Input_EquipSlot, EOBWeaponSlot::Secondary);
	}
	
	if (SlotMeleeAction)
	{
		EIC->BindAction(SlotMeleeAction,     ETriggerEvent::Started, this, &AOBPlayerController::Input_EquipSlot, EOBWeaponSlot::Melee);
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
		if (UOBInventoryComponent* Inv = P->FindComponentByClass<UOBInventoryComponent>())
		{
			Inv->Server_EquipSlot(Slot);  // 클라 → 서버 요청
		}
	}
}
