// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OBFriendListWidget.generated.h"

class UPanelWidget;
class UButton;
class UOBFriendRowWidget;
class UOBOnlinePartySubsystem;

UCLASS()
class OUTBREAK_API UOBFriendListWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// "친구" 버튼 등에서 호출: 목록 비동기 요청.
	UFUNCTION(BlueprintCallable, Category = "Online")
	void RefreshFriends();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UOBOnlinePartySubsystem* GetOnline() const;

	UFUNCTION()
	void HandleFriendsRead();                 // OnFriendsRead(dynamic) 바인딩

	void HandleInvite(const FString& InUserId);  // 행의 초대 클릭
	void RebuildList();

protected:
	UPROPERTY(meta = (BindWidget))         
	TObjectPtr<UPanelWidget> FriendsBox;    // ScrollBox/VerticalBox
	UPROPERTY(meta = (BindWidgetOptional)) 
	TObjectPtr<UButton> RefreshButton; // 선택

	UPROPERTY(EditAnywhere, Category = "Online")
	TSubclassOf<UOBFriendRowWidget> RowClass;

	bool bBound = false;
};
