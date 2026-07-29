// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Party/OBPartyTypes.h"
#include "OBPartySubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE(FOBPartyChanged);

/**
 * 파티(팀) 데이터 소유자 — GameInstance 수명.
 * - 현재는 로컬 스텁: 초대/참가/동기화 없음. 리더십→PlayerState push로 게이팅/큐만 구동.
 * - 데디+세션(M7) 시 Members 동기화 + 실제 초대/수락으로 교체.
 */
UCLASS()
class OUTBREAK_API UOBPartySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	int32 GetPartySize() const { return Members.Num(); }
	bool IsLocalLeader() const;
	const TArray<FOBPartyMember>& GetMembers() const { return Members; }
	
	int32 GetMaxPartySize() const { return MaxPartySize; }

	void LeaveParty();  // 솔로로 리셋

	// ── 스텁 테스트용(실 초대/참가는 M7) ──
	UFUNCTION(BlueprintCallable, Category = "Party|Debug") 
	void DebugAddDummyMember(FText Name);
	
	UFUNCTION(BlueprintCallable, Category = "Party|Debug") 
	void DebugSetLocalLeader(bool bLeader);

	FOBPartyChanged OnPartyChanged;

protected:
	void RebuildSoloDefault();
	void NotifyChanged(); // 브로드캐스트 + PlayerState push
	void PushLeadershipToPlayerState();
	
protected:
	UPROPERTY()
	TArray<FOBPartyMember> Members;

	FString LocalPlayerId;
	
	// 파티 정원(솔로 포함). 초과 추가 차단.
	int32 MaxPartySize = 3;
};
