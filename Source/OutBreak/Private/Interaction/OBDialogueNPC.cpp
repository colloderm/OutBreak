// Fill out your copyright notice in the Description page of Project Settings.

#include "Interaction/OBDialogueNPC.h"

#include "Player/Controller/OBPlayerController.h"
#include "Dialogue/OBDialogueWidget.h"

void AOBDialogueNPC::Interact_Implementation(AOBPlayerController* PC)
{	
	if (!PC || !InteractWidgetClass) return;
	
	UUserWidget* Widget = PC->OpenInteractionWidget(InteractWidgetClass);
	ActiveDialogue = Cast<UOBDialogueWidget>(Widget);
	if (!ActiveDialogue) return;
	
	// 중복 바인딩 방지 후 액션 수신 연결
	ActiveDialogue->OnDialogueAction.RemoveAll(this);
	ActiveDialogue->OnDialogueAction.AddDynamic(this, &AOBDialogueNPC::OnDialogueActionReceived);
	
	ActiveDialogue->StartDialogue(DialogueTable, StartNode);
}
