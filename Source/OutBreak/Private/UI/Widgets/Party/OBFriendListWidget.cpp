// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/Party/OBFriendListWidget.h"

#include "UI/Widgets/Party/OBFriendRowWidget.h"
#include "Online/OBOnlinePartySubsystem.h"
#include "Components/PanelWidget.h"
#include "Components/Button.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

void UOBFriendListWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (UOBOnlinePartySubsystem* Online = GetOnline())
	{
		if (!bBound)
		{
			Online->OnFriendsRead.AddDynamic(this, &UOBFriendListWidget::HandleFriendsRead);
			bBound = true;
		}
	}

	if (RefreshButton)
	{
		RefreshButton->OnClicked.AddDynamic(this, &UOBFriendListWidget::RefreshFriends);
	}

	RefreshFriends(); // 열릴 때 자동 1회 요청
}

void UOBFriendListWidget::NativeDestruct()
{
	if (UOBOnlinePartySubsystem* Online = GetOnline())
	{
		Online->OnFriendsRead.RemoveAll(this);
	}
	bBound = false;
	
	Super::NativeDestruct();
}

UOBOnlinePartySubsystem* UOBFriendListWidget::GetOnline() const
{
	if (UWorld* W = GetWorld())
	{
		if (UGameInstance* GI = W->GetGameInstance())
		{
			return GI->GetSubsystem<UOBOnlinePartySubsystem>();
		}
	}
	
	return nullptr;
}

void UOBFriendListWidget::RefreshFriends()
{
	if (UOBOnlinePartySubsystem* Online = GetOnline())
	{
		Online->ReadFriends(); // 완료 시 HandleFriendRead
	}
}

void UOBFriendListWidget::HandleFriendsRead()
{
	RebuildList();
}

void UOBFriendListWidget::RebuildList()
{
	if (!FriendsBox || !RowClass) return;
	FriendsBox->ClearChildren();

	UOBOnlinePartySubsystem* Online = GetOnline();
	if (!Online) return;

	for (const FOBFriendInfo& F : Online->GetFriends())
	{
		UOBFriendRowWidget* Row = CreateWidget<UOBFriendRowWidget>(this, RowClass);
		if (!Row) continue;

		Row->OnInviteClicked.AddUObject(this, &UOBFriendListWidget::HandleInvite);
		Row->Setup(F);
		FriendsBox->AddChild(Row);
	}
}

void UOBFriendListWidget::HandleInvite(const FString& InUserId)
{
	if (UOBOnlinePartySubsystem* Online = GetOnline())
	{
		Online->InviteFriend(InUserId); // 파티 없으면 자동 생성 후 Steam 오버레이 초대
	}
}
