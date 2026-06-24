// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Components/OBAbilitySystemComponent.h"

#include "Ability/Abilities/OBGameplayAbility.h"

void UOBAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid()) return;

	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			InputPressedSpecHandles.AddUnique(Spec.Handle);
			InputHeldSpecHandles.AddUnique(Spec.Handle);
		}
	}
}

void UOBAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid()) return;

	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			InputReleasedSpecHandles.AddUnique(Spec.Handle);
			InputHeldSpecHandles.Remove(Spec.Handle);
		}
	}
}

void UOBAbilitySystemComponent::ProcessAbilityInput(float DeltaTime, bool bGamePaused)
{
	TArray<FGameplayAbilitySpecHandle> AbilitiesToActivate;

	// 1) 유지 중: WhileInputActive 정책이면 활성화 대상(연사/조준 유지).
	for (const FGameplayAbilitySpecHandle& Handle : InputHeldSpecHandles)
	{
		if (const FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle))
		{
			if (Spec->Ability && !Spec->IsActive())
			{
				const UOBGameplayAbility* OBAbility = Cast<UOBGameplayAbility>(Spec->Ability);
				if (OBAbility && OBAbility->GetActivationPolicy() == EOBAbilityActivationPolicy::WhileInputActive)
				{
					AbilitiesToActivate.AddUnique(Handle);
				}
			}
		}
	}

	// 2) 눌림: 활성 중이면 InputPressed 통지, 아니면 OnInputTriggered 정책 활성화(단발).
	for (const FGameplayAbilitySpecHandle& Handle : InputPressedSpecHandles)
	{
		if (FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle))
		{
			if (Spec->Ability)
			{
				Spec->InputPressed = true;
				if (Spec->IsActive())
				{
					AbilitySpecInputPressed(*Spec);
				}
				else
				{
					const UOBGameplayAbility* OBAbility = Cast<UOBGameplayAbility>(Spec->Ability);
					if (OBAbility && OBAbility->GetActivationPolicy() == EOBAbilityActivationPolicy::OnInputTriggered)
					{
						AbilitiesToActivate.AddUnique(Handle);
					}
				}
			}
		}
	}

	// 3) 활성화 실행.
	for (const FGameplayAbilitySpecHandle& Handle : AbilitiesToActivate)
	{
		TryActivateAbility(Handle);
	}

	// 4) 뗌: 활성 능력에 InputReleased 통지 + WhileInputActive는 종료.
	for (const FGameplayAbilitySpecHandle& Handle : InputReleasedSpecHandles)
	{
		if (FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle))
		{
			if (Spec->Ability)
			{
				Spec->InputPressed = false;
				if (Spec->IsActive())
				{
					AbilitySpecInputReleased(*Spec);
					
					// WhileInputActive(ADS 등)는 입력 떼면 종료.
					const UOBGameplayAbility* OBAbility = Cast<UOBGameplayAbility>(Spec->Ability);
					if (OBAbility && OBAbility->GetActivationPolicy() == EOBAbilityActivationPolicy::WhileInputActive)
					{
						CancelAbilityHandle(Handle);
					}
				}
			}
		}
	}

	// 5) 이번 프레임 눌림/뗌 초기화(유지는 보존).
	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
}

void UOBAbilitySystemComponent::ClearAbilityInput()
{
	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
	InputHeldSpecHandles.Reset();
}

