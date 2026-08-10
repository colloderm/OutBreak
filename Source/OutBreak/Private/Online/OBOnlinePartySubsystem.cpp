// Fill out your copyright notice in the Description page of Project Settings.

#include "Online/OBOnlinePartySubsystem.h"

#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSubsystemNames.h"
#include "Interfaces/OnlinePresenceInterface.h"

namespace
{
	// 리더 → 멤버 출발 신호 키. 양쪽이 같은 이름을 써야 해서 상수로 둔다.
	const FName OB_TravelKey(TEXT("TRAVEL"));
}

static const FName PARTY_SESSION_NAME(TEXT("OBParty"));
static const int32 PARTY_MAX = 3;

// 소셜(친구/파티/세션)은 항상 Steam 서브시스템을 명시적으로 사용.
// (기본 서브시스템은 게임 전송용 NULL이므로 Online::GetSubsystem 쓰면 안 됨)
static IOnlineSubsystem* GetSocialOSS()
{
	return IOnlineSubsystem::Get(STEAM_SUBSYSTEM);
}
static IOnlineSessionPtr GetSocialSession()
{
	IOnlineSubsystem* OSS = GetSocialOSS();
	return OSS ? OSS->GetSessionInterface() : nullptr;
}

void UOBOnlinePartySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	if (IOnlineSubsystem* OSS = GetSocialOSS())
	{
		UE_LOG(LogTemp, Log, TEXT("[Online] Subsystem = %s"), *OSS->GetSubsystemName().ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Online] OnlineSubsystem 없음(Steam 미초기화?)"));
	}
	
	if (IOnlineSessionPtr Session = GetSocialSession())
	{
		InviteAcceptedHandle = Session->AddOnSessionUserInviteAcceptedDelegate_Handle(
			FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &UOBOnlinePartySubsystem::HandleInviteAccepted));
	}
}

FString UOBOnlinePartySubsystem::GetLocalPlayerName() const
{
	if (IOnlineSubsystem* OSS = GetSocialOSS())
	{
		if (IOnlineIdentityPtr Identity = OSS->GetIdentityInterface())
		{
			return Identity->GetPlayerNickname(0);
		}
	}
	
	return TEXT("Player");
}

void UOBOnlinePartySubsystem::ReadFriends()
{
	IOnlineSubsystem* OSS = GetSocialOSS();
	if (!OSS) return;
	
	IOnlineFriendsPtr Friends = OSS->GetFriendsInterface();
	if (!Friends.IsValid()) return;
	
	// Default 리스트 비동기 읽기
	Friends->ReadFriendsList(
		0, EFriendsLists::ToString(EFriendsLists::Default),
		FOnReadFriendsListComplete::CreateUObject(this, &UOBOnlinePartySubsystem::HandleReadFriendsComplete));
}

void UOBOnlinePartySubsystem::Deinitialize()
{
	if (IOnlineSessionPtr Session = GetSocialSession())
	{
		Session->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteAcceptedHandle);
	}
	
	Super::Deinitialize();
}

void UOBOnlinePartySubsystem::CreateParty()
{
	IOnlineSessionPtr Session = GetSocialSession();
	if (!Session.IsValid()) return;

	if (Session->GetNamedSession(PARTY_SESSION_NAME)) return; // 이미 있음

	TSharedRef<FOnlineSessionSettings> Settings = MakeShared<FOnlineSessionSettings>();
	Settings->NumPublicConnections = PARTY_MAX;
	Settings->NumPrivateConnections = 0;
	Settings->bIsLANMatch = false;
	Settings->bShouldAdvertise = true;
	Settings->bAllowJoinInProgress = true;
	Settings->bUsesPresence = true;
	Settings->bAllowJoinViaPresence = true;
	Settings->bAllowInvites = true;
	Settings->bUseLobbiesIfAvailable = true; // Steam 로비 사용(초대 안정)

	CreateHandle = Session->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UOBOnlinePartySubsystem::HandleCreateSessionComplete));

	Session->CreateSession(0, PARTY_SESSION_NAME, *Settings);
}

void UOBOnlinePartySubsystem::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (IOnlineSessionPtr Session = GetSocialSession())
		Session->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);

	if (bWasSuccessful)
	{
		bInParty = true;
		bIsLeader = true;
		UE_LOG(LogTemp, Log, TEXT("[Online] 파티 생성 완료"));
		OnPartyChanged.Broadcast();

		// 생성 대기 중이던 초대가 있으면 지금 전송.
		if (!PendingInviteUserId.IsEmpty())
		{
			const FString Id = PendingInviteUserId;
			PendingInviteUserId.Reset();
			InviteFriend(Id);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Online] 파티 생성 실패"));
	}
}

void UOBOnlinePartySubsystem::InviteFriend(const FString& UserId)
{
	IOnlineSessionPtr Session = GetSocialSession();
	if (!Session.IsValid()) return;

	// 파티 없으면 먼저 생성하고, 생성 완료 후 이 초대를 전송.
	if (!Session->GetNamedSession(PARTY_SESSION_NAME))
	{
		PendingInviteUserId = UserId;
		CreateParty();
		return;
	}

	IOnlineIdentityPtr Identity = GetSocialOSS()->GetIdentityInterface();
	TSharedPtr<const FUniqueNetId> FriendId = Identity.IsValid() ? Identity->CreateUniquePlayerId(UserId) : nullptr;
	if (!FriendId.IsValid()) return;

	Session->SendSessionInviteToFriend(0, PARTY_SESSION_NAME, *FriendId);
	UE_LOG(LogTemp, Log, TEXT("[Online] 초대 전송: %s"), *UserId);
}

void UOBOnlinePartySubsystem::HandleInviteAccepted(bool bWasSuccessful, int32 LocalUserNum,
	TSharedPtr<const FUniqueNetId> UserId, const FOnlineSessionSearchResult& InviteResult)
{
	if (!bWasSuccessful || !InviteResult.IsValid()) return;

	IOnlineSessionPtr Session = GetSocialSession();
	if (!Session.IsValid()) return;

	JoinHandle = Session->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UOBOnlinePartySubsystem::HandleJoinSessionComplete));

	Session->JoinSession(0, PARTY_SESSION_NAME, InviteResult);
}

void UOBOnlinePartySubsystem::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (IOnlineSessionPtr Session = GetSocialSession())
		Session->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);

	if (Result == EOnJoinSessionCompleteResult::Success)
	{
		bInParty = true;
		bIsLeader = false;
		StartFollowPoll(); // 멤버는 리더 출발 신호를 대기
		
		OnPartyChanged.Broadcast();
		
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Online] 파티 합류 실패(%d)"), (int32)Result);
	}
}

void UOBOnlinePartySubsystem::LeaveParty()
{
	IOnlineSessionPtr Session = GetSocialSession();
	if (!Session.IsValid() || !Session->GetNamedSession(PARTY_SESSION_NAME)) return;

	DestroyHandle = Session->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &UOBOnlinePartySubsystem::HandleDestroySessionComplete));

	Session->DestroySession(PARTY_SESSION_NAME);
}

void UOBOnlinePartySubsystem::HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (IOnlineSessionPtr Session = GetSocialSession())
		Session->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);

	bInParty = false;
	bIsLeader = false;
	UE_LOG(LogTemp, Log, TEXT("[Online] 파티 나감"));
	OnPartyChanged.Broadcast();
}

FString UOBOnlinePartySubsystem::GetPartyCode() const
{
	if (IOnlineSessionPtr Session = GetSocialSession())
	{
		if (FNamedOnlineSession* Named = Session->GetNamedSession(PARTY_SESSION_NAME))
		{
			return Named->GetSessionIdStr();
		}
	}
	
	return FString();
}

void UOBOnlinePartySubsystem::TravelToServer(const FString& ServerAddress)
{
	const FString Code = GetPartyCode();
	FString URL = ServerAddress;
	if (!Code.IsEmpty())
	{
		URL += FString::Printf(TEXT("?party=%s"), *Code);
	}
	
	UGameInstance* GI = GetGameInstance();
	if (GI)
	{
		if (APlayerController* PC = GI->GetFirstLocalPlayerController())
		{
			PC->ClientTravel(URL, TRAVEL_Absolute);
		}
	}
}

void UOBOnlinePartySubsystem::LeaderStartExpedition(const FString& ServerAddress)
{
	IOnlineSessionPtr Session = GetSocialSession();
	FNamedOnlineSession* Named = Session.IsValid() ? Session->GetNamedSession(PARTY_SESSION_NAME) : nullptr;
	
	// 파티가 있고 내가 리더면 -> 팀원에게 출발 신호(서버주소) 브로드캐스트.
	if (Named && bIsLeader)
	{
		FOnlineSessionSettings Updated = Named->SessionSettings;
		Updated.Set(OB_TravelKey, ServerAddress, EOnlineDataAdvertisementType::ViaOnlineService);
		Session->UpdateSession(PARTY_SESSION_NAME, Updated, true);

		UE_LOG(LogTemp, Log, TEXT("[Online] 출발 신호 브로드캐스트: %s"), *ServerAddress);
	}
	
	bTraveled = true;
	TravelToServer(ServerAddress); // 리더(또는 솔로) 이동
}

void UOBOnlinePartySubsystem::StartFollowPoll()
{
	bTraveled = false;
	if (UWorld* W = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		W->GetTimerManager().SetTimer(FollowTimer, this, &UOBOnlinePartySubsystem::PollLeaderStart, 1.f, true);
	}
}

void UOBOnlinePartySubsystem::PollLeaderStart()
{
	if (bTraveled || bIsLeader) return;
	
	IOnlineSessionPtr Session = GetSocialSession();
	FNamedOnlineSession* Named = Session.IsValid() ? Session->GetNamedSession(PARTY_SESSION_NAME) : nullptr;
	if (!Named) return;
	
	FString Addr;
	if (Named->SessionSettings.Get(OB_TravelKey, Addr) && !Addr.IsEmpty())
	{
		bTraveled = true;
		UE_LOG(LogTemp, Log, TEXT("[Online] 리더 출발 신호 수신: %s"), *Addr);
	}
}

int32 UOBOnlinePartySubsystem::GetPartyMemberCount() const
{
	if (IOnlineSessionPtr Session = GetSocialSession())
	{
		if (FNamedOnlineSession* Named = Session->GetNamedSession(PARTY_SESSION_NAME))
		{
			const int32 Max = Named->SessionSettings.NumPublicConnections;
			return FMath::Max(1, Max - Named->NumOpenPublicConnections);
		}
	}
	return bInParty ? 1 : 0;
}

void UOBOnlinePartySubsystem::HandleReadFriendsComplete(int32 LocalUserNum, bool bWasSuccessful, const FString& ListName, 
                                                        const FString& ErrorStr)
{
	CachedFriends.Reset();
	
	if (bWasSuccessful)
	{
		if (IOnlineSubsystem* OSS = GetSocialOSS())
		{
			IOnlineFriendsPtr Friends = OSS->GetFriendsInterface();
			if (Friends.IsValid())
			{
				TArray<TSharedRef<FOnlineFriend>> List;
				Friends->GetFriendsList(0, ListName, List);
				
				for (const TSharedRef<FOnlineFriend>& F : List)
				{
					FOBFriendInfo Info;
					Info.DisplayName = F->GetDisplayName();
					Info.UserId = F->GetUserId()->ToString();

					const FOnlineUserPresence& Pres = F->GetPresence();
					Info.bIsOnline = Pres.bIsOnline;
					Info.bIsPlayingThisGame = Pres.bIsPlayingThisGame;

					CachedFriends.Add(Info);
				}
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[Online] 친구 %d명 읽음"), CachedFriends.Num());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Online] ReadFriends 실패: %s"), *ErrorStr);
	}
	
	OnFriendsRead.Broadcast();
}
