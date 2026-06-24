// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "OBPlayerController.generated.h"

class UOBAbilitySystemComponent;
class UCameraShakeBase;
class UOBInputConfig;
struct FGameplayTag;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

UCLASS()
class OUTBREAK_API AOBPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	//발사 시 시야 회전 반동 + 카메라 쉐이크 적용.
	void ApplyWeaponRecoil(float PitchKick, float YawKick, float RecoverySpeed, TSubclassOf<UCameraShakeBase> CameraShake, float CameraShakeScale = 1.f);

protected:
	virtual void Tick(float DeltaSeconds) override;

private:
	// 누적된 반동을 매 프레임 0으로 복귀시킨다.
	void UpdateRecoilRecovery(float DeltaSeconds);

	// 아직 복구 안 된 반동 누적량.
	float AccumulatedRecoilPitch = 0.0f;
	float AccumulatedRecoilYaw = 0.0f;

	// 현재 무기의 복구 속도(ApplyWeaponRecoil에서 갱신).
	float CurrentRecoilRecoverySpeed = 8.0f;
	
protected:
	//~ APlayerController interface
	virtual void SetupInputComponent() override;
	virtual void BeginPlay() override;
	//~ End interface
	
	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_JumpStarted();
	void Input_JumpCompleted();
	
	// 능력 입력 핸들러(눌림/뗌).
	void Input_AbilityInputPressed(FGameplayTag InputTag);
	void Input_AbilityInputReleased(FGameplayTag InputTag);

	// 조종 폰의 커스텀 ASC 조회.
	UOBAbilitySystemComponent* GetOBAbilitySystemComponent() const;
	
protected:
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	int32 InputMappingPriority = 0;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UOBInputConfig> InputConfig;
	
};
