// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OBMainMenuWidget.generated.h"

class UButton;
class UOBSessionSubsystem;

UCLASS()
class OUTBREAK_API UOBMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// 메뉴 표시 + 세션 서브시스템 연결(레벨 BP에서 호출).
	UFUNCTION(BlueprintCallable, Category = "OB|Menu")
	void MenuSetup(int32 InNumPublicConnections = 4, FString InMatchType = TEXT("Coop"));

protected:
	virtual bool Initialize() override;
	virtual void NativeDestruct() override;

	// 버튼 콜백.
	UFUNCTION() void OnHostClicked();
	UFUNCTION() void OnJoinClicked();

	// 세션 서브시스템 결과 콜백.
	void HandleCreateSessionComplete(bool bWasSuccessful);
	void HandleFindSessionsComplete(const TArray<FOnlineSessionSearchResult>& Results, bool bWasSuccessful);
	void HandleJoinSessionComplete(EOnJoinSessionCompleteResult::Type Result);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OB|Menu")
	TSoftObjectPtr<UWorld> LobbyLevel;
	
	// WBP의 버튼(이름 일치 필요).
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> HostButton;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> JoinButton;

private:
	void MenuTearDown();

	UPROPERTY()
	TObjectPtr<UOBSessionSubsystem> SessionSubsystem;

	int32 NumPublicConnections = 4;
	FString MatchType = TEXT("Coop");
	FString LobbyMap;
};
