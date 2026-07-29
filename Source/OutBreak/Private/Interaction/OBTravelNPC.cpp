// Fill out your copyright notice in the Description page of Project Settings.

#include "Interaction/OBTravelNPC.h"


void AOBTravelNPC::Interact_Implementation(AOBPlayerController* PC)
{
	// 부모가 InteractWidgetClass(=WBP_ExpeditionSelect)를 오픈. 위젯이 자기 초기화 완결.
	Super::Interact_Implementation(PC);
}
