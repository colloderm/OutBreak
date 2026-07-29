// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/Expedition/OBSpectatorWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Player/Controller/OBPlayerController.h"
#include "TimerManager.h"

void UOBSpectatorWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (BTN_Prev)
		BTN_Prev->OnClicked.AddDynamic(this, &UOBSpectatorWidget::HandlePrevClicked);
	if (BTN_Next)
		BTN_Next->OnClicked.AddDynamic(this, &UOBSpectatorWidget::HandleNextClicked);
	if (BTN_Leave)
		BTN_Leave->OnClicked.AddDynamic(this, &UOBSpectatorWidget::HandleLeaveClicked);
	
	RefreshTargetLabel();
	
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().SetTimer(
			LabelRefreshTimer, this, &UOBSpectatorWidget::RefreshTargetLabel, 0.25f, /*bLoop=*/true);
	}
}

void UOBSpectatorWidget::NativeDestruct()
{
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(LabelRefreshTimer);
	}
	
	Super::NativeDestruct();
}

void UOBSpectatorWidget::HandlePrevClicked()
{
	if (AOBPlayerController* PC = GetOwningPlayer<AOBPlayerController>())
	{
		PC->SpectatePrev();
	}
}

void UOBSpectatorWidget::HandleNextClicked()
{
	if (AOBPlayerController* PC = GetOwningPlayer<AOBPlayerController>())
	{
		PC->SpectateNext();
	}
}

void UOBSpectatorWidget::HandleLeaveClicked()
{
	if (AOBPlayerController* PC = GetOwningPlayer<AOBPlayerController>())
	{
		PC->ReturnToHome();
	}
}

void UOBSpectatorWidget::RefreshTargetLabel()
{
	if (!TXT_Target) return;
	
	const AOBPlayerController* PC = GetOwningPlayer<AOBPlayerController>();
	const FString Name = PC ? PC->GetSpectateTargetName() : FString();
	
	TXT_Target->SetText(Name.IsEmpty()
		? FText::FromString(TEXT("관련 대상 없음"))
		: FText::FromString(FString::Printf(TEXT("관전 중: %s"), *Name)));
}
