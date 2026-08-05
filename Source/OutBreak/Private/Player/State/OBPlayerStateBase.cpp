// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/State/OBPlayerStateBase.h"

#include "Net/UnrealNetwork.h"
#include "AbilitySystemComponent.h"
#include "Ability/Components/OBAbilitySystemComponent.h"
#include "Ability/Attributes/OBAttributeSetBase.h"
#include "Character/OBCharacterBase.h"
#include "GameFramework/PlayerController.h"
#include "LoadOut/OBLoadoutSubsystem.h"
#include "Weapon/OBWeaponBase.h"
#include "Engine/World.h"

AOBPlayerStateBase::AOBPlayerStateBase()
{
	AbilitySystemComponent = CreateDefaultSubobject<UOBAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	SetNetUpdateFrequency(100.0f);
	
	AttributeSet = CreateDefaultSubobject<UOBAttributeSetBase>(TEXT("AttributeSet"));
	
}

UAbilitySystemComponent* AOBPlayerStateBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AOBPlayerStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AOBPlayerStateBase, SelectedWeapons);
	DOREPLIFETIME(AOBPlayerStateBase, bReady);
	DOREPLIFETIME(AOBPlayerStateBase, ExpeditionStatus);
	DOREPLIFETIME(AOBPlayerStateBase, TeamId);
	DOREPLIFETIME(AOBPlayerStateBase, bIsPartyLeader);
	DOREPLIFETIME(AOBPlayerStateBase, SelectedCarryItems);
	DOREPLIFETIME_CONDITION(AOBPlayerStateBase, ExtractionProgress, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AOBPlayerStateBase, bIsExtracting,      COND_OwnerOnly);
}

void AOBPlayerStateBase::SetWeaponForSlot(EOBWeaponSlot Slot, TSubclassOf<AOBWeaponBase> WeaponClass)
{
	if (!HasAuthority()) return;

	// 같은 슬롯의 기존 선택 제거(슬롯당 1개).
	SelectedWeapons.RemoveAll([Slot](const TSubclassOf<AOBWeaponBase>& W)
	{
		if (!W) return false;
		if (AOBWeaponBase* CDO = W->GetDefaultObject<AOBWeaponBase>())
			if (UOBWeaponData* Data = CDO->GetWeaponData())
				return Data->WeaponSlot == Slot;
		return false;
	});

	if (WeaponClass) 
		SelectedWeapons.Add(WeaponClass);
	
	OnLobbyStateChanged.Broadcast();
}

void AOBPlayerStateBase::SetReady(bool bInReady)
{
	if (!HasAuthority()) return;
	
	bReady = bInReady;
	OnLobbyStateChanged.Broadcast();
}

void AOBPlayerStateBase::SetExpeditionStatus(EOBPlayerExpeditionStatus NewStatus)
{
	if (!HasAuthority() || ExpeditionStatus == NewStatus) return;
	
	ExpeditionStatus = NewStatus;
	OnExpeditionStatusChanged.Broadcast(); // 리슨 호스트 로컬 갱신
	ApplyExpeditionStatusToLoadout();   // ← 리슨 호스트/스탠드얼론 커버
}

void AOBPlayerStateBase::SetTeamId(uint8 NewTeamId)
{
	if (!HasAuthority()) return;
	
	TeamId = NewTeamId;
}

void AOBPlayerStateBase::SetExtractionProgress(float InProgress01, bool bInExtracting)
{
	if (!HasAuthority()) return;

	ExtractionProgress = InProgress01;
	bIsExtracting = bInExtracting;
	OnExtractionProgressChanged.Broadcast(); // 리슨 호스트 로컬 즉시 갱신
}

void AOBPlayerStateBase::SetPartyLeader(bool bInLeader)
{
	if (!HasAuthority()) return;
	
	bIsPartyLeader = bInLeader;
	OnLobbyStateChanged.Broadcast();
}

void AOBPlayerStateBase::SetSelectedWeaponsBulk(const TArray<TSubclassOf<AOBWeaponBase>>& InWeapons)
{
	if (!HasAuthority()) return;

	SelectedWeapons = InWeapons;   // 통째 교체(슬롯 유효성은 클라 Loadout이 이미 보장)
	OnLobbyStateChanged.Broadcast();
}

void AOBPlayerStateBase::SetCarryItemsBulk(const TArray<FOBItemStack>& InItems)
{
	if (!HasAuthority()) return;

	SelectedCarryItems = InItems;

	// 폰이 먼저 스폰됐을 수 있다(클라 push가 늦게 도착하는 경우). 그때 여기서 채운다.
	if (AOBCharacterBase* Char = Cast<AOBCharacterBase>(GetPawn()))
	{
		Char->ApplyCarryItems();
	}
}

bool AOBPlayerStateBase::AreSameTeam(const AActor* A, const AActor* B)
{
	auto GetTeam = [](const AActor* Actor, uint8& OutTeam) -> bool 
	{
		const APawn* Pawn = Cast<APawn>(Actor);
		if (!Pawn) return false;
		if (const AOBPlayerStateBase* PS = Pawn->GetPlayerState<AOBPlayerStateBase>())
		{
			OutTeam = PS->GetTeamId();
			return true;
		}
		return false; // 컨트롤러/PS 없음(AI 등) -> 팀 없음
	};
	
	uint8 TA = 0, TB = 0;
	if (!GetTeam(A,TA) || !GetTeam(B,TB)) return false; // 한쪽이라도 플레이어 아님
	return TA != 0 && TA == TB; // 0 미배정 제외, 같은 팀만 true
}

void AOBPlayerStateBase::OnRep_SelectedWeapons()
{
	OnLobbyStateChanged.Broadcast();
}

void AOBPlayerStateBase::OnRep_Ready()
{
	OnLobbyStateChanged.Broadcast();
}

void AOBPlayerStateBase::OnRep_ExpeditionStatus()
{
	OnExpeditionStatusChanged.Broadcast();
	ApplyExpeditionStatusToLoadout();
}

void AOBPlayerStateBase::OnRep_ExtractionProgress()
{
	OnExtractionProgressChanged.Broadcast(); // 클라 HUD 갱신
}

void AOBPlayerStateBase::ApplyExpeditionStatusToLoadout()
{
	const UWorld* World = GetWorld();
	const APlayerController* LocalPC = World ? World->GetFirstPlayerController() : nullptr;
	if (!LocalPC || LocalPC->PlayerState != this) return;   // 내 로컬 PS만

	UOBLoadoutSubsystem* Loadout = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UOBLoadoutSubsystem>() : nullptr;
	if (!Loadout) return;

	switch (ExpeditionStatus)
	{
	case EOBPlayerExpeditionStatus::Dead:      
		Loadout->ClearLoadout();
		Loadout->ClearCarryItems();   // 반입분도 잃는다(시체로 넘어갔다)
		break;
	case EOBPlayerExpeditionStatus::Extracted: 
		Loadout->AddCurrency(ExtractReward); 
		// 반입분은 가방째 정산되어 이미 창고에 들어왔다. 여기 남기면 다음 판에 복사된다.
		Loadout->ClearCarryItems();
		break;
	default: 
		break;
	}
}

void AOBPlayerStateBase::CopyProperties(APlayerState* NewPlayerState)
{
	Super::CopyProperties(NewPlayerState);
	
	if (AOBPlayerStateBase* PS = Cast<AOBPlayerStateBase>(NewPlayerState))
	{
		PS->SelectedWeapons = SelectedWeapons;
		PS->SelectedCarryItems = SelectedCarryItems;
		PS->bReady = bReady;
	}
}
