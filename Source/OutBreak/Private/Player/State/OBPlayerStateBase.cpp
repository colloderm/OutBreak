// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/State/OBPlayerStateBase.h"

#include "AbilitySystemComponent.h"
#include "Ability/Components/OBAbilitySystemComponent.h"
#include "Ability/Attributes/OBAttributeSetBase.h"

AOBPlayerStateBase::AOBPlayerStateBase()
{
	AbilitySystemComponent = CreateDefaultSubobject<UOBAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	SetNetUpdateFrequency(100.0f);
	
	AttributeSet = CreateDefaultSubobject<UOBAttributeSetBase>(TEXT("AttributeSet"));
	
}

UAbilitySystemComponent* AOBPlayerStateBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}