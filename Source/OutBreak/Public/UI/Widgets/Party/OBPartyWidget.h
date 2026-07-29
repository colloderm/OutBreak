// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OBPartyWidget.generated.h"

class UOBFriendListWidget;
class UWidgetSwitcher;
class UButton; 
class UPanelWidget; 
class UTextBlock;
class UOBPartySubsystem; 
class UOBPartyMemberRowWidget;

UCLASS()
class OUTBREAK_API UOBPartyWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	void Rebuild();

	UFUNCTION() 
	void HandleLeaveClicked();
	
	UFUNCTION() 
	void HandleDebugAdd();     // TEMP
	
	UFUNCTION() 
	void HandleDebugToggle();  // TEMP
	
	UFUNCTION() 
	void OnFriendTabClicked();
	
	UFUNCTION() 
	void OnRequestTabClicked();
	
protected:
	UPROPERTY(meta = (BindWidget))         
	TObjectPtr<UPanelWidget> MembersBox;
	
	UPROPERTY(meta = (BindWidgetOptional)) 
	TObjectPtr<UButton>      LeaveButton;   // "팀 나가기"
	
	UPROPERTY(meta = (BindWidgetOptional)) 
	TObjectPtr<UTextBlock>   TitleText;     // "팀 (n/n)"
	

	// TEMP: 실 초대/참가(M7) 전 테스트용.
	UPROPERTY(meta = (BindWidgetOptional)) 
	TObjectPtr<UButton> BtnDebugAdd;
	
	UPROPERTY(meta = (BindWidgetOptional)) 
	TObjectPtr<UButton> BtnDebugToggle;

	
	UPROPERTY(meta = (BindWidgetOptional)) 
	TObjectPtr<UButton> BtnFriend;
	
	UPROPERTY(meta = (BindWidgetOptional)) 
	TObjectPtr<UButton> BtnRequest;
	
	UPROPERTY(meta = (BindWidgetOptional)) 
	TObjectPtr<UWidgetSwitcher> ContentSwitcher;
	
	UPROPERTY(meta = (BindWidgetOptional)) 
	TObjectPtr<UWidget> FriendPanel;
	
	UPROPERTY(meta = (BindWidgetOptional)) 
	TObjectPtr<UWidget> RequestPanel;
	
	// FriendPanel 안에 임베드한 WBP_FriendList 인스턴스(이름 "FriendList").
	UPROPERTY(meta = (BindWidgetOptional)) 
	TObjectPtr<UOBFriendListWidget> FriendList;
	

	UPROPERTY(EditAnywhere, Category = "Party") 
	TSubclassOf<UOBPartyMemberRowWidget> RowWidgetClass;

	UPROPERTY() 
	TObjectPtr<UOBPartySubsystem> Party;
	
	UPROPERTY() 
	TArray<TObjectPtr<UOBPartyMemberRowWidget>> Rows;
};
