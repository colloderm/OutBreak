// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "Game/Expedition/OBExpeditionTypes.h"
#include "Weapon/Data/OBWeaponData.h"
#include "OBPlayerStateBase.generated.h"

class AOBWeaponBase;
class UOBAbilitySystemComponent;
class UOBAttributeSetBase;
class UAbilitySystemComponent;

UCLASS()
class OUTBREAK_API AOBPlayerStateBase : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()
public:
	AOBPlayerStateBase();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* NewPlayerState) override;
	
	UOBAttributeSetBase* GetAttributeSet() const { return AttributeSet; }
	UOBAbilitySystemComponent* GetOBAbilitySystemComponent() const { return AbilitySystemComponent; }
	
	// 로비 선택(서버).
	void SetWeaponForSlot(EOBWeaponSlot Slot, TSubclassOf<AOBWeaponBase> WeaponClass);
	void SetReady(bool bInReady);

	const TArray<TSubclassOf<AOBWeaponBase>>& GetSelectedWeapons() const { return SelectedWeapons; }
	bool IsReady() const { return bReady; }
	
	EOBPlayerExpeditionStatus GetExpeditionStatus() const { return ExpeditionStatus; }
	bool IsAliveInExpedition() const { return ExpeditionStatus == EOBPlayerExpeditionStatus::Alive; }
	uint8 GetTeamId() const { return TeamId; }

	void SetExpeditionStatus(EOBPlayerExpeditionStatus NewStatus);
	void SetTeamId(uint8 NewTeamId);
	
	bool IsPartyLeader() const { return bIsPartyLeader; }
	void SetPartyLeader(bool bInLeader);
	
	// 클라가 GameInstance Loadout을 한 번에 밀어넣을 때 사용(세션 진입 시).
	void SetSelectedWeaponsBulk(const TArray<TSubclassOf<AOBWeaponBase>>& InWeapons);
	
public:
	// 로비 UI 갱신.
	DECLARE_MULTICAST_DELEGATE(FOBOnLobbyStateChanged);
	FOBOnLobbyStateChanged OnLobbyStateChanged;
	
	// 결과/HUD 갱신 알림.
	DECLARE_MULTICAST_DELEGATE(FOBOnExpeditionStatusChanged);
	FOBOnExpeditionStatusChanged OnExpeditionStatusChanged;
	
protected:
	UFUNCTION() 
	void OnRep_SelectedWeapons();
	
	UFUNCTION() 
	void OnRep_Ready();
	
	UFUNCTION() 
	void OnRep_ExpeditionStatus();
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ability", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UOBAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UOBAttributeSetBase> AttributeSet;

	UPROPERTY(ReplicatedUsing = OnRep_SelectedWeapons, BlueprintReadOnly, Category = "Lobby")
	TArray<TSubclassOf<AOBWeaponBase>> SelectedWeapons;

	UPROPERTY(ReplicatedUsing = OnRep_Ready, BlueprintReadOnly, Category = "Lobby")
	bool bReady = false;
	
	UPROPERTY(ReplicatedUsing = OnRep_ExpeditionStatus, BlueprintReadOnly, Category = "Expedition")
	EOBPlayerExpeditionStatus ExpeditionStatus = EOBPlayerExpeditionStatus::Alive;

	// 솔로=고유값 / 파티=공유값. GameMode가 진입 시 부여.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Expedition")
	uint8 TeamId = 0;
	
	// 솔로=true(자기 자신이 리더). 파티 시 팀장만 true → "탐사 시작" 버튼 게이팅.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Party")
	bool bIsPartyLeader = true;
};
