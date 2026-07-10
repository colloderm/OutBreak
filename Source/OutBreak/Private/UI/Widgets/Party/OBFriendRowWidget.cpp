// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Widgets/Party/OBFriendRowWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

void UOBFriendRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (InviteButton)
	{
		InviteButton->OnClicked.AddDynamic(this, &UOBFriendRowWidget::HandleInviteClicked);
	}
}

void UOBFriendRowWidget::Setup(const FOBFriendInfo& InFriend)
{
	UserId = InFriend.UserId;
	if (NameText)
		NameText->SetText(FText::FromString(InFriend.DisplayName));
	
	FString Str;
	FLinearColor Color;
	if (InFriend.bIsPlayingThisGame)
	{
		Str = TEXT("게임 중");
		Color = FLinearColor::Green;
	}
	else if (InFriend.bIsOnline)
	{
		Str = TEXT("온라인");
		Color = FLinearColor::White;
	}
	else
	{
		Str = TEXT("오프라인");
		Color = FLinearColor::Gray;
	}
	
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Str));
		StatusText->SetColorAndOpacity(FSlateColor(Color));
	}
	
	// 온라인 친구만 초대 가능
	if (InviteButton)
		InviteButton->SetIsEnabled(InFriend.bIsOnline);
}

void UOBFriendRowWidget::HandleInviteClicked()
{
	OnInviteClicked.Broadcast(UserId); // 리스트 위젯이 받아 초대
}
