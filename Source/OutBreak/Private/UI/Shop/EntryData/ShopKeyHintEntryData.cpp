// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Shop/EntryData/ShopKeyHintEntryData.h"

void UShopKeyHintEntryData::Initialize(const FText& InActionText, const FText& InKeyText)
{
	ActionText = InActionText;
	KeyText = InKeyText;
}
