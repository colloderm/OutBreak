// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "OBPlayerStateBase.generated.h"

class UOBAbilitySystemComponent;
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
	
	// 타입이 필요한 곳(컨트롤러 등)을 위한 전용 getter.
	UOBAbilitySystemComponent* GetOBAbilitySystemComponent() const { return AbilitySystemComponent; }
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ability", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UOBAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UOBAttributeSetBase> AttributeSet;
};
