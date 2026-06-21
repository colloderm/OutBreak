// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "OBPlayerStateBase.generated.h"

class UOBAttributeSetBase;
class UAbilitySystemComponent;

UCLASS()
class OUTBREAK_API AOBPlayerStateBase : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()
public:
	AOBPlayerStateBase();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	UOBAttributeSetBase* GetAttributeSet() const { return AttributeSet; }
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ability", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UOBAttributeSetBase> AttributeSet;
};
