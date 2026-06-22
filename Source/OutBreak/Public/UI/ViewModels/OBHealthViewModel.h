// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "OBHealthViewModel.generated.h"

struct FOnAttributeChangeData;
class UAbilitySystemComponent;
/*
왜 존재하는가?
- GAS 체력 속성을 위젯이 바인딩할 표시 데이터로 변환하는 다리(MVVM ViewModel).
무엇을 저장하는가?
- Health, MaxHealth, 파생 HealthPercent.
멀티플레이에서 어떤 역할을 하는가?
- 로컬 전용. 복제된 속성을 구독해 UI를 갱신한다(복제 안 함).
 */
UCLASS(BlueprintType)
class OUTBREAK_API UOBHealthViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()
public:
	// 표시할 대상의 ASC 연결 및 초기값/구독 설정
	// 클라이언트만 적용, 서버X
	UFUNCTION(BlueprintCallable, Category = "OB|UI")
	void SetAbilitySystemComponent(UAbilitySystemComponent* InASC);
	
private:
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void HandleMaxHealthChanged(const FOnAttributeChangeData& Data);
	
	void UpdateHealthPercent();
	
	// --- Getter/Setter (FieldNotify가 참조) ---
	float GetHealth() const { return Health; }
	void SetHealth(float NewValue);
	
	float GetMaxHealth() const { return MaxHealth; }
	void SetMaxHealth(float NewValue);
	
	float GetHealthPercent() const { return HealthPercent; }
	
private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Setter, Category = "Health", meta = (AllowPrivateAccess = "true"))
	float Health = 0.0f;
	
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Setter, Category = "Health", meta = (AllowPrivateAccess = "true"))
	float MaxHealth = 0.0f;
	
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category = "Health", meta = (AllowPrivateAccess = "true"))
	float HealthPercent = 0.0f;
	
	UPROPERTY()
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	
};
