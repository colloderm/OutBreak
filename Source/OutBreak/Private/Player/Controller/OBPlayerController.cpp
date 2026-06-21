// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/Controller/OBPlayerController.h"

#include "Character/OBCharacterBase.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"

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