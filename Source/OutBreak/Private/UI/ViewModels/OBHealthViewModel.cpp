// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/ViewModels/OBHealthViewModel.h"

#include "Ability/Attributes/OBAttributeSetBase.h"
#include "AbilitySystemComponent.h"

void UOBHealthViewModel::SetAbilitySystemComponent(UAbilitySystemComponent* InASC)
{
	if (!InASC) return;
	
	AbilitySystemComponent = InASC;
	
	// 초기값 세팅(현재 속성 스냅샷)
	bool bFound = false;
	SetMaxHealth(InASC->GetGameplayAttributeValue(UOBAttributeSetBase::GetMaxHealthAttribute(), bFound));
	SetHealth(InASC->GetGameplayAttributeValue(UOBAttributeSetBase::GetHealthAttribute(), bFound));
	
	// 이후 변경을 구독(서버 복제 -> 클라 델리게이트 발화)
	InASC->GetGameplayAttributeValueChangeDelegate(UOBAttributeSetBase::GetHealthAttribute())
		.AddUObject(this, &UOBHealthViewModel::HandleHealthChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UOBAttributeSetBase::GetMaxHealthAttribute())
		.AddUObject(this, &UOBHealthViewModel::HandleMaxHealthChanged);
	
}

void UOBHealthViewModel::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	SetHealth(Data.NewValue);
}

void UOBHealthViewModel::HandleMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	SetMaxHealth(Data.NewValue);
}

void UOBHealthViewModel::SetHealth(float NewValue)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(Health, NewValue))
	{
		UpdateHealthPercent();
	}
}

void UOBHealthViewModel::SetMaxHealth(float NewValue)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, NewValue))
	{
		UpdateHealthPercent();
	}
}

void UOBHealthViewModel::UpdateHealthPercent()
{
	const float NewPercent = (MaxHealth > 0.f) ? (Health / MaxHealth) : 0.f;
	
	// 파생값 변경 전달
	if (UE_MVVM_SET_PROPERTY_VALUE(HealthPercent, NewPercent))
	{
		// 나중에 작업
	}
}

