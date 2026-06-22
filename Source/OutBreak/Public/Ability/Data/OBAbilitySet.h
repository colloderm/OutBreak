// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GameplayAbilitySpecHandle.h"
#include "ActiveGameplayEffectHandle.h"
#include "OBAbilitySet.generated.h"

class UGameplayEffect;
class UAbilitySystemComponent;
class UOBGameplayAbility;

/*
왜 존재하는가? - 부여할 GameplayEffect 1개를 클래스+레벨로 묶는 데이터 구조.
멀티플레이 역할? - 순수 데이터(복제 없음). 서버 적용의 재료.
*/
USTRUCT(BlueprintType)
struct FOBAbilitySet_GameplayEffect
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "GameplayEffect")
	TSubclassOf<UGameplayEffect> GameplayEffect;

	UPROPERTY(EditDefaultsOnly, Category = "GameplayEffect")
	float EffectLevel = 1.0f;
};

/*
왜 존재하는가? - 부여할 GameplayAbility 1개를 클래스+레벨+입력태그로 묶는다.
무엇을 저장하는가? - 능력 클래스, 레벨, 발동 입력 태그.
멀티플레이 역할? - 데이터(복제 없음). 서버가 GiveAbility에 사용.
*/
USTRUCT(BlueprintType)
struct FOBAbilitySet_GameplayAbility
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "GameplayAbility")
	TSubclassOf<UOBGameplayAbility> Ability;

	UPROPERTY(EditDefaultsOnly, Category = "GameplayAbility")
	int32 AbilityLevel = 1;

	// 발동에 사용할 입력 태그(예: InputTag.Weapon.Fire). 입력 단계에서 매칭.
	UPROPERTY(EditDefaultsOnly, Category = "GameplayAbility", Meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

/*
왜 존재하는가? - 부여된 능력/효과의 핸들을 모아 나중에 한 번에 회수하기 위함.
무엇을 저장하는가? - Ability Spec 핸들과 Active GE 핸들 배열.
멀티플레이 역할? - 서버가 보관·회수. 복제 대상 아님(서버 권위 상태).
*/
USTRUCT(BlueprintType)
struct FOBAbilitySet_GrantedHandles
{
	GENERATED_BODY()

	// 부여 결과 핸들 추가(GiveToAbilitySystem 내부에서 호출).
	void AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle);
	void AddGameplayEffectHandle(const FActiveGameplayEffectHandle& Handle);

	void TakeFromAbilitySystem(UAbilitySystemComponent* ASC);

protected:
	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;

	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> GameplayEffectHandles;
};

/*
왜 존재하는가? - 한 묶음의 GAS(효과+능력)를 ASC에 일괄 부여/회수한다(Lyra AbilitySet 축소판).
멀티플레이 역할? - 데이터 정의(복제 없음). 부여/회수는 서버 권위.
*/
UCLASS(BlueprintType, Const)
class OUTBREAK_API UOBAbilitySet : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	/*
	왜 호출되는가? - 정의된 효과/능력 묶음을 대상 ASC에 일괄 부여.
	언제 호출되는가? - 캐릭터 스폰(효과) 또는 무기 장착(능력) 시.
	서버/클라? - 서버 전용(내부 권위 가드).
	OutGrantedHandles: 회수가 필요하면 핸들을 받을 포인터(불필요하면 nullptr).
	*/
	void GiveToAbilitySystem(UAbilitySystemComponent* ASC, FOBAbilitySet_GrantedHandles* OutGrantedHandles, UObject* SourceObject = nullptr) const;
	
protected:
	// 부여할 GameplayEffect 목록(예: 초기 스탯).
	UPROPERTY(EditDefaultsOnly, Category = "GameplayEffects")
	TArray<FOBAbilitySet_GameplayEffect> GrantedGameplayEffects;

	// 부여할 GameplayAbility 목록(예: 발사).
	UPROPERTY(EditDefaultsOnly, Category = "GameplayAbilities")
	TArray<FOBAbilitySet_GameplayAbility> GrantedGameplayAbilities;
};
