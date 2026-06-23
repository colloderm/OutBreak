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
	
	//현재 사망 여부
	UFUNCTION(BlueprintPure, Category = "OB|Death")
	bool IsDead() const { return bIsDead; }
	
	// 체력이 0이 되어 사망을 처리
	void HandleDeath();
	
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_DrawFireTrace(const FVector& Start, const FVector& End, bool bHit);
	
	// 서버에서 호출하여 모든 머신(서버+클라) 실행.
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayFireMontage(UAnimMontage* MontageToPlay);
	
protected:
	//~ APawn interface
	virtual void PossessedBy(AController* NewController) override; // 서버 경로
	virtual void OnRep_PlayerState() override;                     // 클라이언트 경로
	//~ End interface

	void InitAbilitySystemComponent();
	
	/*
	왜 호출되는가? - 사망 복제 도착 시 클라이언트 연출 처리.
	서버/클라? - 클라이언트 콜백.
	*/
	UFUNCTION()
	void OnRep_IsDead();

	// 이동/충돌 비활성 공통 로직(서버+클라).
	void DisablePawnForDeath();
	
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
	
	// ASC가 이미 초기화됐는지 가드(중복 InitAbilityActorInfo/브로드캐스트 방지).
	bool bAbilitySystemInitialized = false;
	
	/*
	왜 존재하는가? - 사망 상태. UI/연출/로직 분기의 기준.
	멀티플레이 역할? - 서버가 설정, OnRep으로 클라 연출 트리거.
	*/
	UPROPERTY(ReplicatedUsing = OnRep_IsDead)
	bool bIsDead = false;

public:
	void SetPawnData(UOBPawnData* InPawnData) { PawnData = InPawnData; }
	
public:
	/*
	왜 존재하는가? - ASC가 준비된 시점을 로컬 UI 등에 알린다(타이밍 문제 해결).
	멀티플레이 역할? - 서버/클라 각자 자기 머신에서 초기화 완료 시 브로드캐스트.
	*/
	FOBOnAbilitySystemInitialized OnAbilitySystemInitialized;
};
