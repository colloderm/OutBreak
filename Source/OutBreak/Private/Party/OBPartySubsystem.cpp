// Fill out your copyright notice in the Description page of Project Settings.

#include "Party/OBPartySubsystem.h"

#include "Player/Controller/OBPlayerController.h"
#include "Engine/GameInstance.h"

void UOBPartySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LocalPlayerId = TEXT("LocalPlayer");
	RebuildSoloDefault();
}

bool UOBPartySubsystem::IsLocalLeader() const
{
	for (const FOBPartyMember& M : Members)
		if (M.PlayerId == LocalPlayerId) return M.bIsLeader;
	return true; // 목록에 없으면 솔로.
}

void UOBPartySubsystem::LeaveParty()
{
	RebuildSoloDefault();
}

void UOBPartySubsystem::RebuildSoloDefault()
{
	Members.Reset();
	FOBPartyMember Me;
	Me.PlayerId = LocalPlayerId;
	Me.DisplayName = FText::FromString(TEXT("나"));
	Me.bIsLeader = true;   // 솔로 = 자기 리더
	Members.Add(Me);
	NotifyChanged();
}

void UOBPartySubsystem::DebugAddDummyMember(FText Name)
{
	if (Members.Num() >= MaxPartySize)
	{
		UE_LOG(LogTemp, Log, TEXT("[Party] 정원(%d) 초과 ㅡ 팀원 추가 무시"), MaxPartySize);
		return;
	}
	
	FOBPartyMember M;
	M.PlayerId = FString::Printf(TEXT("Dummy_%d"), Members.Num());
	M.DisplayName = Name.IsEmpty() ? FText::FromString(TEXT("팀원")) : Name;
	M.bIsLeader = false;
	Members.Add(M);
	NotifyChanged();
}

void UOBPartySubsystem::DebugSetLocalLeader(bool bLeader)
{
	bool bFoundLocal = false;
	for (FOBPartyMember& M : Members)
	{
		if (M.PlayerId == LocalPlayerId)
		{
			M.bIsLeader = bLeader; 
			bFoundLocal = true;
		}
		else
		{
			M.bIsLeader = false;
		}
	}
	
	if (!bFoundLocal) return;

	if (!bLeader) // 로컬이 멤버면 누군가는 리더여야 함.
	{
		bool bHasOther = false;
		for (FOBPartyMember& M : Members)
		{
			if (M.PlayerId != LocalPlayerId)
			{
				M.bIsLeader = true; 
				bHasOther = true; 
				break;
			}
		}
		
		if (!bHasOther)
		{
			FOBPartyMember Leader;
			Leader.PlayerId = TEXT("Dummy_Leader");
			Leader.DisplayName = FText::FromString(TEXT("팀장"));
			Leader.bIsLeader = true;
			Members.Insert(Leader, 0);
		}
	}
	
	NotifyChanged();
}

void UOBPartySubsystem::NotifyChanged()
{
	OnPartyChanged.Broadcast();
	PushLeadershipToPlayerState();
}

void UOBPartySubsystem::PushLeadershipToPlayerState()
{
	if (UGameInstance* GI = GetGameInstance())
		if (AOBPlayerController* PC = Cast<AOBPlayerController>(GI->GetFirstLocalPlayerController()))
			PC->Server_SetPartyLeader(IsLocalLeader());
}