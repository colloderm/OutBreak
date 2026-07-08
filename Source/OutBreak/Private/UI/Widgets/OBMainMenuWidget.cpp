// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/OBMainMenuWidget.h"

#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"

void UOBMainMenuWidget::MenuSetup(int32 InNumPublicConnections, FString InMatchType)
{	
	AddToViewport();
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);
	
	// 입력을 UI 모드로
	if (APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController())
	{
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
		PC->SetShowMouseCursor(true);
	}
}

bool UOBMainMenuWidget::Initialize()
{
	if (!Super::Initialize()) return false;
	
	if (StartButton)
	{
		StartButton->OnClicked.AddDynamic(this, &UOBMainMenuWidget::OnStartClicked);
	}
	
	return true;
}

void UOBMainMenuWidget::NativeDestruct()
{
	MenuTearDown();
	Super::NativeDestruct();
}

void UOBMainMenuWidget::OnStartClicked()
{
	if (HomeLevel.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MainMenu] HomeLevel 미지정."));
		return;
	}
	
	// 개인 Home으로 로컬 이동(세션/리슨 아님 — 개인 레벨).
	MenuTearDown();  // 입력모드 GameOnly 복귀 + 위젯 제거
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, HomeLevel);
}

void UOBMainMenuWidget::MenuTearDown()
{
	RemoveFromParent();
	if (APlayerController* PC =GetGameInstance()->GetFirstLocalPlayerController())
	{
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
		PC->SetShowMouseCursor(false);
	}
}
