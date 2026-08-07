// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Attributes/OBAttributeSetBase.h"

#include "GameplayEffectExtension.h"
#include "Character/OBCharacterBase.h"
#include "Net/UnrealNetwork.h" 

UOBAttributeSetBase::UOBAttributeSetBase()
{
}

void UOBAttributeSetBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UOBAttributeSetBase, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UOBAttributeSetBase, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UOBAttributeSetBase, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UOBAttributeSetBase, MaxStamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UOBAttributeSetBase, HealthRegen, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UOBAttributeSetBase, StaminaRegen, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UOBAttributeSetBase, MoveSpeedMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UOBAttributeSetBase, CarryCapacity, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UOBAttributeSetBase, RecoilControl, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UOBAttributeSetBase, AimStability, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UOBAttributeSetBase, MeleePower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UOBAttributeSetBase, Armor, COND_None, REPNOTIFY_Always);
}

void UOBAttributeSetBase::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	else if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetMaxStaminaAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxStamina());
	}
	else if (Attribute == GetMoveSpeedMultiplierAttribute() ||
		Attribute == GetRecoilControlAttribute() ||
		Attribute == GetAimStabilityAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.05f);
	}
	else if (Attribute == GetCarryCapacityAttribute() ||
		Attribute == GetMeleePowerAttribute() ||
		Attribute == GetArmorAttribute() ||
		Attribute == GetHealthRegenAttribute() ||
		Attribute == GetStaminaRegenAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
}

void UOBAttributeSetBase::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		const float LocalDamage = GetDamage();
		SetDamage(0.0f);

		if (LocalDamage > 0.0f)
		{
			const float NewHealth = GetHealth() - LocalDamage;
			SetHealth(FMath::Clamp(NewHealth, 0.0f, GetMaxHealth()));
		}
	}
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	}
	
	// 체력이 0 이하가 되면 사망 처리(서버). 중복은 Character가 가드.
	const bool bAffectedHealth = 
		Data.EvaluatedData.Attribute == GetDamageAttribute() || Data.EvaluatedData.Attribute == GetHealthAttribute();

	if (bAffectedHealth && GetHealth() <= 0.0f)
	{
		if (UAbilitySystemComponent* OwningASC = GetOwningAbilitySystemComponent())
		{
			if (AOBCharacterBase* Character = Cast<AOBCharacterBase>(OwningASC->GetAvatarActor()))
			{
				Character->HandleDeath();
			}
		}
	}
	
}

void UOBAttributeSetBase::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UOBAttributeSetBase, Health, OldValue);
}

void UOBAttributeSetBase::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UOBAttributeSetBase, MaxHealth, OldValue);
}

void UOBAttributeSetBase::OnRep_Stamina(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UOBAttributeSetBase, Stamina, OldValue); }
void UOBAttributeSetBase::OnRep_MaxStamina(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UOBAttributeSetBase, MaxStamina, OldValue); }
void UOBAttributeSetBase::OnRep_HealthRegen(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UOBAttributeSetBase, HealthRegen, OldValue); }
void UOBAttributeSetBase::OnRep_StaminaRegen(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UOBAttributeSetBase, StaminaRegen, OldValue); }
void UOBAttributeSetBase::OnRep_MoveSpeedMultiplier(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UOBAttributeSetBase, MoveSpeedMultiplier, OldValue); }
void UOBAttributeSetBase::OnRep_CarryCapacity(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UOBAttributeSetBase, CarryCapacity, OldValue); }
void UOBAttributeSetBase::OnRep_RecoilControl(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UOBAttributeSetBase, RecoilControl, OldValue); }
void UOBAttributeSetBase::OnRep_AimStability(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UOBAttributeSetBase, AimStability, OldValue); }
void UOBAttributeSetBase::OnRep_MeleePower(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UOBAttributeSetBase, MeleePower, OldValue); }
void UOBAttributeSetBase::OnRep_Armor(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UOBAttributeSetBase, Armor, OldValue); }
