// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/Controller/OBPlayerController.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Ability/Components/OBAbilitySystemComponent.h"
#include "Ability/Tags/OBGameplayTags.h"
#include "Character/OBCharacterBase.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "InputAction.h"
#include "InputMappingContext.h"
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
#include "UI/HUD/OBHUD.h"
#include "UI/Widgets/Expedition/OBExpeditionResultWidget.h"
#include "UI/Widgets/Expedition/OBWorldMapWidget.h"
#include "Game/Expedition/OBInsertionHelicopter.h"
#include "Game/Expedition/OBHelicopterTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameplayCameraComponentBase.h"
#include "DrawDebugHelpers.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogOBInsertionInput, Log, All);

AOBPlayerController::AOBPlayerController()
{
	// Default quick-slot keys are 4..9. The actual key mappings remain in the
	// Enhanced Input Mapping Context; this array exposes the six IA properties.
	QuickSlotActions.SetNum(6);
	PawnMappingContextsToSuspendDuringInsertion.Add(
		TSoftObjectPtr<UInputMappingContext>(
			FSoftObjectPath(TEXT("/Game/Input/IMC_Sandbox.IMC_Sandbox"))));
}

void AOBPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(
		AOBPlayerController,
		ReplicatedInsertionTransitState,
		COND_OwnerOnly);
}

void AOBPlayerController::ShowInsertionDebugMessage(
	const FString& Message,
	const FColor& Color,
	float Duration,
	int32 MessageKey) const
{
	if (!bShowInsertionDebugMessages || !IsLocalController() || !GEngine)
	{
		return;
	}

	const float EffectiveDuration = Duration > 0.f ? Duration : InsertionDebugMessageDuration;
	const uint64 ScreenKey = MessageKey < 0
		? static_cast<uint64>(-1)
		: static_cast<uint64>(MessageKey);
	GEngine->AddOnScreenDebugMessage(
		ScreenKey,
		EffectiveDuration,
		Color,
		FString::Printf(TEXT("[INSERTION] %s"), *Message));
}

bool AOBPlayerController::HasActiveInsertionTransit() const
{
	return IsPlayerInsertionTransitActive(ReplicatedInsertionTransitState.Phase);
}

void AOBPlayerController::ApplyInsertionAbilityGate(const bool bLocked, const bool bCancelActiveAbilities)
{
	if (UOBAbilitySystemComponent* ASC = GetOBAbilitySystemComponent())
	{
		ASC->SetLooseGameplayTagCount(OBGameplayTags::State_HelicopterTransit, bLocked ? 1 : 0);
		if (bLocked && bCancelActiveAbilities)
		{
			ASC->FlushPlayerAbilityInput(TEXT("HelicopterTransit"));
		}
		else
		{
			ASC->ClearAbilityInput();
		}
	}
}

void AOBPlayerController::SetInsertionTransitState(
	AOBInsertionHelicopter* Helicopter,
	const EOBPlayerInsertionTransitPhase Phase,
	AActor* NewViewTarget)
{
	if (!HasAuthority())
	{
		return;
	}

	const EOBPlayerInsertionTransitPhase PreviousPhase = ReplicatedInsertionTransitState.Phase;
	const bool bWasLocked = IsPlayerInsertionTransitActive(PreviousPhase);
	const bool bLocked = IsPlayerInsertionTransitActive(Phase);
	ReplicatedInsertionTransitState.Revision =
		ReplicatedInsertionTransitState.Revision == MAX_int32
			? 1
			: ReplicatedInsertionTransitState.Revision + 1;
	ReplicatedInsertionTransitState.Phase = Phase;
	ReplicatedInsertionTransitState.Helicopter = Helicopter;
	ReplicatedInsertionTransitState.ViewTarget = NewViewTarget
		? NewViewTarget
		: (bLocked ? static_cast<AActor*>(Helicopter) : static_cast<AActor*>(GetPawn()));
	const AOBPlayerStateBase* OBPlayerState = GetPlayerState<AOBPlayerStateBase>();
	ReplicatedInsertionTransitState.bCanSelectTarget =
		bLocked && OBPlayerState && OBPlayerState->IsPartyLeader();

	if (IsLocalController())
	{
		ApplyInsertionTransitStateLocal(TEXT("Authority"));
	}
	else
	{
		ApplyInsertionAbilityGate(bLocked, bLocked && !bWasLocked);
		bHelicopterTransitLocked = bLocked;
	}
	ForceNetUpdate();

	UE_LOG(LogOBInsertionInput, Log,
		TEXT("[InsertionState] Server commit PC=%s Revision=%d Previous=%d Phase=%d Pawn=%s Helicopter=%s ViewTarget=%s Locked=%s"),
		*GetName(), ReplicatedInsertionTransitState.Revision,
		static_cast<int32>(PreviousPhase), static_cast<int32>(Phase),
		*GetNameSafe(GetPawn()), *GetNameSafe(Helicopter),
		*GetNameSafe(ReplicatedInsertionTransitState.ViewTarget),
		bLocked ? TEXT("true") : TEXT("false"));
}

void AOBPlayerController::OnRep_InsertionTransitState()
{
	ApplyInsertionTransitStateLocal(TEXT("OwnerReplication"));
}

void AOBPlayerController::RefreshInsertionTransitSelectionPermission()
{
	if (!HasAuthority() || !HasActiveInsertionTransit())
	{
		return;
	}

	SetInsertionTransitState(
		ReplicatedInsertionTransitState.Helicopter,
		ReplicatedInsertionTransitState.Phase,
		ReplicatedInsertionTransitState.ViewTarget);
}

void AOBPlayerController::SetPawnInputSuppressedForInsertion(const bool bSuppress)
{
	if (!IsLocalController())
	{
		return;
	}
	SetPawnMappingContextsSuppressedForInsertion(bSuppress);

	if (bSuppress)
	{
		APawn* ControlledPawn = GetPawn();
		if (bInsertionDisabledPawnInput
			&& InsertionInputSuppressedPawn.IsValid()
			&& InsertionInputSuppressedPawn.Get() != ControlledPawn)
		{
			InsertionInputSuppressedPawn->EnableInput(
				InsertionInputSuppressedPawn->GetController() == this ? this : nullptr);
			bInsertionDisabledPawnInput = false;
			InsertionInputSuppressedPawn.Reset();
		}
		if (ControlledPawn && ControlledPawn->InputEnabled())
		{
			ControlledPawn->DisableInput(this);
			InsertionInputSuppressedPawn = ControlledPawn;
			bInsertionDisabledPawnInput = true;
			UE_LOG(LogOBInsertionInput, Log,
				TEXT("[InsertionInput] Pawn Blueprint input suppressed PC=%s Pawn=%s"),
				*GetName(), *ControlledPawn->GetName());
		}
		return;
	}

	if (bInsertionDisabledPawnInput)
	{
		APawn* SuppressedPawn = InsertionInputSuppressedPawn.Get();
		if (IsValid(SuppressedPawn))
		{
			SuppressedPawn->EnableInput(
				SuppressedPawn->GetController() == this ? this : nullptr);
			UE_LOG(LogOBInsertionInput, Log,
				TEXT("[InsertionInput] Pawn Blueprint input restored PC=%s Pawn=%s"),
				*GetName(), *SuppressedPawn->GetName());
		}
	}
	bInsertionDisabledPawnInput = false;
	InsertionInputSuppressedPawn.Reset();
}

void AOBPlayerController::SetPawnMappingContextsSuppressedForInsertion(const bool bSuppress)
{
	if (!IsLocalController())
	{
		return;
	}
	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!Subsystem)
	{
		return;
	}

	if (bSuppress)
	{
		for (TSoftObjectPtr<UInputMappingContext>& ContextReference
			: PawnMappingContextsToSuspendDuringInsertion)
		{
			UInputMappingContext* Context = ContextReference.Get();
			if (!Context)
			{
				Context = ContextReference.LoadSynchronous();
			}
			if (!Context || Context == DefaultMappingContext || Context == InsertionMappingContext)
			{
				continue;
			}

			int32 AppliedPriority = 0;
			if (Subsystem->HasMappingContext(Context, AppliedPriority))
			{
				InsertionSuspendedMappingContextPriorities.FindOrAdd(Context) = AppliedPriority;
				Subsystem->RemoveMappingContext(Context);
				UE_LOG(LogOBInsertionInput, Log,
					TEXT("[InsertionInput] Pawn mapping context suspended PC=%s Context=%s Priority=%d"),
					*GetName(), *Context->GetName(), AppliedPriority);
			}
		}
		return;
	}

	for (const TPair<const UInputMappingContext*, int32>& Suspended
		: InsertionSuspendedMappingContextPriorities)
	{
		if (IsValid(Suspended.Key) && !Subsystem->HasMappingContext(Suspended.Key))
		{
			Subsystem->AddMappingContext(Suspended.Key, Suspended.Value);
			UE_LOG(LogOBInsertionInput, Log,
				TEXT("[InsertionInput] Pawn mapping context restored PC=%s Context=%s Priority=%d"),
				*GetName(), *Suspended.Key->GetName(), Suspended.Value);
		}
	}
	InsertionSuspendedMappingContextPriorities.Reset();
}

void AOBPlayerController::ApplyInsertionTransitStateLocal(const TCHAR* Source)
{
	if (!IsLocalController())
	{
		return;
	}

	const bool bWasLocked = bHelicopterTransitLocked;
	const bool bLocked = HasActiveInsertionTransit();

	if (!bLocked)
	{
		const bool bNeedsDeploymentRestore =
			ReplicatedInsertionTransitState.Phase == EOBPlayerInsertionTransitPhase::Deployed
			&& (bWasLocked || bInsertionPresentationActive
				|| (IsValid(GetPawn()) && GetViewTarget() != GetPawn()));
		if (bNeedsDeploymentRestore && !IsInsertionDeploymentReady())
		{
			const float Now = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.f;
			if (InsertionDeploymentReadyWaitStartedRealTime < 0.f)
			{
				InsertionDeploymentReadyWaitStartedRealTime = Now;
			}
			const float Waited = Now - InsertionDeploymentReadyWaitStartedRealTime;
			if (Waited < MaxInsertionDeploymentReadyWaitSeconds)
			{
				if (!GetWorldTimerManager().IsTimerActive(InsertionDeploymentReadyTimer))
				{
					GetWorldTimerManager().SetTimer(
						InsertionDeploymentReadyTimer,
						this,
						&AOBPlayerController::PollInsertionDeploymentReady,
						FMath::Max(0.01f, InsertionDeploymentReadyPollInterval),
						false);
				}
				UE_LOG(LogOBInsertionInput, Verbose,
					TEXT("[InsertionState] Deployment restore deferred PC=%s Pawn=%s AttachedTo=%s MovementMode=%d Waited=%.2f Source=%s"),
					*GetName(), *GetNameSafe(GetPawn()),
					*GetNameSafe(GetPawn() ? GetPawn()->GetAttachParentActor() : nullptr),
					Cast<ACharacter>(GetPawn())
						? static_cast<int32>(CastChecked<ACharacter>(GetPawn())->GetCharacterMovement()->MovementMode)
						: -1,
					Waited, Source);
				return;
			}

			UE_LOG(LogOBInsertionInput, Error,
				TEXT("[InsertionState] Deployment readiness timed out; forcing owner restore PC=%s Pawn=%s AttachedTo=%s Waited=%.2f"),
				*GetName(), *GetNameSafe(GetPawn()),
				*GetNameSafe(GetPawn() ? GetPawn()->GetAttachParentActor() : nullptr), Waited);
		}

		GetWorldTimerManager().ClearTimer(InsertionDeploymentReadyTimer);
		InsertionDeploymentReadyWaitStartedRealTime = -1.f;
		bHelicopterTransitLocked = false;
		ApplyInsertionAbilityGate(false, false);
		SetPawnInputSuppressedForInsertion(false);
		const bool bNeedsDeployedViewRestore =
			ReplicatedInsertionTransitState.Phase == EOBPlayerInsertionTransitPhase::Deployed
			&& IsValid(GetPawn())
			&& GetViewTarget() != GetPawn();
		if (bWasLocked || bInsertionPresentationActive || bNeedsDeployedViewRestore)
		{
			RestoreGameplayViewAndInput(
				Cast<APawn>(ReplicatedInsertionTransitState.ViewTarget.Get()),
				Source);
		}
		return;
	}

	GetWorldTimerManager().ClearTimer(InsertionDeploymentReadyTimer);
	InsertionDeploymentReadyWaitStartedRealTime = -1.f;
	bHelicopterTransitLocked = true;
	ApplyInsertionAbilityGate(true, !bWasLocked);
	SetPawnInputSuppressedForInsertion(true);
	InsertionHelicopter = ReplicatedInsertionTransitState.Helicopter;
	AActor* EffectiveTarget = ReplicatedInsertionTransitState.ViewTarget;
	if (!IsValid(EffectiveTarget))
	{
		EffectiveTarget = IsValid(InsertionHelicopter.Get())
			? static_cast<AActor*>(InsertionHelicopter.Get())
			: static_cast<AActor*>(GetPawn());
	}
	if (IsValid(EffectiveTarget) && GetViewTarget() != EffectiveTarget)
	{
		SetViewTargetWithBlend(EffectiveTarget, InsertionViewBlendSeconds);
	}
	if (!bWasLocked
		&& ReplicatedInsertionTransitState.Phase == EOBPlayerInsertionTransitPhase::Seated
		&& IsValid(InsertionHelicopter.Get()))
	{
		SetControlRotation(InsertionHelicopter->GetCabinViewRotation());
	}

	UE_LOG(LogOBInsertionInput, Log,
		TEXT("[InsertionState] Owner apply Source=%s PC=%s Revision=%d Phase=%d Pawn=%s Helicopter=%s DesiredView=%s ActualView=%s Locked=true"),
		Source, *GetName(), ReplicatedInsertionTransitState.Revision,
		static_cast<int32>(ReplicatedInsertionTransitState.Phase),
		*GetNameSafe(GetPawn()), *GetNameSafe(InsertionHelicopter.Get()),
		*GetNameSafe(EffectiveTarget), *GetNameSafe(GetViewTarget()));
	ScheduleInsertionClientReconcile();
}

bool AOBPlayerController::IsInsertionDeploymentReady() const
{
	const APawn* ControlledPawn = GetPawn();
	if (!IsValid(ControlledPawn))
	{
		return false;
	}
	if (ControlledPawn->GetAttachParentActor() != nullptr)
	{
		return false;
	}
	if (const ACharacter* DeployedCharacter = Cast<ACharacter>(ControlledPawn))
	{
		const UCharacterMovementComponent* Movement = DeployedCharacter->GetCharacterMovement();
		return Movement && Movement->MovementMode != MOVE_None;
	}
	return true;
}

void AOBPlayerController::PollInsertionDeploymentReady()
{
	// A one-shot timer may still report itself active while executing its callback.
	// Clear the handle first so a still-attached Pawn can schedule the next poll.
	GetWorldTimerManager().ClearTimer(InsertionDeploymentReadyTimer);
	ApplyInsertionTransitStateLocal(TEXT("DeploymentReadyPoll"));
}

void AOBPlayerController::RequestInsertionPoint(const FVector2D& WorldXY)
{
	if (IsLocalController())
	{
		UE_LOG(LogOBInsertionInput, Log,
			TEXT("[InsertionInput] Client request PC=%s WorldXY=%s TransitLocked=%s Presentation=%s CanSelect=%s"),
			*GetName(), *WorldXY.ToString(), bHelicopterTransitLocked ? TEXT("true") : TEXT("false"),
			bInsertionPresentationActive ? TEXT("true") : TEXT("false"),
			bCanSelectInsertionTarget ? TEXT("true") : TEXT("false"));
		ShowInsertionDebugMessage(
			FString::Printf(TEXT("Server request sent XY=%s"), *WorldXY.ToString()),
			FColor::Yellow, 4.f, 77102);
		Server_RequestInsertionPoint(WorldXY);
	}
}

void AOBPlayerController::ToggleInsertionMapFromFocusedWidget()
{
	HandleInsertionMapToggle(TEXT("MapWidgetPreview"));
}

void AOBPlayerController::RequestInsertionPointFromFocusedWidget()
{
	HandleInsertionTraceRequest(TEXT("MapWidgetPreview"));
}

void AOBPlayerController::ApplyWorldMapInputMode(UUserWidget* MapWidget, const bool bOpen)
{
	if (!IsLocalController())
	{
		return;
	}
	if (bOpen && MapWidget)
	{
		FInputModeGameAndUI Mode;
		Mode.SetWidgetToFocus(MapWidget->TakeWidget());
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockOnCapture);
		SetInputMode(Mode);
		SetShowMouseCursor(true);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
	}
	UE_LOG(LogOBInsertionInput, Verbose,
		TEXT("[InsertionUI] Map input mode applied PC=%s Widget=%s Open=%s Transit=%s"),
		*GetName(), *GetNameSafe(MapWidget), bOpen ? TEXT("true") : TEXT("false"),
		bHelicopterTransitLocked ? TEXT("true") : TEXT("false"));
}

void AOBPlayerController::CloseWorldMapForModal(const TCHAR* Reason)
{
	AOBHUD* OBHUD = GetHUD<AOBHUD>();
	UOBWorldMapWidget* MapWidget = OBHUD ? OBHUD->GetWorldMapWidget() : nullptr;
	if (!MapWidget || !MapWidget->IsMapOpen())
	{
		return;
	}
	MapWidget->SetMapOpen(false);
	UE_LOG(LogOBInsertionInput, Log,
		TEXT("[InsertionUI] World map closed before modal transition PC=%s Reason=%s"),
		*GetName(), Reason ? Reason : TEXT("Unspecified"));
}

void AOBPlayerController::Server_RequestInsertionPoint_Implementation(FVector2D WorldXY)
{
	UE_LOG(LogOBInsertionInput, Log,
		TEXT("[InsertionInput] Server RPC received PC=%s WorldXY=%s Pawn=%s"),
		*GetName(), *WorldXY.ToString(), *GetNameSafe(GetPawn()));
	if (AOBExpeditionGameMode* ExpeditionGameMode = GetWorld()->GetAuthGameMode<AOBExpeditionGameMode>())
	{
		ExpeditionGameMode->RequestInsertionPoint(this, WorldXY);
	}
	else
	{
		UE_LOG(LogOBInsertionInput, Error,
			TEXT("[InsertionInput] Server RPC rejected: ExpeditionGameMode missing PC=%s World=%s"),
			*GetName(), *GetNameSafe(GetWorld()));
	}
}

void AOBPlayerController::Client_InsertionPointResult_Implementation(
	bool bAccepted,
	FVector_NetQuantize ResolvedLocation,
	const FString& Message)
{
	if (bAccepted)
	{
		UE_LOG(LogOBInsertionInput, Log,
			TEXT("[InsertionInput] Client result PC=%s Accepted=true Resolved=%s Message=%s"),
			*GetName(), *FVector(ResolvedLocation).ToCompactString(), *Message);
	}
	else
	{
		UE_LOG(LogOBInsertionInput, Warning,
			TEXT("[InsertionInput] Client result PC=%s Accepted=false Resolved=%s Message=%s"),
			*GetName(), *FVector(ResolvedLocation).ToCompactString(), *Message);
	}
	ShowInsertionDebugMessage(
		FString::Printf(TEXT("Server result %s | %s | %s"),
			bAccepted ? TEXT("ACCEPTED") : TEXT("REJECTED"),
			*FVector(ResolvedLocation).ToCompactString(), *Message),
		bAccepted ? FColor::Green : FColor::Red,
		InsertionDebugMessageDuration,
		77102);
	if (AOBHUD* OBHUD = GetHUD<AOBHUD>())
	{
		if (UOBWorldMapWidget* MapWidget = OBHUD->EnsureWorldMapWidget())
		{
			MapWidget->NotifyInsertionPointResult(bAccepted, FVector(ResolvedLocation), Message);
		}
		if (bAccepted)
		{
			OBHUD->CloseInsertionMap();
		}
	}
	BP_OnInsertionPointResult(bAccepted, FVector(ResolvedLocation), Message);
}

void AOBPlayerController::Server_ReportInsertionClientReady_Implementation(
	AOBInsertionHelicopter* Helicopter,
	EOBInsertionPhase ClientPhase,
	bool bViewTargetReady)
{
	const AOBPlayerStateBase* PS = GetPlayerState<AOBPlayerStateBase>();
	const bool bValidTeam = Helicopter && PS && Helicopter->GetTeamId() == PS->GetTeamId();
	if (bValidTeam)
	{
		UE_LOG(LogOBInsertionInput, Log,
			TEXT("[InsertionNet] Client ready ack PC=%s Team=%d Helicopter=%s HelicopterTeam=%d ClientPhase=%d ViewReady=%s ValidTeam=true"),
			*GetName(), PS->GetTeamId(), *GetNameSafe(Helicopter), Helicopter->GetTeamId(),
			static_cast<int32>(ClientPhase), bViewTargetReady ? TEXT("true") : TEXT("false"));
	}
	else
	{
		UE_LOG(LogOBInsertionInput, Warning,
			TEXT("[InsertionNet] Rejected client ready ack PC=%s Team=%d Helicopter=%s HelicopterTeam=%d ClientPhase=%d ViewReady=%s ValidTeam=false"),
			*GetName(), PS ? PS->GetTeamId() : 0, *GetNameSafe(Helicopter),
			Helicopter ? Helicopter->GetTeamId() : 0, static_cast<int32>(ClientPhase),
			bViewTargetReady ? TEXT("true") : TEXT("false"));
	}
}

void AOBPlayerController::BeginInsertionPresentationLocal(
	AOBInsertionHelicopter* Helicopter,
	float SelectionDeadlineServerTime,
	bool bCanSelectTarget,
	const TCHAR* Source)
{
	if (!IsLocalController() || !IsValid(Helicopter))
	{
		ScheduleInsertionClientReconcile();
		return;
	}
	if (!HasActiveInsertionTransit()
		|| ReplicatedInsertionTransitState.Helicopter != Helicopter)
	{
		UE_LOG(LogOBInsertionInput, Warning,
			TEXT("[InsertionNet] Presentation rejected without matching owner transit PC=%s OwnerPhase=%d OwnerHelicopter=%s TeamHelicopter=%s"),
			*GetName(), static_cast<int32>(ReplicatedInsertionTransitState.Phase),
			*GetNameSafe(ReplicatedInsertionTransitState.Helicopter), *GetNameSafe(Helicopter));
		return;
	}

	const bool bFirstStart = !bInsertionPresentationActive || InsertionHelicopter.Get() != Helicopter;
	if (bFirstStart)
	{
		bInsertionReadyAckSent = false;
		LastAppliedInsertionPhase = EOBInsertionPhase::None;
		LastAppliedInsertionPresentationRevision = 0;
	}
	const EOBInsertionPhase CurrentPhase = Helicopter->GetInsertionPhase();
	const bool bSelectionAvailable = CurrentPhase == EOBInsertionPhase::WaitingForTarget
		|| CurrentPhase == EOBInsertionPhase::Orbiting;
	const bool bSelectionBusy = CurrentPhase == EOBInsertionPhase::LoadingTarget
		|| CurrentPhase == EOBInsertionPhase::ValidatingTarget;
	bInsertionPresentationActive = true;
	bCanSelectInsertionTarget = bCanSelectTarget;
	bInsertionTargetSelectionAvailable = bCanSelectTarget && bSelectionAvailable;
	InsertionSelectionDeadlineServerTime = SelectionDeadlineServerTime;
	InsertionHelicopter = Helicopter;
	InsertionMapOpenAttempts = 0;
	GetWorldTimerManager().ClearTimer(InsertionClientReconcileTimer);

	AActor* DesiredViewTarget = ReplicatedInsertionTransitState.ViewTarget;
	if (!IsValid(DesiredViewTarget))
	{
		DesiredViewTarget = Helicopter;
	}
	if (GetViewTarget() != DesiredViewTarget)
	{
		SetViewTargetWithBlend(DesiredViewTarget, InsertionViewBlendSeconds);
	}
	if (bFirstStart
		&& ReplicatedInsertionTransitState.Phase == EOBPlayerInsertionTransitPhase::Seated)
	{
		SetControlRotation(Helicopter->GetCabinViewRotation());
	}
	EnterInsertionInputMode(bCanSelectTarget);

	UE_LOG(LogOBInsertionInput, Log,
		TEXT("[InsertionNet] Presentation active Source=%s PC=%s Pawn=%s ViewTarget=%s Helicopter=%s Team=%d Phase=%d LeaderCanSelect=%s Deadline=%.2f First=%s"),
		Source, *GetName(), *GetNameSafe(GetPawn()), *GetNameSafe(GetViewTarget()), *GetNameSafe(Helicopter),
		Helicopter->GetTeamId(), static_cast<int32>(Helicopter->GetInsertionPhase()),
		bCanSelectTarget ? TEXT("true") : TEXT("false"), SelectionDeadlineServerTime,
		bFirstStart ? TEXT("true") : TEXT("false"));
	ShowInsertionDebugMessage(
		FString::Printf(TEXT("CLIENT READY | Mouse Look | M Map | E Trace | Leader=%s"),
			bCanSelectTarget ? TEXT("true") : TEXT("false")),
		FColor::Cyan, 10.f, 77100);
	if (bFirstStart)
	{
		BP_OnInsertionPresentationStarted(Helicopter, SelectionDeadlineServerTime, bCanSelectTarget);
	}
	if (bSelectionAvailable || bSelectionBusy)
	{
		TryOpenInsertionMap();
	}

	if (!bInsertionReadyAckSent)
	{
		bInsertionReadyAckSent = true;
		Server_ReportInsertionClientReady(
			Helicopter, Helicopter->GetInsertionPhase(), GetViewTarget() == Helicopter);
	}
}

void AOBPlayerController::ApplyInsertionPresentationUpdateLocal(
	EOBInsertionPhase Phase,
	const FString& StatusMessage,
	bool bForceMapOpen,
	const TCHAR* Source)
{
	LastAppliedInsertionPhase = Phase;
	UE_LOG(LogOBInsertionInput, Log,
		TEXT("[InsertionNet] Presentation update Source=%s PC=%s Phase=%d ForceMap=%s CanSelect=%s Message=%s"),
		Source, *GetName(), static_cast<int32>(Phase), bForceMapOpen ? TEXT("true") : TEXT("false"),
		bCanSelectInsertionTarget ? TEXT("true") : TEXT("false"), *StatusMessage);
	ShowInsertionDebugMessage(
		FString::Printf(TEXT("State Phase=%d CanSelect=%s | %s"),
			static_cast<int32>(Phase),
			bCanSelectInsertionTarget ? TEXT("true") : TEXT("false"),
			*StatusMessage),
		FColor::Cyan, InsertionDebugMessageDuration, 77100);
	BP_OnInsertionPresentationUpdated(Phase, StatusMessage, bForceMapOpen);

	const bool bSelectionAvailable = Phase == EOBInsertionPhase::WaitingForTarget
		|| Phase == EOBInsertionPhase::Orbiting;
	const bool bSelectionBusy = Phase == EOBInsertionPhase::LoadingTarget
		|| Phase == EOBInsertionPhase::ValidatingTarget;
	bInsertionTargetSelectionAvailable = bSelectionAvailable && bCanSelectInsertionTarget;

	if (AOBHUD* OBHUD = GetHUD<AOBHUD>())
	{
		if (bSelectionAvailable)
		{
			if (!OBHUD->OpenInsertionMap(bCanSelectInsertionTarget) && bForceMapOpen)
			{
				InsertionMapOpenAttempts = 0;
				TryOpenInsertionMap();
			}
		}
		else if (bSelectionBusy)
		{
			OBHUD->OpenInsertionMap(false);
		}
		else if (Phase == EOBInsertionPhase::Approaching || Phase == EOBInsertionPhase::Scanning
			|| Phase == EOBInsertionPhase::Hovering || Phase == EOBInsertionPhase::Rappelling)
		{
			OBHUD->CloseInsertionMap();
		}
	}
	else if (bForceMapOpen)
	{
		InsertionMapOpenAttempts = 0;
		TryOpenInsertionMap();
	}
}

void AOBPlayerController::ScheduleInsertionClientReconcile()
{
	if (!IsLocalController() || !GetWorld()
		|| GetWorldTimerManager().IsTimerActive(InsertionClientReconcileTimer))
	{
		return;
	}
	GetWorldTimerManager().SetTimer(
		InsertionClientReconcileTimer,
		this,
		&AOBPlayerController::ReconcileInsertionPresentationFromGameState,
		0.25f,
		false);
}

void AOBPlayerController::ReconcileInsertionPresentationFromGameState()
{
	if (!IsLocalController() || !GetWorld())
	{
		return;
	}

	AOBExpeditionGameState* GS = GetWorld()->GetGameState<AOBExpeditionGameState>();
	AOBPlayerStateBase* PS = GetPlayerState<AOBPlayerStateBase>();
	if (!GS || !PS)
	{
		ScheduleInsertionClientReconcile();
		return;
	}
	if (GS->GetPhase() != EOBExpeditionPhase::Insertion)
	{
		return;
	}
	if (!HasActiveInsertionTransit())
	{
		// Team mission state is not player ownership. A deployed or late-join
		// player must never be relocked merely because teammates are still aboard.
		UE_LOG(LogOBInsertionInput, Verbose,
			TEXT("[InsertionNet] Reconcile skipped for non-transit owner PC=%s OwnerPhase=%d Presentation=%s"),
			*GetName(), static_cast<int32>(ReplicatedInsertionTransitState.Phase),
			bInsertionPresentationActive ? TEXT("true") : TEXT("false"));
		return;
	}

	FOBTeamInsertionState TeamState;
	const bool bHasTeamState = GS->GetTeamInsertionState(PS->GetTeamId(), TeamState);
	if (!bHasTeamState || !IsValid(TeamState.Helicopter)
		|| !IsValid(ReplicatedInsertionTransitState.Helicopter)
		|| ReplicatedInsertionTransitState.Helicopter != TeamState.Helicopter)
	{
		UE_LOG(LogOBInsertionInput, Verbose,
			TEXT("[InsertionNet] Reconcile waiting PC=%s Team=%d HasState=%s TeamHelicopter=%s OwnerHelicopter=%s OwnerPhase=%d"),
			*GetName(), PS->GetTeamId(),
			bHasTeamState ? TEXT("true") : TEXT("false"),
			*GetNameSafe(TeamState.Helicopter),
			*GetNameSafe(ReplicatedInsertionTransitState.Helicopter),
			static_cast<int32>(ReplicatedInsertionTransitState.Phase));
		ScheduleInsertionClientReconcile();
		return;
	}

	if (!bInsertionPresentationActive || InsertionHelicopter.Get() != TeamState.Helicopter)
	{
		BeginInsertionPresentationLocal(
			TeamState.Helicopter,
			TeamState.SelectionDeadlineServerTime,
			ReplicatedInsertionTransitState.bCanSelectTarget,
			TEXT("GameStateReconcile"));
	}

	const bool bSelectionPermissionChanged = bInsertionPresentationActive
		&& bCanSelectInsertionTarget != ReplicatedInsertionTransitState.bCanSelectTarget;
	if (bInsertionPresentationActive
		&& (LastAppliedInsertionPhase != TeamState.Phase
			|| LastAppliedInsertionPresentationRevision != TeamState.PresentationRevision
			|| bSelectionPermissionChanged))
	{
		bCanSelectInsertionTarget = ReplicatedInsertionTransitState.bCanSelectTarget;
		const FString StatusMessage = TeamState.StatusMessage.IsEmpty()
			? TEXT("Insertion state synchronized from the server.")
			: TeamState.StatusMessage;
		ApplyInsertionPresentationUpdateLocal(
			TeamState.Phase, StatusMessage, TeamState.bForceMapOpen,
			TEXT("GameStateReconcile"));
		LastAppliedInsertionPresentationRevision = TeamState.PresentationRevision;
	}
}

void AOBPlayerController::EnterInsertionInputMode(bool bCanSelectTarget)
{
	bCanSelectInsertionTarget = bCanSelectTarget;
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (InsertionMappingContext && !bInsertionMappingContextAdded)
		{
			Subsystem->AddMappingContext(InsertionMappingContext, InsertionInputMappingPriority);
			bInsertionMappingContextAdded = true;
			UE_LOG(LogOBInsertionInput, Log,
				TEXT("[InsertionInput] MappingContext added PC=%s Context=%s Priority=%d"),
				*GetName(), *InsertionMappingContext->GetName(), InsertionInputMappingPriority);
		}
		else if (!InsertionMappingContext)
		{
			UE_LOG(LogOBInsertionInput, Verbose,
				TEXT("[InsertionInput] Optional InsertionMappingContext is not assigned on %s; Default Map/Interact actions remain authoritative."),
				*GetClass()->GetName());
		}
	}
}

void AOBPlayerController::ExitInsertionInputMode()
{
	GetWorldTimerManager().ClearTimer(InsertionMapOpenRetryTimer);
	if (bInsertionMappingContextAdded)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			Subsystem->RemoveMappingContext(InsertionMappingContext);
		}
		UE_LOG(LogOBInsertionInput, Log,
			TEXT("[InsertionInput] MappingContext removed PC=%s Context=%s"),
			*GetName(), *GetNameSafe(InsertionMappingContext.Get()));
		bInsertionMappingContextAdded = false;
	}
}

void AOBPlayerController::TryOpenInsertionMap()
{
	if (!IsLocalController() || !bInsertionPresentationActive)
	{
		return;
	}

	++InsertionMapOpenAttempts;
	AOBHUD* OBHUD = GetHUD<AOBHUD>();
	if (OBHUD && OBHUD->OpenInsertionMap(
		bCanSelectInsertionTarget && bInsertionTargetSelectionAvailable))
	{
		GetWorldTimerManager().ClearTimer(InsertionMapOpenRetryTimer);
		UE_LOG(LogOBInsertionInput, Log,
			TEXT("[InsertionUI] Auto-open succeeded PC=%s HUD=%s Attempt=%d CanSelect=%s"),
			*GetName(), *OBHUD->GetName(), InsertionMapOpenAttempts,
			bCanSelectInsertionTarget ? TEXT("true") : TEXT("false"));
		return;
	}

	if (InsertionMapOpenAttempts >= 25)
	{
		UE_LOG(LogOBInsertionInput, Error,
			TEXT("[InsertionUI] Auto-open failed permanently PC=%s HUD=%s HUDClass=%s Attempts=%d"),
			*GetName(), *GetNameSafe(OBHUD), *GetNameSafe(GetHUD() ? GetHUD()->GetClass() : nullptr),
			InsertionMapOpenAttempts);
		return;
	}

	UE_LOG(LogOBInsertionInput, Warning,
		TEXT("[InsertionUI] Auto-open deferred PC=%s HUD=%s Attempt=%d/25"),
		*GetName(), *GetNameSafe(OBHUD), InsertionMapOpenAttempts);
	GetWorldTimerManager().SetTimer(
		InsertionMapOpenRetryTimer, this, &AOBPlayerController::TryOpenInsertionMap, 0.2f, false);
}

void AOBPlayerController::HandleInsertionMapToggle(const TCHAR* InputSource)
{
	AOBHUD* OBHUD = GetHUD<AOBHUD>();
	const UOBWorldMapWidget* ExistingMapWidget = OBHUD ? OBHUD->GetWorldMapWidget() : nullptr;
	const bool bMapAlreadyOpen = ExistingMapWidget && ExistingMapWidget->IsMapOpen();
	const bool bOtherGameplayModalOpen = IsInventoryInputBlocked()
		|| (PartyWidget && PartyWidget->IsInViewport())
		|| IsValid(ActiveInteractionWidget);
	if (!bMapAlreadyOpen && !bHelicopterTransitLocked && bOtherGameplayModalOpen)
	{
		UE_LOG(LogOBInsertionInput, Warning,
			TEXT("[InsertionUI] Map toggle rejected while another modal owns input PC=%s Inventory=%s Party=%s Interaction=%s Source=%s"),
			*GetName(), IsInventoryInputBlocked() ? TEXT("true") : TEXT("false"),
			PartyWidget && PartyWidget->IsInViewport() ? TEXT("true") : TEXT("false"),
			IsValid(ActiveInteractionWidget) ? TEXT("true") : TEXT("false"),
			InputSource);
		return;
	}

	UE_LOG(LogOBInsertionInput, Display,
		TEXT("[InsertionInput] M accepted Source=%s PC=%s HUD=%s TransitLocked=%s Presentation=%s"),
		InputSource, *GetName(), *GetNameSafe(GetHUD()),
		bHelicopterTransitLocked ? TEXT("true") : TEXT("false"),
		bInsertionPresentationActive ? TEXT("true") : TEXT("false"));
	ShowInsertionDebugMessage(
		FString::Printf(TEXT("M received [%s]"), InputSource),
		FColor::Yellow, 3.f, 77101);

	if (OBHUD)
	{
		OBHUD->ToggleWorldMap();
		const UOBWorldMapWidget* MapWidget = OBHUD->GetWorldMapWidget();
		const bool bMapOpen = MapWidget && MapWidget->IsMapOpen();
		UE_LOG(LogOBInsertionInput, Display,
			TEXT("[InsertionUI] M toggle complete Source=%s Widget=%s Open=%s"),
			InputSource, *GetNameSafe(MapWidget), bMapOpen ? TEXT("true") : TEXT("false"));
		ShowInsertionDebugMessage(
			FString::Printf(TEXT("Map %s"), bMapOpen ? TEXT("OPEN") : TEXT("CLOSED")),
			bMapOpen ? FColor::Cyan : FColor::Silver, 3.f, 77101);
	}
	else
	{
		UE_LOG(LogOBInsertionInput, Error,
			TEXT("[InsertionUI] ToggleMap failed: AOBHUD missing PC=%s HUD=%s HUDClass=%s Source=%s"),
			*GetName(), *GetNameSafe(GetHUD()), *GetNameSafe(GetHUD() ? GetHUD()->GetClass() : nullptr),
			InputSource);
		ShowInsertionDebugMessage(TEXT("Map toggle FAILED: AOBHUD missing"), FColor::Red, 8.f, 77101);
		if (bInsertionPresentationActive)
		{
			InsertionMapOpenAttempts = 0;
			TryOpenInsertionMap();
		}
	}
}

void AOBPlayerController::HandleInsertionTraceRequest(const TCHAR* InputSource)
{
	UE_LOG(LogOBInsertionInput, Display,
		TEXT("[InsertionInput] E accepted Source=%s PC=%s ViewTarget=%s TransitLocked=%s Presentation=%s CanSelect=%s Available=%s"),
		InputSource, *GetName(), *GetNameSafe(GetViewTarget()),
		bHelicopterTransitLocked ? TEXT("true") : TEXT("false"),
		bInsertionPresentationActive ? TEXT("true") : TEXT("false"),
		bCanSelectInsertionTarget ? TEXT("true") : TEXT("false"),
		bInsertionTargetSelectionAvailable ? TEXT("true") : TEXT("false"));
	ShowInsertionDebugMessage(
		FString::Printf(TEXT("E received [%s] -> trace requested"), InputSource),
		FColor::Yellow, 4.f, 77102);
	TryRequestInsertionPointFromView();
}

void AOBPlayerController::TryRequestInsertionPointFromView()
{
	if (!IsLocalController() || !bHelicopterTransitLocked || !bInsertionPresentationActive)
	{
		ReportLocalInsertionPointFailure(TEXT("Insertion view targeting is not active."));
		return;
	}
	if (!bCanSelectInsertionTarget)
	{
		ReportLocalInsertionPointFailure(TEXT("Only the party leader can select the insertion point."));
		return;
	}
	if (!bInsertionTargetSelectionAvailable)
	{
		ReportLocalInsertionPointFailure(TEXT("The current insertion phase does not accept another target."));
		return;
	}

	AOBInsertionHelicopter* Helicopter = InsertionHelicopter.Get();
	if (!Helicopter || GetViewTarget() != Helicopter)
	{
		ReportLocalInsertionPointFailure(TEXT("The helicopter camera is not the active view target."));
		return;
	}

	FVector TraceStart;
	FRotator ViewRotation;
	GetPlayerViewPoint(TraceStart, ViewRotation);
	const FVector TraceEnd = TraceStart + ViewRotation.Vector() * FMath::Max(1000.f, InsertionTargetTraceDistance);
	UE_LOG(LogOBInsertionInput, Display,
		TEXT("[InsertionTrace] Begin PC=%s Start=%s End=%s Rotation=%s ViewTarget=%s Channel=%d Distance=%.0f"),
		*GetName(), *TraceStart.ToCompactString(), *TraceEnd.ToCompactString(),
		*ViewRotation.ToCompactString(), *GetNameSafe(GetViewTarget()),
		static_cast<int32>(InsertionTargetTraceChannel.GetValue()), InsertionTargetTraceDistance);
	ShowInsertionDebugMessage(
		FString::Printf(TEXT("TRACE START Rot=%s Distance=%.0f"),
			*ViewRotation.ToCompactString(), InsertionTargetTraceDistance),
		FColor::Yellow, 4.f, 77102);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(OBInsertionViewTarget), false, GetPawn());
	QueryParams.AddIgnoredActor(Helicopter);
	FHitResult Hit;
	const bool bBlockingHit = GetWorld() && GetWorld()->LineTraceSingleByChannel(
		Hit, TraceStart, TraceEnd, InsertionTargetTraceChannel, QueryParams);

#if ENABLE_DRAW_DEBUG
	if (bDrawInsertionTargetTrace && GetWorld())
	{
		DrawDebugLine(
			GetWorld(), TraceStart, bBlockingHit ? Hit.ImpactPoint : TraceEnd,
			bBlockingHit ? FColor::Green : FColor::Red, false, 8.f, 0, 4.f);
		if (bBlockingHit)
		{
			DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 100.f, 12, FColor::Green, false, 8.f, 0, 5.f);
		}
	}
#endif

	if (!bBlockingHit)
	{
		const FString Message = TEXT("The helicopter view trace did not hit the map.");
		UE_LOG(LogOBInsertionInput, Warning,
			TEXT("[InsertionTrace] Miss PC=%s Start=%s End=%s Rotation=%s Channel=%d Distance=%.0f"),
			*GetName(), *TraceStart.ToCompactString(), *TraceEnd.ToCompactString(),
			*ViewRotation.ToCompactString(), static_cast<int32>(InsertionTargetTraceChannel.GetValue()),
			InsertionTargetTraceDistance);
		ShowInsertionDebugMessage(TEXT("TRACE MISS: no Visibility blocking hit"), FColor::Red, 8.f, 77102);
		BP_OnInsertionViewTrace(false, TraceStart, TraceEnd, FVector::ZeroVector, Message);
		ReportLocalInsertionPointFailure(Message);
		return;
	}

	UE_LOG(LogOBInsertionInput, Log,
		TEXT("[InsertionTrace] Hit PC=%s Start=%s Impact=%s Actor=%s Component=%s Normal=%s Channel=%d"),
		*GetName(), *TraceStart.ToCompactString(), *Hit.ImpactPoint.ToCompactString(),
		*GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), *Hit.ImpactNormal.ToCompactString(),
		static_cast<int32>(InsertionTargetTraceChannel.GetValue()));
	ShowInsertionDebugMessage(
		FString::Printf(TEXT("TRACE HIT %s | Actor=%s | sending to server"),
			*Hit.ImpactPoint.ToCompactString(), *GetNameSafe(Hit.GetActor())),
		FColor::Green, 8.f, 77102);
	BP_OnInsertionViewTrace(true, TraceStart, TraceEnd, Hit.ImpactPoint,
		TEXT("Insertion target trace hit. Waiting for server validation."));
	RequestInsertionPoint(FVector2D(Hit.ImpactPoint.X, Hit.ImpactPoint.Y));
}

void AOBPlayerController::ReportLocalInsertionPointFailure(const FString& Message, const FVector& Location)
{
	UE_LOG(LogOBInsertionInput, Warning,
		TEXT("[InsertionTrace] Local rejection PC=%s Location=%s Message=%s"),
		*GetName(), *Location.ToCompactString(), *Message);
	ShowInsertionDebugMessage(
		FString::Printf(TEXT("LOCAL REJECT | %s"), *Message),
		FColor::Red, 8.f, 77102);
	if (AOBHUD* OBHUD = GetHUD<AOBHUD>())
	{
		if (UOBWorldMapWidget* MapWidget = OBHUD->EnsureWorldMapWidget())
		{
			MapWidget->NotifyInsertionPointResult(false, Location, Message);
		}
	}
	BP_OnInsertionPointResult(false, Location, Message);
}

void AOBPlayerController::RestoreGameplayViewAndInput(APawn* RestoredPawn, const TCHAR* Reason)
{
	APawn* CurrentPawn = GetPawn();
	APawn* EffectivePawn = IsValid(RestoredPawn) ? RestoredPawn : CurrentPawn;
	const bool bWasLocked = bHelicopterTransitLocked;
	const bool bWasPresentationActive = bInsertionPresentationActive;
	SetPawnInputSuppressedForInsertion(false);

	bHelicopterTransitLocked = false;
	bInsertionPresentationActive = false;
	bCanSelectInsertionTarget = false;
	bInsertionTargetSelectionAvailable = false;
	InsertionSelectionDeadlineServerTime = 0.f;
	LastAppliedInsertionPhase = EOBInsertionPhase::None;
	LastAppliedInsertionPresentationRevision = 0;
	bInsertionReadyAckSent = false;
	InsertionHelicopter.Reset();
	GetWorldTimerManager().ClearTimer(InsertionClientReconcileTimer);
	GetWorldTimerManager().ClearTimer(InsertionDeploymentReadyTimer);
	InsertionDeploymentReadyWaitStartedRealTime = -1.f;
	ExitInsertionInputMode();

	if (UOBAbilitySystemComponent* ASC = GetOBAbilitySystemComponent())
	{
		ASC->ClearAbilityInput();
		ASC->SetLooseGameplayTagCount(OBGameplayTags::State_HelicopterTransit, 0);
	}
	if (AOBHUD* OBHUD = GetHUD<AOBHUD>())
	{
		OBHUD->CloseInsertionMap();
	}
	if (IsLocalController())
	{
		EnsureDefaultInputMappingContext();
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
		ApplyInsertionExitCamera(EffectivePawn);
	}
	else if (HasAuthority() && EffectivePawn)
	{
		// Server SetViewTarget is retained for relevancy/authority bookkeeping.
		SetViewTarget(EffectivePawn);
	}

	UE_LOG(LogOBInsertionInput, Log,
		TEXT("[InsertionInput] Restore PC=%s Pawn=%s ViewTarget=%s Reason=%s WasLocked=%s WasPresentation=%s"),
		*GetName(), *GetNameSafe(EffectivePawn), *GetNameSafe(GetViewTarget()), Reason,
		bWasLocked ? TEXT("true") : TEXT("false"),
		bWasPresentationActive ? TEXT("true") : TEXT("false"));
	if (bWasLocked || bWasPresentationActive)
	{
		BP_OnInsertionPresentationEnded(EffectivePawn);
	}
}

void AOBPlayerController::ApplyInsertionExitCamera(APawn* RestoredPawn)
{
	if (!IsLocalController() || !IsValid(RestoredPawn))
	{
		return;
	}

	const FRotator RestoredRotation(
		InsertionExitCameraPitch,
		RestoredPawn->GetActorRotation().Yaw,
		0.f);
	SetControlRotation(RestoredRotation);

	UGameplayCameraComponentBase* GameplayCamera =
		RestoredPawn->FindComponentByClass<UGameplayCameraComponentBase>();
	if (GameplayCamera)
	{
		GameplayCamera->ActivateCameraForPlayerController(
			this,
			true,
			EGameplayCameraComponentActivationMode::Push);
	}

	const bool bViewTargetNeedsCorrection = GetViewTarget() != RestoredPawn;
	if (bViewTargetNeedsCorrection)
	{
		if (bCutCameraOnInsertionExit)
		{
			SetViewTarget(RestoredPawn);
		}
		else
		{
			SetViewTargetWithBlend(RestoredPawn, InsertionViewBlendSeconds);
		}
	}
	if (bCutCameraOnInsertionExit && PlayerCameraManager)
	{
		PlayerCameraManager->SetGameCameraCutThisFrame();
	}

	const FVector CameraLocation = PlayerCameraManager
		? PlayerCameraManager->GetCameraLocation()
		: FVector::ZeroVector;
	const bool bGameplayCameraReady = GameplayCamera
		&& GameplayCamera->GetEvaluationContext().IsValid();
	UE_LOG(LogOBInsertionInput, Display,
		TEXT("[InsertionCamera] Gameplay provider restored PC=%s Pawn=%s AttachedTo=%s PawnLocation=%s ViewTarget=%s ControlRotation=%s CameraManager=%s PreviousCameraLocation=%s GameplayCamera=%s GameplayReady=%s Cut=%s"),
		*GetName(), *RestoredPawn->GetName(), *GetNameSafe(RestoredPawn->GetAttachParentActor()),
		*RestoredPawn->GetActorLocation().ToCompactString(), *GetNameSafe(GetViewTarget()),
		*GetControlRotation().ToCompactString(), *GetNameSafe(PlayerCameraManager), *CameraLocation.ToCompactString(),
		*GetNameSafe(GameplayCamera), bGameplayCameraReady ? TEXT("true") : TEXT("false"),
		bCutCameraOnInsertionExit ? TEXT("true") : TEXT("false"));
	ShowInsertionDebugMessage(
		FString::Printf(TEXT("GAMEPLAY CAMERA RESTORED View=%s Rot=%s"),
			*GetNameSafe(GetViewTarget()), *GetControlRotation().ToCompactString()),
		FColor::Green, 5.f, 77105);
}

void AOBPlayerController::EnsureDefaultInputMappingContext()
{
	if (!IsLocalController() || !DefaultMappingContext)
	{
		return;
	}
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (!Subsystem->HasMappingContext(DefaultMappingContext))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, InputMappingPriority);
			UE_LOG(LogOBInsertionInput, Log,
				TEXT("[InsertionInput] Default mapping restored PC=%s Context=%s Priority=%d"),
				*GetName(), *DefaultMappingContext->GetName(), InputMappingPriority);
		}
	}
}

void AOBPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController()) return;

	EnsureDefaultInputMappingContext();

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
			const TArray<FInventoryData> WeaponInstances = Loadout->GetSelectedWeaponInstances();
			if (!WeaponInstances.IsEmpty())
			{
				Server_ApplyLoadoutInstances(WeaponInstances);
			}
			else
			{
				const TArray<TSubclassOf<AOBWeaponBase>> Classes = Loadout->GetSelectedClasses();
				if (!Classes.IsEmpty()) Server_ApplyLoadout(Classes);
			}

			const TArray<FOBItemStack>& CarryStacks = Loadout->GetCarryStackItems();
			const TArray<FInventoryData>& CarryInstances = Loadout->GetCarryItemInstances();
			if (!CarryStacks.IsEmpty() || !CarryInstances.IsEmpty())
			{
				Server_ApplyCarryLoadout(CarryStacks, CarryInstances);
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
	ApplyInsertionTransitStateLocal(TEXT("OnRepPlayerState"));
	ScheduleInsertionClientReconcile();
}

void AOBPlayerController::OnRep_Pawn()
{
	Super::OnRep_Pawn();
	EnsureDefaultInputMappingContext();
	ApplyInsertionTransitStateLocal(TEXT("OnRepPawn"));
	ScheduleInsertionClientReconcile();
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
		if (bHelicopterTransitLocked)
		{
			// A Pawn Blueprint can add its legacy context after possession/BeginPlay.
			// Reassert suppression while aboard so that it cannot consume Look/E/M
			// before the controller-owned insertion actions.
			SetPawnMappingContextsSuppressedForInsertion(true);
		}
		
		// 누적 입력을 능력 발동/통지로 처리.
		if (UOBAbilitySystemComponent* ASC = GetOBAbilitySystemComponent())
		{
			if (!bHelicopterTransitLocked)
			{
				ASC->ProcessAbilityInput(DeltaSeconds, false);
			}
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
	
	if (MapAction)
	{
		EIC->BindAction(MapAction, ETriggerEvent::Started, this, &AOBPlayerController::Input_ToggleMap);
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

bool AOBPlayerController::IsInventoryInputBlocked() const
{
	AOBCharacterBase* CharacterBase = Cast<AOBCharacterBase>(GetPawn());
	UPlayerInventoryComponent* Inventory = CharacterBase ? CharacterBase->GetPlayerInventoryComponent() : nullptr;
	
	return Inventory && Inventory->IsInventoryOpen();
}

void AOBPlayerController::Input_Move(const FInputActionValue& Value)
{
	if (bHelicopterTransitLocked) return;

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
	if (IsInventoryInputBlocked()) return;
	
	const FVector2D AxisValue = Value.Get<FVector2D>();
	if (bHelicopterTransitLocked)
	{
		// The helicopter CalcCamera consumes this controller rotation locally,
		// so each passenger gets an independent free-look direction.
		AddYawInput(AxisValue.X);
		AddPitchInput(AxisValue.Y);

		const float Now = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.f;
		if (Now - LastInsertionLookDebugRealTime >= 0.5f)
		{
			LastInsertionLookDebugRealTime = Now;
			UE_LOG(LogOBInsertionInput, Display,
				TEXT("[InsertionLook] Input received PC=%s Axis=%s ControlRotation=%s ViewTarget=%s"),
				*GetName(), *AxisValue.ToString(), *GetControlRotation().ToCompactString(),
				*GetNameSafe(GetViewTarget()));
			ShowInsertionDebugMessage(
				FString::Printf(TEXT("LOOK received Axis=%s Rot=%s"),
					*AxisValue.ToString(), *GetControlRotation().ToCompactString()),
				FColor::Green, 0.75f, 77103);
		}
		return;
	}

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return;
	}

	ControlledPawn->AddControllerYawInput(AxisValue.X);
	ControlledPawn->AddControllerPitchInput(AxisValue.Y);
}

void AOBPlayerController::Input_JumpStarted()
{
	if (IsInventoryInputBlocked()) return;
	
	if (bHelicopterTransitLocked) return;

	if (ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn()))
	{
		ControlledCharacter->Jump();
	}
}

void AOBPlayerController::Input_JumpCompleted()
{
	if (IsInventoryInputBlocked()) return;
	
	if (bHelicopterTransitLocked) return;

	if (ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn()))
	{
		ControlledCharacter->StopJumping();
	}
}

void AOBPlayerController::Input_InventoryKey()
{
	if (bHelicopterTransitLocked) return;
	if ((PartyWidget && PartyWidget->IsInViewport()) || IsValid(ActiveInteractionWidget)) return;

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
	CloseWorldMapForModal(TEXT("InventoryOpened"));

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
		UE_LOG(LogTemp, Error, TEXT("%s::%s: PlayerInventoryComponent is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
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
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	SetShowMouseCursor(true);
	
	// 발사/ADS를 누른 채로 인벤토리를 열면 뗀 입력이 안 들어와 눌린 상태로 굳는다.
	if (UOBAbilitySystemComponent* ASC = GetOBAbilitySystemComponent())
	{
		ASC->FlushPlayerAbilityInput(TEXT("InventoryOpened"));
	}	
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
	if (IsInventoryInputBlocked()) return;
	
	if (bHelicopterTransitLocked) return;

	if (UOBAbilitySystemComponent* ASC = GetOBAbilitySystemComponent())
	{
		ASC->AbilityInputTagPressed(InputTag);
	}
}

void AOBPlayerController::Input_AbilityInputReleased(FGameplayTag InputTag)
{
	if (IsInventoryInputBlocked()) return;
	
	if (bHelicopterTransitLocked) return;

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
	if (IsInventoryInputBlocked()) return;
	
	if (bHelicopterTransitLocked) return;

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
	if (IsInventoryInputBlocked()) return;
	
	if (bHelicopterTransitLocked) return;

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
		EnsureDefaultInputMappingContext();
		ApplyInsertionTransitStateLocal(TEXT("AcknowledgePossession"));
		if (!bHelicopterTransitLocked)
		{
			FInputModeGameOnly Mode;
			SetInputMode(Mode);
			SetShowMouseCursor(false);
		}
	}
}

void AOBPlayerController::Input_Interact()
{
	if (IsInventoryInputBlocked()) return;
	
	if (bHelicopterTransitLocked)
	{
		HandleInsertionTraceRequest(TEXT("EnhancedInput"));
		return;
	}

	if (AOBInteractableActor* Target = CurrentInteractable.Get())
		Target->Interact(this);
}

void AOBPlayerController::Input_TogglePartyUI()
{
	if (bHelicopterTransitLocked) return;
	if (IsInventoryInputBlocked() || IsValid(ActiveInteractionWidget)) return;

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
		CloseWorldMapForModal(TEXT("PartyOpened"));
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
	
	// 내 팀이 전멸한 시점에 이 플레이어의 판은 끝났다. 다른 팀의 세션 종료를 기다릴 이유가 없으므로 개인 결과창을 같은 지연으로 띄운다.
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
	GS->OnTeamInsertionStatesChanged.AddDynamic(this, &AOBPlayerController::HandleTeamInsertionStatesChanged);
	bPhaseBound = true;
	
	// Apply the current phase immediately. This also covers clients that bind
	// after the insertion phase replication/event has already happened.
	HandleExpeditionPhaseChanged(GS->GetPhase());
}

void AOBPlayerController::HandleExpeditionPhaseChanged(EOBExpeditionPhase NewPhase)
{
	if (!IsLocalController()) return;
	UE_LOG(LogOBInsertionInput, Log,
		TEXT("[InsertionInput] Expedition phase PC=%s Phase=%d TransitLocked=%s Presentation=%s"),
		*GetName(), static_cast<int32>(NewPhase),
		bHelicopterTransitLocked ? TEXT("true") : TEXT("false"),
		bInsertionPresentationActive ? TEXT("true") : TEXT("false"));

	if (NewPhase == EOBExpeditionPhase::Insertion)
	{
		// Team state drives presentation only after the owner-only transit state
		// proves that this player is still aboard.
		ScheduleInsertionClientReconcile();
		return;
	}
	if (NewPhase == EOBExpeditionPhase::InProgress)
	{
		if (HasActiveInsertionTransit())
		{
			// GameState and PlayerController replicate on different actor channels.
			// Never clear only the local lock when InProgress wins that race: doing
			// so would leave the authoritative ASC tag active and a later owner-state
			// OnRep would lock the player again. The server commits Deployed before
			// advancing the expedition, so wait for that owner-only transition.
			UE_LOG(LogOBInsertionInput, Warning,
				TEXT("[InsertionState] Expedition entered InProgress before owner transit release PC=%s OwnerPhase=%d Revision=%d; waiting for Deployed replication."),
				*GetName(), static_cast<int32>(ReplicatedInsertionTransitState.Phase),
				ReplicatedInsertionTransitState.Revision);
			return;
		}
		if (bHelicopterTransitLocked || bInsertionPresentationActive)
		{
			RestoreGameplayViewAndInput(GetPawn(), TEXT("ExpeditionInProgressFailsafe"));
		}
		return;
	}
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

void AOBPlayerController::HandleTeamInsertionStatesChanged()
{
	if (!IsLocalController())
	{
		return;
	}
	ReconcileInsertionPresentationFromGameState();
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

void AOBPlayerController::Client_ApplyExtractionResultV2_Implementation(
	const TArray<FOBItemStack>& StackHaul,
	const TArray<FInventoryData>& LootedInstances,
	const TArray<FInventoryData>& ReturnedLoadoutInstances)
{
	LastExtractionHaul = StackHaul;
	for (const FInventoryData& Item : LootedInstances)
	{
		if (Item.ItemTag.IsValid() && Item.ItemStack > 0)
		{
			OBItemStacks::Add(LastExtractionHaul, Item.ItemTag, Item.ItemStack);
		}
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UOBLoadoutSubsystem* Loadout = GI->GetSubsystem<UOBLoadoutSubsystem>())
		{
			Loadout->ApplyExtractionResult(
				StackHaul,
				LootedInstances,
				ReturnedLoadoutInstances);
		}
	}
	UE_LOG(LogTemp, Log,
		TEXT("[Expedition] Extraction V2: stacks=%d instances=%d returned=%d"),
		StackHaul.Num(), LootedInstances.Num(), ReturnedLoadoutInstances.Num());
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

	// ClientTravel은 이 연결 하나만 세션에서 떼어내 로컬 맵을 연다.
	// OpenLevel은 리슨 서버에서 서버 트래블이 되어 접속자 전원을 끌고 간다.
	ClientTravel(HomeLevel.GetLongPackageName(), TRAVEL_Absolute, /*bSeamless=*/false);
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
	if (!IsLocalController() || !WidgetClass || ActiveInteractionWidget
		|| bHelicopterTransitLocked || IsInventoryInputBlocked()
		|| (PartyWidget && PartyWidget->IsInViewport()))
	{
		return nullptr;
	}
	CloseWorldMapForModal(TEXT("InteractionOpened"));

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
	if (AOBExpeditionGameMode* ExpeditionGameMode =
		GetWorld() ? GetWorld()->GetAuthGameMode<AOBExpeditionGameMode>() : nullptr)
	{
		ExpeditionGameMode->HandlePartyLeaderClaim(this, bLeader);
		return;
	}

	if (AOBPlayerStateBase* PS = GetPlayerState<AOBPlayerStateBase>())
	{
		PS->SetPartyLeader(bLeader);
		RefreshInsertionTransitSelectionPermission();
	}
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

void AOBPlayerController::Server_ApplyLoadoutInstances_Implementation(
	const TArray<FInventoryData>& Weapons)
{
	if (AOBPlayerStateBase* PS = GetPlayerState<AOBPlayerStateBase>())
	{
		PS->SetSelectedWeaponInstancesBulk(Weapons);
	}
}

void AOBPlayerController::Server_TakeLootInstance_Implementation(
	AOBLootContainer* Container,
	FGuid InstanceId,
	int32 Count)
{
	if (!Container || !InstanceId.IsValid() || Count <= 0) return;

	AOBCharacterBase* MyChar = Cast<AOBCharacterBase>(GetPawn());
	if (!MyChar || MyChar->IsDead()) return;
	if (FVector::DistSquared(
		MyChar->GetActorLocation(),
		Container->GetActorLocation()) > OBMaxInteractionDistanceSq)
	{
		return;
	}

	if (UPlayerInventoryComponent* Inv = MyChar->GetPlayerInventoryComponent())
	{
		Container->TryTakeItemInstance(Inv, InstanceId, Count);
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

void AOBPlayerController::Server_ApplyCarryLoadout_Implementation(
	const TArray<FOBItemStack>& StackItems,
	const TArray<FInventoryData>& ItemInstances)
{
	if (AOBPlayerStateBase* PS = GetPlayerState<AOBPlayerStateBase>())
	{
		PS->SetCarryLoadoutBulk(StackItems, ItemInstances);
	}
}

void AOBPlayerController::Client_ReturnCarryLeftover_Implementation(const TArray<FOBItemStack>& Items)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UOBLoadoutSubsystem* Loadout = GI->GetSubsystem<UOBLoadoutSubsystem>())
		{
			for (const FOBItemStack& Item : Items)
			{
				Loadout->RemoveCarryItem(Item.ItemTag, Item.Count);
			}
			UE_LOG(LogTemp, Warning,
				TEXT("[Carry] 가방이 모자라 %d종을 창고로 되돌렸다."), Items.Num());
		}
	}
}

void AOBPlayerController::Input_ToggleMap()
{
	HandleInsertionMapToggle(TEXT("EnhancedInput"));
}

void AOBPlayerController::OBInsertionDump()
{
	AOBHUD* OBHUD = GetHUD<AOBHUD>();
	UOBWorldMapWidget* MapWidget = OBHUD ? OBHUD->GetWorldMapWidget() : nullptr;
	AOBExpeditionGameState* GS = GetWorld() ? GetWorld()->GetGameState<AOBExpeditionGameState>() : nullptr;
	AOBPlayerStateBase* PS = GetPlayerState<AOBPlayerStateBase>();
	FOBTeamInsertionState TeamState;
	const bool bHasTeamState = GS && PS && GS->GetTeamInsertionState(PS->GetTeamId(), TeamState);

	UE_LOG(LogOBInsertionInput, Display,
		TEXT("[InsertionDump] PC=%s Local=%s Authority=%s Pawn=%s ViewTarget=%s TransitLocked=%s OwnerTransitPhase=%d OwnerRevision=%d OwnerHelicopter=%s Presentation=%s CanSelect=%s ")
		TEXT("SelectionAvailable=%s MappingAdded=%s HUD=%s MapWidget=%s MapOpen=%s ExpeditionPhase=%d Team=%d Leader=%s HasTeamState=%s ")
		TEXT("InsertionPhase=%d TeamPresentationRevision=%d Helicopter=%s Requested=%s Resolved=%s PassengerCount=%d DeployedCount=%d Deadline=%.2f"),
		*GetName(), IsLocalController() ? TEXT("true") : TEXT("false"), HasAuthority() ? TEXT("true") : TEXT("false"),
		*GetNameSafe(GetPawn()), *GetNameSafe(GetViewTarget()),
		bHelicopterTransitLocked ? TEXT("true") : TEXT("false"),
		static_cast<int32>(ReplicatedInsertionTransitState.Phase),
		ReplicatedInsertionTransitState.Revision,
		*GetNameSafe(ReplicatedInsertionTransitState.Helicopter),
		bInsertionPresentationActive ? TEXT("true") : TEXT("false"),
		bCanSelectInsertionTarget ? TEXT("true") : TEXT("false"),
		bInsertionTargetSelectionAvailable ? TEXT("true") : TEXT("false"),
		bInsertionMappingContextAdded ? TEXT("true") : TEXT("false"),
		*GetNameSafe(OBHUD), *GetNameSafe(MapWidget),
		MapWidget && MapWidget->IsMapOpen() ? TEXT("true") : TEXT("false"),
		GS ? static_cast<int32>(GS->GetPhase()) : -1,
		PS ? PS->GetTeamId() : 0,
		PS && PS->IsPartyLeader() ? TEXT("true") : TEXT("false"),
		bHasTeamState ? TEXT("true") : TEXT("false"),
		bHasTeamState ? static_cast<int32>(TeamState.Phase) : -1,
		bHasTeamState ? TeamState.PresentationRevision : 0,
		bHasTeamState ? *GetNameSafe(TeamState.Helicopter) : TEXT("None"),
		bHasTeamState && TeamState.bHasRequestedLocation ? TEXT("true") : TEXT("false"),
		bHasTeamState && TeamState.bHasResolvedLocation ? TEXT("true") : TEXT("false"),
		bHasTeamState ? TeamState.PassengerCount : 0,
		bHasTeamState ? TeamState.DeployedCount : 0,
		InsertionSelectionDeadlineServerTime);

	ShowInsertionDebugMessage(
		FString::Printf(
			TEXT("DUMP Local=%s Pawn=%s View=%s Locked=%s Presentation=%s Select=%s Map=%s Phase=%d"),
			IsLocalController() ? TEXT("true") : TEXT("false"),
			*GetNameSafe(GetPawn()), *GetNameSafe(GetViewTarget()),
			bHelicopterTransitLocked ? TEXT("true") : TEXT("false"),
			bInsertionPresentationActive ? TEXT("true") : TEXT("false"),
			bInsertionTargetSelectionAvailable ? TEXT("true") : TEXT("false"),
			MapWidget && MapWidget->IsMapOpen() ? TEXT("open") : TEXT("closed"),
			bHasTeamState ? static_cast<int32>(TeamState.Phase) : -1),
		FColor::Cyan, 12.f, 77104);
}

void AOBPlayerController::OBInsertionOpenMap()
{
	if (!IsLocalController())
	{
		return;
	}
	if (AOBHUD* OBHUD = GetHUD<AOBHUD>())
	{
		const bool bOpened = OBHUD->OpenInsertionMap(
			bCanSelectInsertionTarget && bInsertionTargetSelectionAvailable);
		UE_LOG(LogOBInsertionInput, Display,
			TEXT("[InsertionUI] OBInsertionOpenMap PC=%s Opened=%s CanSelect=%s"),
			*GetName(), bOpened ? TEXT("true") : TEXT("false"),
			bCanSelectInsertionTarget ? TEXT("true") : TEXT("false"));
	}
	else
	{
		UE_LOG(LogOBInsertionInput, Error,
			TEXT("[InsertionUI] OBInsertionOpenMap failed: HUD missing PC=%s"), *GetName());
	}
}

void AOBPlayerController::OBInsertionTrace()
{
	if (!IsLocalController())
	{
		return;
	}
	HandleInsertionTraceRequest(TEXT("Console"));
}

void AOBPlayerController::Client_ReturnCarryItemInstances_Implementation(const TArray<FInventoryData>& Items)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UOBLoadoutSubsystem* Loadout = GI->GetSubsystem<UOBLoadoutSubsystem>())
		{
			Loadout->ReturnCarryItemInstances(Items);
		}
	}
}
