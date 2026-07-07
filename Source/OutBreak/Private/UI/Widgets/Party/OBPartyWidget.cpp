// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/Party/OBPartyWidget.h"

#include "UI/Widgets/Party/OBPartyMemberRowWidget.h"
#include "Party/OBPartySubsystem.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"

void UOBPartyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (LeaveButton)    
		LeaveButton->OnClicked.AddDynamic(this, &UOBPartyWidget::HandleLeaveClicked);
	
	if (BtnDebugAdd)    
		BtnDebugAdd->OnClicked.AddDynamic(this, &UOBPartyWidget::HandleDebugAdd);
	
	if (BtnDebugToggle) 
		BtnDebugToggle->OnClicked.AddDynamic(this, &UOBPartyWidget::HandleDebugToggle);

	if (UGameInstance* GI = GetGameInstance())
		Party = GI->GetSubsystem<UOBPartySubsystem>();
	
	if (Party)
		Party->OnPartyChanged.AddUObject(this, &UOBPartyWidget::Rebuild);

	Rebuild();
}

void UOBPartyWidget::NativeDestruct()
{
	if (Party) 
		Party->OnPartyChanged.RemoveAll(this);
	
	Super::NativeDestruct();
}

void UOBPartyWidget::Rebuild()
{
	if (!MembersBox || !RowWidgetClass || !Party) return;

	MembersBox->ClearChildren();
	Rows.Reset();

	for (const FOBPartyMember& M : Party->GetMembers())
	{
		UOBPartyMemberRowWidget* Row = CreateWidget<UOBPartyMemberRowWidget>(this, RowWidgetClass);
		if (!Row) continue;
		
		Row->Setup(M);
		MembersBox->AddChild(Row);
		Rows.Add(Row);
	}

	if (TitleText)
		TitleText->SetText(FText::FromString(FString::Printf(TEXT("팀 (%d/%d)"), Party->GetPartySize(), Party->GetMaxPartySize())));
}

void UOBPartyWidget::HandleLeaveClicked()
{
	if (Party) 
		Party->LeaveParty();
}

void UOBPartyWidget::HandleDebugAdd()
{
	if (Party) 
		Party->DebugAddDummyMember(FText::GetEmpty());
}

void UOBPartyWidget::HandleDebugToggle()
{
	if (Party) 
		Party->DebugSetLocalLeader(!Party->IsLocalLeader());
}

