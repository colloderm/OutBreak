// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "OBCharacterBase.generated.h"

DECLARE_MULTICAST_DELEGATE(FOBOnAbilitySystemInitialized);

class UOBPawnData;
class UOBAbilitySet;
class UAbilitySystemComponent;
class UOBAttributeSetBase;
class UOBEquipmentComponent;
class USpringArmComponent;
class UCameraComponent;

UCLASS()
class OUTBREAK_API AOBCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AOBCharacterBase();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	// 복제 등록
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION(BlueprintPure, Category = "OB|Death")
	bool IsDead() const { return bIsDead; }
	
	// 체력이 0이 되어 사망을 처리
	void HandleDeath();
	
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_DrawFireTrace(const FVector& Start, const FVector& End, bool bHit);
	
	// 서버에서 호출하여 모든 머신(서버+클라) 실행.
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayFireMontage(UAnimMontage* MontageToPlay);
	
	void SetPawnData(UOBPawnData* InPawnData) { PawnData = InPawnData; }
	
	// 조준 상태(서버). 이동 감속 + 복제 상태
	void SetAiming(bool bnewAiming);
	
	UFUNCTION(BlueprintPure, Category = "OB|ADS")
	bool IsAiming() const { return bIsAiming; }
	
	// 발사 시 집중 효과 펄스(로컬용)
	void AddFireFocusPulse(float PulseAmount);
	
public:
	/*
	왜 존재하는가? - ASC가 준비된 시점을 로컬 UI 등에 알린다(타이밍 문제 해결).
	멀티플레이 역할? - 서버/클라 각자 자기 머신에서 초기화 완료 시 브로드캐스트.
	*/
	FOBOnAbilitySystemInitialized OnAbilitySystemInitialized;
	
protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	
	//~ APawn interface
	virtual void PossessedBy(AController* NewController) override; // 서버 경로
	virtual void OnRep_PlayerState() override;                     // 클라이언트 경로
	//~ End interface

	void InitAbilitySystemComponent();
	
	// 사망 클라이언트 연출 처리.
	UFUNCTION()
	void OnRep_IsDead();

	// 이동/충돌 비활성 공통 로직(서버+클라).
	void DisablePawnForDeath();
	
	void StartDeath();

	void StartRagdoll();
	
	UFUNCTION()
	void OnRep_isAiming();
	void UpdateAimingState();
	
	// 발사 시 집중 효과를 카메라 적용
	void ApplyCombatFocusPostProcess();
	
protected:
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UOBAttributeSetBase> AttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pawn")
	TObjectPtr<UOBPawnData> PawnData;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	TObjectPtr<UOBAbilitySet> DefaultAbilitySet;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment")
	TObjectPtr<UOBEquipmentComponent> EquipmentComponent;
	
	// 집중 강도(0~1).
	UPROPERTY(EditDefaultsOnly, Category = "Camera|CombatFocus")
	float FocusVignette = 0.6f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Camera|CombatFocus")
	float FocusMotionBlur = 0.35f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Camera|CombatFocus")
	float FocusFringe = 2.0f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Camera|CombatFocus")
	float FocusRecoverySpeed = 6.0f;
	
	// 조준 중 유지되는 기본 집중(은은한 터널비전).
	UPROPERTY(EditDefaultsOnly, Category = "Camera|CombatFocus")
	float AimFocusBaseline = 0.3f;
	
	// ASC가 이미 초기화됐는지 가드(중복 InitAbilityActorInfo/브로드캐스트 방지).
	bool bAbilitySystemInitialized = false;
	
	UPROPERTY(ReplicatedUsing = OnRep_IsDead)
	bool bIsDead = false;
	
	UPROPERTY(ReplicatedUsing = OnRep_IsAiming)
	bool bIsAiming = false;
	
private:
	float DefaultWalkSpeed = 600.f;
	float DefaultCameraFOV = 90.f;
	float TargetCameraFOV = 90.f;
	float CameraBlendSpeed = 12.f;
	
	float CombatFocus = 0.0f;        // 현재
	float CombatFocusTarget = 0.0f;  // 목표(ADS 기준)
	float BaseVignette = 0.4f;       // 기본값 캐시
	float BaseMotionBlur = 0.0f;
};
