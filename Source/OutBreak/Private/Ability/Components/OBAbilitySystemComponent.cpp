// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Components/OBAbilitySystemComponent.h"

#include "Ability/Abilities/OBGameplayAbility.h"

DEFINE_LOG_CATEGORY_STATIC(LogOBAbilityInput, Log, All);

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

void UOBAbilitySystemComponent::FlushPlayerAbilityInput(const TCHAR* Reason)
{
	TArray<FGameplayAbilitySpecHandle> HandlesToFlush;
	HandlesToFlush.Reserve(
		InputPressedSpecHandles.Num()
		+ InputReleasedSpecHandles.Num()
		+ InputHeldSpecHandles.Num());

	auto AddTrackedHandles = [&HandlesToFlush](const TArray<FGameplayAbilitySpecHandle>& Handles)
	{
		for (const FGameplayAbilitySpecHandle& Handle : Handles)
		{
			HandlesToFlush.AddUnique(Handle);
		}
	};

	AddTrackedHandles(InputPressedSpecHandles);
	AddTrackedHandles(InputReleasedSpecHandles);
	AddTrackedHandles(InputHeldSpecHandles);

	int32 PreservedPassiveCount = 0;

	// Spec::InputPressed survives the one-frame pressed array until the matching
	// release. Also collect every active player-driven OB ability so a reload,
	// melee, consumable, burst, or similar action cannot continue in transit.
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		const UOBGameplayAbility* OBAbility = Cast<UOBGameplayAbility>(Spec.Ability);
		if (Spec.IsActive()
			&& OBAbility
			&& OBAbility->GetActivationPolicy() == EOBAbilityActivationPolicy::OnSpawn)
		{
			++PreservedPassiveCount;
		}
		const bool bPlayerDrivenActiveAbility = Spec.IsActive()
			&& OBAbility
			&& OBAbility->GetActivationPolicy() != EOBAbilityActivationPolicy::OnSpawn;
		if (Spec.InputPressed || bPlayerDrivenActiveAbility)
		{
			HandlesToFlush.AddUnique(Spec.Handle);
		}
	}

	int32 ReleasedCount = 0;
	int32 CancelledCount = 0;
	for (const FGameplayAbilitySpecHandle& Handle : HandlesToFlush)
	{
		FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
		if (!Spec || !Spec->Ability)
		{
			continue;
		}

		const bool bHadTrackedInput = Spec->InputPressed
			|| InputPressedSpecHandles.Contains(Handle)
			|| InputReleasedSpecHandles.Contains(Handle)
			|| InputHeldSpecHandles.Contains(Handle);
		Spec->InputPressed = false;

		if (bHadTrackedInput && Spec->IsActive())
		{
			AbilitySpecInputReleased(*Spec);
			++ReleasedCount;
		}

		// Release callbacks may have ended the ability, so resolve the spec again
		// before deciding whether a cancellation is still necessary.
		Spec = FindAbilitySpecFromHandle(Handle);
		if (!Spec || !Spec->Ability || !Spec->IsActive())
		{
			continue;
		}

		const UOBGameplayAbility* OBAbility = Cast<UOBGameplayAbility>(Spec->Ability);
		if (OBAbility && OBAbility->GetActivationPolicy() == EOBAbilityActivationPolicy::OnSpawn)
		{
			continue;
		}

		// Unknown/non-OB abilities are preserved because their passive policy is
		// not knowable here. All project player-input abilities derive from
		// UOBGameplayAbility and are cancelled selectively by policy.
		if (OBAbility)
		{
			CancelAbilityHandle(Handle);
			++CancelledCount;
		}
	}

	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
	InputHeldSpecHandles.Reset();

	UE_LOG(LogOBAbilityInput, Log,
		TEXT("[AbilityInput] Player-input flush Reason=%s ASC=%s Handles=%d Released=%d CancelledPlayerAbilities=%d PreservedPassives=%d"),
		Reason ? Reason : TEXT("Unspecified"), *GetName(), HandlesToFlush.Num(), ReleasedCount,
		CancelledCount, PreservedPassiveCount);
}

