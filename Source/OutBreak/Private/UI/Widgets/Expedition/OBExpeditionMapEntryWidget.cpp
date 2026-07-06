// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/Expedition/OBExpeditionMapEntryWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Game/Expedition/OBExpeditionMapData.h"

void UOBExpeditionMapEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (RootButton)
		RootButton->OnClicked.AddDynamic(this, &UOBExpeditionMapEntryWidget::HandleClicked);
}

void UOBExpeditionMapEntryWidget::Setup(UOBExpeditionMapData* InMapData)
{
	MapData = InMapData;
	if (!MapData) return;
	
	if (NameText)
		NameText->SetText(MapData->DisplayName);
	
	if (ThumbnailImage && MapData->Thumbnail)
		ThumbnailImage->SetBrushFromTexture(MapData->Thumbnail);
	
	if (DifficultyText)
		DifficultyText->SetText(FText::FromString(FString::ChrN(MapData->DifficultyStars, TEXT('★'))));
	
	SetSelected(false);
}

void UOBExpeditionMapEntryWidget::SetSelected(bool bSelected)
{
	if (CheckImage)
		CheckImage->SetVisibility(bSelected ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
}

void UOBExpeditionMapEntryWidget::HandleClicked()
{
	OnEntryClicked.Broadcast(MapData);
}
