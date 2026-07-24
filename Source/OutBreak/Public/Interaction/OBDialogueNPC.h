// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OBInteractableActor.h"
#include "Dialogue/OBDialogueTypes.h"
#include "OBDialogueNPC.generated.h"

class UOBDialogueWidget;
class UDataTable;

// 다이얼로그를 여는 NPC 베이스. 자식이 HandleAction만 오버라이드한다.
UCLASS(Abstract)
class OUTBREAK_API AOBDialogueNPC : public AOBInteractableActor
{
	GENERATED_BODY()

public:
	virtual void Interact_Implementation(AOBPlayerController* PC) override;

protected:
	// 선택지 액션 처리. 자식이 오버라이드. 위젯 참조는 ActiveDialogue로.
	virtual void HandleAction(EOBDialogueAction Action) {}

	// OnDialogueAction 델리게이트 바인딩 대상(UFUNCTION 필수) → 가상 HandleAction 호출.
	UFUNCTION()
	void OnDialogueActionReceived(EOBDialogueAction Action) { HandleAction(Action); }

protected:
	// 이 NPC의 대사 테이블.
	UPROPERTY(EditAnywhere, Category = "Dialogue")
	TObjectPtr<UDataTable> DialogueTable;

	UPROPERTY(EditAnywhere, Category = "Dialogue")
	FName StartNode = TEXT("Root");

	// 현재 열린 다이얼로그(GoToNode 호출용).
	UPROPERTY(Transient)
	TObjectPtr<UOBDialogueWidget> ActiveDialogue;
	
};
