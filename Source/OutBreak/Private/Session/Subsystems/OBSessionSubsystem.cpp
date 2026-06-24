// Fill out your copyright notice in the Description page of Project Settings.

#include "Session/Subsystems/OBSessionSubsystem.h"

#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "GameFramework/PlayerController.h"

void UOBSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld()))
	{
		SessionInterface = Subsystem->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			// 친구 초대 수락 구독(게임 인스턴스 수명 동안 유지).
			InviteAcceptedHandle = SessionInterface->AddOnSessionUserInviteAcceptedDelegate_Handle(
				FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &UOBSessionSubsystem::HandleSessionUserInviteAccepted));
		}
	}
}

void UOBSessionSubsystem::Deinitialize()
{
	if (SessionInterface.IsValid() && InviteAcceptedHandle.IsValid())
	{
		SessionInterface->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteAcceptedHandle);
	}
	Super::Deinitialize();
}

void UOBSessionSubsystem::CreateSession(int32 NumPublicConnections, const FString& MatchType)
{
	if (!SessionInterface.IsValid())
	{
		OnCreateSessionCompleteEvent.Broadcast(false);
		return;
	}

	// 기존 세션 정리.
	if (SessionInterface->GetNamedSession(NAME_GameSession))
	{
		SessionInterface->DestroySession(NAME_GameSession);
	}

	CreateCompleteHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UOBSessionSubsystem::HandleCreateSessionComplete));

	LastSessionSettings = MakeShareable(new FOnlineSessionSettings());
	LastSessionSettings->bIsLANMatch = (Online::GetSubsystem(GetWorld())->GetSubsystemName() == NULL_SUBSYSTEM); // 스팀 없으면 LAN
	LastSessionSettings->NumPublicConnections = NumPublicConnections;
	LastSessionSettings->bAllowJoinInProgress = true;
	LastSessionSettings->bAllowJoinViaPresence = true;   // 친구 초대 핵심
	LastSessionSettings->bShouldAdvertise = true;
	LastSessionSettings->bUsesPresence = true;           // 친구 초대 핵심
	LastSessionSettings->bUseLobbiesIfAvailable = true;  // 스팀 로비 사용
	LastSessionSettings->Set(FName("MatchType"), MatchType, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	LastSessionSettings->BuildUniqueId = 1;

	const ULocalPlayer* LocalPlayer = GetGameInstance()->GetFirstGamePlayer();
	if (!SessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, *LastSessionSettings))
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateCompleteHandle);
		OnCreateSessionCompleteEvent.Broadcast(false);
	}
}

void UOBSessionSubsystem::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateCompleteHandle);
	}
	OnCreateSessionCompleteEvent.Broadcast(bWasSuccessful);
	// 성공 시 호스트가 맵으로 ServerTravel(호출부에서). 예: GetWorld()->ServerTravel("/Game/Maps/L_TestMap?listen");
}

void UOBSessionSubsystem::FindSessions(int32 MaxSearchResults)
{
	if (!SessionInterface.IsValid())
	{
		OnFindSessionsCompleteEvent.Broadcast({}, false);
		return;
	}

	FindCompleteHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UOBSessionSubsystem::HandleFindSessionsComplete));

	LastSessionSearch = MakeShareable(new FOnlineSessionSearch());
	LastSessionSearch->MaxSearchResults = MaxSearchResults;
	LastSessionSearch->bIsLanQuery = (Online::GetSubsystem(GetWorld())->GetSubsystemName() == NULL_SUBSYSTEM);
	LastSessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);

	const ULocalPlayer* LocalPlayer = GetGameInstance()->GetFirstGamePlayer();
	if (!SessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), LastSessionSearch.ToSharedRef()))
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindCompleteHandle);
		OnFindSessionsCompleteEvent.Broadcast({}, false);
	}
}

void UOBSessionSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindCompleteHandle);
	}
	const TArray<FOnlineSessionSearchResult> Results = LastSessionSearch.IsValid() ? LastSessionSearch->SearchResults : TArray<FOnlineSessionSearchResult>();
	OnFindSessionsCompleteEvent.Broadcast(Results, bWasSuccessful && Results.Num() > 0);
}

void UOBSessionSubsystem::JoinSession(const FOnlineSessionSearchResult& SessionResult)
{
	if (!SessionInterface.IsValid())
	{
		OnJoinSessionCompleteEvent.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
		return;
	}

	JoinCompleteHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UOBSessionSubsystem::HandleJoinSessionComplete));

	const ULocalPlayer* LocalPlayer = GetGameInstance()->GetFirstGamePlayer();
	if (!SessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, SessionResult))
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinCompleteHandle);
		OnJoinSessionCompleteEvent.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
	}
}

void UOBSessionSubsystem::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinCompleteHandle);
	}

	OnJoinSessionCompleteEvent.Broadcast(Result);

	// 성공 시 클라이언트가 호스트로 자동 이동(친구 초대도 이 경로).
	if (Result == EOnJoinSessionCompleteResult::Success)
	{
		FString ConnectString;
		if (SessionInterface->GetResolvedConnectString(NAME_GameSession, ConnectString))
		{
			if (APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController())
			{
				PC->ClientTravel(ConnectString, ETravelType::TRAVEL_Absolute);
			}
		}
	}
}

void UOBSessionSubsystem::DestroySession()
{
	if (!SessionInterface.IsValid())
	{
		return;
	}
	DestroyCompleteHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &UOBSessionSubsystem::HandleDestroySessionComplete));
	SessionInterface->DestroySession(NAME_GameSession);
}

void UOBSessionSubsystem::HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroyCompleteHandle);
	}
}

void UOBSessionSubsystem::HandleSessionUserInviteAccepted(bool bWasSuccessful, int32 ControllerId,
	FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult)
{
	// 스팀 오버레이에서 친구가 "참가" → 곧바로 해당 세션 참가.
	if (bWasSuccessful && InviteResult.IsValid())
	{
		JoinSession(InviteResult);
	}
}