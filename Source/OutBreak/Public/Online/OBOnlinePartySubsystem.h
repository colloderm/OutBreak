// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OBOnlinePartySubsystem.generated.h"

//UI 표시용 친구 1명 정보
USTRUCT(BlueprintType)
struct FOBFriendInfo
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly, Category = "Online")
	FString DisplayName;
	
	UPROPERTY(BlueprintReadOnly, Category = "Online")
	FString UserId;
	
	UPROPERTY(BlueprintReadOnly, Category = "Online")
	bool bIsOnline = false; // net id 문자열(초대 대상 식별용)
	
	UPROPERTY(BlueprintReadOnly, Category = "Online")
	bool bIsPlayingThisGame = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOBOnFriendsRead);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOBOnPartyChanged);

/**
 * Steam 온라인 소셜/파티 허브(GameInstance 수명).
 * - M8-1: 친구 목록 읽기/표시.
 * - M8-2(예정): 파티 세션 생성/초대/수락.
 * - M8-3(예정): 파티장 시작 → 전원 ?party=<코드> 트래블.
 */
UCLASS()
class OUTBREAK_API UOBOnlinePartySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// 로컬 플레이어 표시명(Steam 닉네임).
	UFUNCTION(BlueprintPure, Category = "Online")
	FString GetLocalPlayerName() const;

	// 친구 목록 비동기 읽기 → 완료 시 OnFriendsRead 브로드캐스트.
	UFUNCTION(BlueprintCallable, Category = "Online")
	void ReadFriends();

	// 마지막으로 읽은 친구 목록(UI 바인딩).
	UFUNCTION(BlueprintPure, Category = "Online")
	const TArray<FOBFriendInfo>& GetFriends() const { return CachedFriends; }
	
	virtual void Deinitialize() override;

	// 파티(로비) 생성 — 리더.
	UFUNCTION(BlueprintCallable, Category = "Online") 
	void CreateParty();
	
	// 친구 초대(Steam 오버레이). 파티 없으면 자동 생성 후 전송.
	UFUNCTION(BlueprintCallable, Category = "Online") 
	void InviteFriend(const FString& UserId);
	
	// 파티 나가기(세션 파괴).
	UFUNCTION(BlueprintCallable, Category = "Online") 
	void LeaveParty();
	

	UFUNCTION(BlueprintPure, Category = "Online") 
	bool IsInParty() const { return bInParty; }
	
	UFUNCTION(BlueprintPure, Category = "Online") 
	bool IsPartyLeader() const { return bIsLeader; }
	
	UFUNCTION(BlueprintPure, Category = "Online") 
	int32 GetPartyMemberCount() const;
	
public:
	// 친구 읽기 완료 알림(UI가 목록 갱신).
	UPROPERTY(BlueprintAssignable, Category = "Online")
	FOBOnFriendsRead OnFriendsRead;
	
	// 파티 상태/멤버 변경 → UI 갱신.
	UPROPERTY(BlueprintAssignable, Category = "Online")
	FOBOnPartyChanged OnPartyChanged;

private:
	// IOnlineFriends::ReadFriendsList 완료 콜백.
	void HandleReadFriendsComplete(int32 LocalUserNum, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr);
	
	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	
	void HandleInviteAccepted(bool bWasSuccessful, int32 LocalUserNum, TSharedPtr<const FUniqueNetId> UserId,
		
		const FOnlineSessionSearchResult& InviteResult);
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);

private:
	UPROPERTY()
	TArray<FOBFriendInfo> CachedFriends;

	bool bInParty = false;
	bool bIsLeader = false;
	FString PendingInviteUserId; // 파티 없을 때 초대 → 생성 완료 후 전송

	FDelegateHandle CreateHandle, JoinHandle, DestroyHandle, InviteAcceptedHandle;
};
