// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/OBMainMenuWidget.h"

#include "OnlineSessionSettings.h"
#include "Session/Subsystems/OBSessionSubsystem.h"
#include "Components/Button.h"

void UOBMainMenuWidget::MenuSetup(int32 InNumPublicConnections, FString InMatchType)
{
	NumPublicConnections = InNumPublicConnections;
	MatchType = InMatchType;
	
	AddToViewport();
	SetVisibility(ESlateVisibility::Visible);
	bIsFocusable = true;
	
	// 입력을 UI 모드로
	if (APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController())
	{
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
		PC->SetShowMouseCursor(true);
	}
	
	// 세션 서브시스템 가져오기 + 델리게이트
	SessionSubsystem = GetGameInstance()->GetSubsystem<UOBSessionSubsystem>();
	if (SessionSubsystem)
	{
		SessionSubsystem->OnCreateSessionCompleteEvent.AddUObject(this, &UOBMainMenuWidget::HandleCreateSessionComplete);
		SessionSubsystem->OnFindSessionsCompleteEvent.AddUObject(this, &UOBMainMenuWidget::HandleFindSessionsComplete);
		SessionSubsystem->OnJoinSessionCompleteEvent.AddUObject(this, &UOBMainMenuWidget::HandleJoinSessionComplete);
	}
}

bool UOBMainMenuWidget::Initialize()
{
	if (!Super::Initialize()) return false;
	
	if (HostButton)
	{
		HostButton->OnClicked.AddDynamic(this, &UOBMainMenuWidget::OnHostClicked);
	}
	if (JoinButton)
	{
		JoinButton->OnClicked.AddDynamic(this, &UOBMainMenuWidget::OnJoinClicked);
	}
	
	return true;
}

void UOBMainMenuWidget::NativeDestruct()
{
	MenuTearDown();
	Super::NativeDestruct();
}

void UOBMainMenuWidget::OnHostClicked()
{
	if (HostButton)
	{
		HostButton->SetIsEnabled(false);
	}
	if (SessionSubsystem)
	{
		SessionSubsystem->CreateSession(NumPublicConnections, MatchType);
	}
}

void UOBMainMenuWidget::OnJoinClicked()
{
	if (JoinButton)
	{
		JoinButton->SetIsEnabled(false);
	}
	if (SessionSubsystem)
	{
		SessionSubsystem->FindSessions(20);
	}
}

void UOBMainMenuWidget::HandleCreateSessionComplete(bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		// 호스트: 리슨 서버로 맵 이동
		if (LobbyLevel.IsNull())
		{
			UE_LOG(LogTemp, Warning, TEXT("[MainMenu] LobbyLevel이 설정되지 않았습니다(Details에서 맵 지정 필요)."));
			if (HostButton)
			{
				HostButton->SetIsEnabled(true);
			}
			return;
		}
		
		const FString MapPath = LobbyLevel.ToSoftObjectPath().GetLongPackageName();
		UE_LOG(LogTemp, Log, TEXT("[MainMenu] ServerTravel → %s?listen"), *MapPath);
		if (UWorld* World = GetWorld())
		{
			World->ServerTravel(FString::Printf(TEXT("%s?listen"), *MapPath));
		}
	}
	else if (HostButton)
	{
		HostButton->SetIsEnabled(true); // 실패 시 재시도 가능
	}
}

void UOBMainMenuWidget::HandleFindSessionsComplete(const TArray<FOnlineSessionSearchResult>& Results, bool bWasSuccessful)
{
	UE_LOG(LogTemp, Warning, TEXT("[Menu] Find: success=%d, count=%d"), bWasSuccessful, Results.Num());
	if (!SessionSubsystem) return;
	
	if (bWasSuccessful)
	{
		for (const FOnlineSessionSearchResult& Result : Results)
		{
			FString FoundMatchType;
			Result.Session.SessionSettings.Get(FName("MatchType"), FoundMatchType);
			if (FoundMatchType == MatchType)
			{
				SessionSubsystem->JoinSession(Result);
				return;
			}
		}
	}
	
	// 찾은 세션 없음 -> 버튼 복구
	if (JoinButton)
	{
		JoinButton->SetIsEnabled(true);
	}
}

void UOBMainMenuWidget::HandleJoinSessionComplete(EOnJoinSessionCompleteResult::Type Result)
{
	UE_LOG(LogTemp, Warning, TEXT("[Menu] Join result=%d"), (int32)Result);
	// 성공 시 서브시스템이 자동 ClientTravel. 실패만 처리
	if (Result != EOnJoinSessionCompleteResult::Success && JoinButton)
	{
		JoinButton->SetIsEnabled(true);
	}
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
