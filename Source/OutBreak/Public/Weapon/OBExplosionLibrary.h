#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OBExplosionLibrary.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;

/**
 * 폭발 반경 피해 + 임펄스.
 * 플레이어는 ASC(GameplayEffect), 좀비는 TakeDamage로 경로가 갈린다.
 * BP의 Apply Radial Damage 노드는 후자만 처리하므로 쓰면 안 된다.
 * 수류탄과 차량 폭발이 이 함수를 공유한다.
 */
UCLASS()
class OUTBREAK_API UOBExplosionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * @param BaseDamage        폭심 기준 피해. 거리에 따라 제곱근 감쇠한다.
	 * @param DamageEffect      플레이어용 GE. 비우면 플레이어는 피해를 안 받는다.
	 * @param ImpulseStrength   0이면 임펄스 없음. 살아있는 캐릭터는 LaunchCharacter,
	 *                          래그돌·물리 액터는 AddRadialImpulse로 밀린다.
	 * @param SourceASCOverride 가해자 ASC 직접 지정(수류탄용). 비우면 컨트롤러에서 찾는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "OutBreak|Explosion", meta = (WorldContext = "WorldContextObject", 
		AutoCreateRefTerm = "IgnoreActors"))
	static void ApplyExplosion(
		const UObject* WorldContextObject,
		FVector Center,
		float Radius,
		float BaseDamage,
		TSubclassOf<UGameplayEffect> DamageEffect,
		AController* InstigatorController,
		AActor* DamageCauser,
		float ImpulseStrength,
		const TArray<AActor*>& IgnoreActors,
		UAbilitySystemComponent* SourceASCOverride = nullptr);
};