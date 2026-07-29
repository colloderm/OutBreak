// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OBMainMenuWidget.generated.h"

class UButton;

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

	UFUNCTION()
	void OnStartClicked();

	// 개인 Home 레벨(에디터에서 L_Home 지정).
	UPROPERTY(EditAnywhere, Category = "Flow")
	TSoftObjectPtr<UWorld> HomeLevel;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> StartButton;

private:
	void MenuTearDown();
	
};
