// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Ability/Abilities/OBGameplayAbility.h"
#include "OBGameplayAbility_RangedWeapon.generated.h"

class AOBWeaponBase;
class UOBWeaponData;

/*
 * 왜 존재하는가?
 - 히트스캔 무기 발사 행위를 담당한다. 트레이스 → 명중 대상에 데미지 GE 적용.
무엇을 저장하는가?
 - 자체 상태는 없음(무기 데이터에서 수치를 읽는다). 디버그 옵션만 보유.
멀티플레이에서 어떤 역할을 하는가?
 - 서버 권위로 판정/데미지를 처리하고, 결과(Health)는 AttributeSet이 복제한다.
 */
UCLASS()
class OUTBREAK_API UOBGameplayAbility_RangedWeapon : public UOBGameplayAbility
{
	GENERATED_BODY()
public:
	UOBGameplayAbility_RangedWeapon(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
protected:
	/*
	왜 호출되는가? - 발사 입력으로 능력이 발동될 때 실제 사격 로직을 실행.
	언제 호출되는가? - 입력 → TryActivateAbility 성공 시.
	서버/클라? - 양쪽 실행되나, 트레이스·데미지는 내부 권위 가드로 서버 전용.
	*/
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	AOBWeaponBase* GetEquippedWeapon() const;

	/*
	왜 호출되는가? - 서버에서 라인트레이스를 수행하고 명중 시 데미지를 적용.
	서버/클라? - 서버 전용(호출부에서 권위 확인 후 진입).
	*/
	void PerformServerWeaponTrace();

	/*
	왜 존재하는가? - 개발 중 탄도 확인용 디버그 라인 표시 토글.
	멀티플레이 역할? - 시각 디버그 전용(게임플레이 영향 없음).
	*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OB|Debug")
	bool bDrawDebugTrace = false;
};
