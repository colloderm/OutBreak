// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OBSessionSubsystem.generated.h"

// UI 구독용 델리게이트.
DECLARE_MULTICAST_DELEGATE_OneParam(FOBOnCreateSessionComplete, bool /*bSuccess*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOBOnFindSessionsComplete, const TArray<FOnlineSessionSearchResult>& /*Results*/, bool /*bSuccess*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOBOnJoinSessionComplete, EOnJoinSessionCompleteResult::Type /*Result*/);

UCLASS()
class OUTBREAK_API UOBSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
	public:
	//~ USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 세션 작업(UI가 호출).
	void CreateSession(int32 NumPublicConnections, const FString& MatchType);
	void FindSessions(int32 MaxSearchResults);
	void JoinSession(const FOnlineSessionSearchResult& SessionResult);
	void DestroySession();

	// 결과 통지.
	FOBOnCreateSessionComplete OnCreateSessionCompleteEvent;
	FOBOnFindSessionsComplete OnFindSessionsCompleteEvent;
	FOBOnJoinSessionComplete OnJoinSessionCompleteEvent;

protected:
	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);

	// 스팀 오버레이에서 친구가 "참가" 시.
	void HandleSessionUserInviteAccepted(bool bWasSuccessful, int32 ControllerId,
		FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);

private:
	IOnlineSessionPtr SessionInterface;
	TSharedPtr<FOnlineSessionSettings> LastSessionSettings;
	TSharedPtr<FOnlineSessionSearch> LastSessionSearch;

	FDelegateHandle CreateCompleteHandle;
	FDelegateHandle FindCompleteHandle;
	FDelegateHandle JoinCompleteHandle;
	FDelegateHandle DestroyCompleteHandle;
	FDelegateHandle InviteAcceptedHandle;
};
