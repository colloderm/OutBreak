// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Widgets/Party/OBPartyMemberRowWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"

void UOBPartyMemberRowWidget::Setup(const FOBPartyMember& Member)
{
	if (NameText)  
		NameText->SetText(Member.DisplayName);
	
	if (LeaderIcon) 
		LeaderIcon->SetVisibility(Member.bIsLeader ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	
	if (StatusText) 
		StatusText->SetText(FText::FromString(Member.bOnline ? TEXT("온라인") : TEXT("오프라인")));
}
