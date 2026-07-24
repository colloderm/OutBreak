// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "OBCharacterBase.generated.h"

class UOBInventoryComponent;
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
	
	// 조준 트레이스용 카메라(서버에도 컨트롤 회전 복제로 위치 유효).
	UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	
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
	
	UFUNCTION(BlueprintPure, Category = "OB|Combat")
	bool IsRecentlyFired() const { return bRecentlyFired; }
	
	// 현재 탄퍼짐 각도(도). 발사 트레이스와 크로스헤어가 같은 값을 쓴다.
	UFUNCTION(BlueprintPure, Category = "OB|Combat")
	float GetCurrentSpreadAngle() const;
	
	// 스프린트 카메라 랙 토글. BP 스프린트 시작(true)/종료(false)에서 호출.
	UFUNCTION(BlueprintCallable, Category = "OB|Camera")
	void SetSprintCameraLag(bool bSprinting);
	
	// 발사 시 집중 효과 펄스(로컬용)
	void AddFireFocusPulse(float PulseAmount);
	
	// 몽타주를 재생할 메시(우리 게임플레이 AnimInstance=슬롯 보유 메시).
	USkeletalMeshComponent* GetMontageMesh() const;
	
	// 발사 방향 보기
	void NotifyFired();
	
	// 탈출 시 서버 정리: 진행 중 능력(발사 등) 취소 + 무기 해제(재발사/이펙트 중단).
	void HandleExtracted();
	
	bool IsDowned() const { return bIsDowned; }
	void EnterDownedState();                     // 서버: 다운 진입(연출/능력차단/이동정지)
	void ReviveFromDowned(float HealthFraction); // 서버: 부활(체력 일부 회복)
	void FinishDeathFromDowned();                // 서버: 블리드아웃/전멸 → 확정 사망
	
	// 전투 지향(조준/발사) 상태가 바뀔 때만 BP에 통지. GASP 입력 상태 동기화용.
	UFUNCTION(BlueprintImplementableEvent, Category = "Combat")
	void OnCombatOrientationChanged(bool bCombat);
	
public:
	/*
	왜 존재하는가? - ASC가 준비된 시점을 로컬 UI 등에 알린다(타이밍 문제 해결).
	멀티플레이 역할? - 서버/클라 각자 자기 머신에서 초기화 완료 시 브로드캐스트.
	*/
	FOBOnAbilitySystemInitialized OnAbilitySystemInitialized;
	
	UPROPERTY(EditDefaultsOnly, Category = "Death")
	TObjectPtr<UAnimMontage> DeathMontage;        // 죽는 모션(없으면 즉시 래그돌)

	UPROPERTY(EditDefaultsOnly, Category = "Down")
	TObjectPtr<UAnimMontage> DownedEnterMontage;  // 쓰러지는 연출(없으면 포즈만)
	
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
	
	void UpdateCombatOrientation();
	void ClearRecentlyFired();
	
	UFUNCTION() void OnRep_IsDowned();
	
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
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UOBInventoryComponent> InventoryComponent;
	
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
	
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float CombatOrientHoldTime = 1.f;

	bool bRecentlyFired = false;
	FTimerHandle CombatOrientTimer;
	
	UPROPERTY(ReplicatedUsing = OnRep_IsDowned)
	bool bIsDowned = false;
	
	// 마지막으로 BP에 통지한 전투 상태(매 프레임 중복 통지 방지).
	bool bLastCombatOrientation = false;
	
private:
	float DefaultWalkSpeed = 600.f;
	float DefaultCameraFOV = 90.f;
	float TargetCameraFOV = 90.f;
	float CameraBlendSpeed = 12.f;
	
	float CombatFocus = 0.0f;        // 현재
	float CombatFocusTarget = 0.0f;  // 목표(ADS 기준)
	float BaseVignette = 0.4f;       // 기본값 캐시
	float BaseMotionBlur = 0.0f;
	
	// 스프린트 중 카메라 랙 속도. 낮을수록 카메라가 더 늘어져 따라와 속도감↑.
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Sprint")
	float SprintCameraLagSpeed = 5.f;
	
	// 걷기/조깅: 높을수록 랙 적음(거의 즉시)
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Sprint")
	float NormalCameraLagSpeed = 30.f; 

	// 스프린트↔평상시 랙 전환 부드러움
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Sprint")
	float CameraLagBlendSpeed = 6.f; 

	// Tick 보간 목표(내부 상태)
	float TargetCameraLagSpeed = 30.f; 
};
