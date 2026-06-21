// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "OBGameplayAbility.generated.h"

class AOBCharacterBase;

UENUM(BlueprintType)
enum class EOBAbilityActivationPolicy : uint8
{
	// 입력이 눌리는 순간 1회 발동(예: 단발 사격, 재장전).
	OnInputTriggered,
	// 입력이 유지되는 동안 발동 시도(예: 연사, 조준).
	WhileInputActive,
	// 부여되는 즉시 발동(예: 패시브 효과).
	OnSpawn
};

UCLASS(Abstract)
class OUTBREAK_API UOBGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UOBGameplayAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	EOBAbilityActivationPolicy GetActivationPolicy() const { return ActivationPolicy; }

	/*
	왜 호출되는가? - 능력 로직에서 소유 캐릭터(Avatar)에 자주 접근하므로 캐스팅을 캡슐화.
	서버/클라? - 양쪽 모두.
	*/
	UFUNCTION(BlueprintPure, Category = "OB|Ability")
	AOBCharacterBase* GetOBCharacterFromActorInfo() const;
	
protected:
	/*
	왜 호출되는가? - 능력이 ASC에 부여되어 Avatar가 정해질 때. OnSpawn 정책 자동 발동 처리.
	언제 호출되는가? - GiveAbility 후 Avatar 설정 시점.
	서버/클라? - 양쪽. 단 TryActivate의 실제 실행은 NetExecutionPolicy를 따른다.
	*/
	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	/*
	왜 존재하는가? - 이 능력이 어떻게 발동되는지 정의(입력/유지/패시브).
	멀티플레이 역할? - 데이터 정의(복제 불필요). 입력 바인딩/자동발동 분기에 사용.
	*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OB|Ability Activation")
	EOBAbilityActivationPolicy ActivationPolicy = EOBAbilityActivationPolicy::OnInputTriggered;
};
