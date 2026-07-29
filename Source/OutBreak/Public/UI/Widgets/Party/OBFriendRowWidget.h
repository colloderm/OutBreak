// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Online/OBOnlinePartySubsystem.h"
#include "OBFriendRowWidget.generated.h"

class UButton;
class UTextBlock;

// 초대 클릭 -> Userid 전달
DECLARE_MULTICAST_DELEGATE_OneParam(FOBOnFriendInviteClicked, const FString& /*UserId*/) 

UCLASS()
class OUTBREAK_API UOBFriendRowWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void Setup(const FOBFriendInfo& InFriend);
	FOBOnFriendInviteClicked OnInviteClicked;
	
protected:
	virtual void NativeConstruct() override;
	
	UFUNCTION()
	void HandleInviteClicked();
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;
	
	UPROPERTY(meta = (BindWidget)) 
	TObjectPtr<UTextBlock> StatusText;
	
	UPROPERTY(meta = (BindWidget)) 
	TObjectPtr<UButton> InviteButton;
	
	FString UserId; // 이 행 친구의 net id(초대 대상)
};
