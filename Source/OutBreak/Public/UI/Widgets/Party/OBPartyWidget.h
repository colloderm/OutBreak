// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OBPartyWidget.generated.h"

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
	

	UPROPERTY(EditAnywhere, Category = "Party") 
	TSubclassOf<UOBPartyMemberRowWidget> RowWidgetClass;

	UPROPERTY() 
	TObjectPtr<UOBPartySubsystem> Party;
	
	UPROPERTY() 
	TArray<TObjectPtr<UOBPartyMemberRowWidget>> Rows;
};
