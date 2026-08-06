// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "OBAttributeSetBase.generated.h"

/*
 * 속성 접근자 보일러플레이트를 한 줄로 생성하는 표준 매크로.
 * Getter / ValueGetter / ValueSetter / Initter 를 자동 정의한다.
 */
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/*
왜 존재하는가?
 - 게임의 모든 수치 속성을 정의하고, 복제·경계처리·데미지반영을 한 곳에서 관리한다.
무엇을 저장하는가?
 - Health/MaxHealth(영속) 및 Damage(임시 계산용 메타 속성).
멀티플레이에서 어떤 역할을 하는가?
 - 서버가 권위를 갖고 값을 변경하며, RepNotify로 클라이언트와 동기화한다.
*/
UCLASS()
class OUTBREAK_API UOBAttributeSetBase : public UAttributeSet
{
	GENERATED_BODY()
public:
	UOBAttributeSetBase();

	//~ UAttributeSet interface
	// 복제할 속성 목록을 등록한다.
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	// 값이 '변경되기 직전' 호출 — 주로 경계(Clamp) 보정에 사용.
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	// GameplayEffect가 '실행 완료된 직후' 호출 — 메타 속성(Damage)을 실제 Health에 반영.
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	//~ End interface
 
	UPROPERTY(BlueprintReadOnly, Category = "Vital", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UOBAttributeSetBase, Health)
 
	UPROPERTY(BlueprintReadOnly, Category = "Vital", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UOBAttributeSetBase, MaxHealth)
 
	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	FGameplayAttributeData Damage;
	ATTRIBUTE_ACCESSORS(UOBAttributeSetBase, Damage)

	UPROPERTY(BlueprintReadOnly, Category = "Vital", ReplicatedUsing = OnRep_Stamina)
	FGameplayAttributeData Stamina;
	ATTRIBUTE_ACCESSORS(UOBAttributeSetBase, Stamina)

	UPROPERTY(BlueprintReadOnly, Category = "Vital", ReplicatedUsing = OnRep_MaxStamina)
	FGameplayAttributeData MaxStamina;
	ATTRIBUTE_ACCESSORS(UOBAttributeSetBase, MaxStamina)

	UPROPERTY(BlueprintReadOnly, Category = "Recovery", ReplicatedUsing = OnRep_HealthRegen)
	FGameplayAttributeData HealthRegen;
	ATTRIBUTE_ACCESSORS(UOBAttributeSetBase, HealthRegen)

	UPROPERTY(BlueprintReadOnly, Category = "Recovery", ReplicatedUsing = OnRep_StaminaRegen)
	FGameplayAttributeData StaminaRegen;
	ATTRIBUTE_ACCESSORS(UOBAttributeSetBase, StaminaRegen)

	UPROPERTY(BlueprintReadOnly, Category = "Mobility", ReplicatedUsing = OnRep_MoveSpeedMultiplier)
	FGameplayAttributeData MoveSpeedMultiplier;
	ATTRIBUTE_ACCESSORS(UOBAttributeSetBase, MoveSpeedMultiplier)

	UPROPERTY(BlueprintReadOnly, Category = "Carry", ReplicatedUsing = OnRep_CarryCapacity)
	FGameplayAttributeData CarryCapacity;
	ATTRIBUTE_ACCESSORS(UOBAttributeSetBase, CarryCapacity)

	UPROPERTY(BlueprintReadOnly, Category = "Combat", ReplicatedUsing = OnRep_RecoilControl)
	FGameplayAttributeData RecoilControl;
	ATTRIBUTE_ACCESSORS(UOBAttributeSetBase, RecoilControl)

	UPROPERTY(BlueprintReadOnly, Category = "Combat", ReplicatedUsing = OnRep_AimStability)
	FGameplayAttributeData AimStability;
	ATTRIBUTE_ACCESSORS(UOBAttributeSetBase, AimStability)

	UPROPERTY(BlueprintReadOnly, Category = "Combat", ReplicatedUsing = OnRep_MeleePower)
	FGameplayAttributeData MeleePower;
	ATTRIBUTE_ACCESSORS(UOBAttributeSetBase, MeleePower)

	UPROPERTY(BlueprintReadOnly, Category = "Defense", ReplicatedUsing = OnRep_Armor)
	FGameplayAttributeData Armor;
	ATTRIBUTE_ACCESSORS(UOBAttributeSetBase, Armor)
 
   protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION() void OnRep_Stamina(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxStamina(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_HealthRegen(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_StaminaRegen(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MoveSpeedMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_CarryCapacity(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_RecoilControl(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_AimStability(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MeleePower(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Armor(const FGameplayAttributeData& OldValue);
	
};
