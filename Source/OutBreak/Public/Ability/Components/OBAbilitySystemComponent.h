// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "OBAbilitySystemComponent.generated.h"

/*
왜 존재하는가?
 - 입력 태그의 눌림/뗌/유지를 추적해 입력 상태 기반 능력 발동을 가능케 한다(Lyra 패턴).
멀티플레이 역할?
 - 소유 클라의 입력을 능력 활성화로 변환, 예측 실행 후 서버 확정.
*/
UCLASS()
class OUTBREAK_API UOBAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	// 입력 태그가 눌렸을 때(컨트롤러가 호출).
	void AbilityInputTagPressed(const FGameplayTag& InputTag);
	// 입력 태그가 떼졌을 때.
	void AbilityInputTagReleased(const FGameplayTag& InputTag);

	/*
	왜 호출되는가? - 매 틱 누적 입력을 능력 발동/통지로 처리.
	언제 호출되는가? - PlayerController Tick에서.
	서버/클라? - 소유 클라.
	*/
	void ProcessAbilityInput(float DeltaTime, bool bGamePaused);

	// 누적 입력 초기화(메뉴 전환 등).
	void ClearAbilityInput();
	
protected:
	// 이번 프레임 눌린 능력 핸들.
	TArray<FGameplayAbilitySpecHandle> InputPressedSpecHandles;
	// 이번 프레임 떼진 능력 핸들.
	TArray<FGameplayAbilitySpecHandle> InputReleasedSpecHandles;
	// 현재 유지(누름) 중인 능력 핸들.
	TArray<FGameplayAbilitySpecHandle> InputHeldSpecHandles;
};
