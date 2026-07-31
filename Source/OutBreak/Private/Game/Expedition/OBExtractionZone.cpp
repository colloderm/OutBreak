// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/Expedition/OBExtractionZone.h"

#include "Components/SphereComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Game/GameState/OBExpeditionGameState.h"
#include "Game/GameMode/OBExpeditionGameMode.h"
#include "Player/State/OBPlayerStateBase.h"
#include "Engine/World.h"


// Sets default values
AOBExtractionZone::AOBExtractionZone()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // 오버랩 발생 시에만 켬(비용 절감)

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	SetRootComponent(Trigger);
	
	Trigger->InitSphereRadius(300.f);
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionObjectType(ECC_WorldDynamic);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);

	bReplicates = false; // 판정은 서버 전용, 결과는 PlayerState로 복제
}

void AOBExtractionZone::ConfigureAsPersonal(uint8 InTeamId)
{
	OwningTeamId = InTeamId;

	// 팀원 전원에게 보여야 하므로 bOnlyRelevantToOwner는 못 쓴다(소유자는 1명뿐).
	SetReplicates(true);
	bOnlyRelevantToOwner = false;
}

bool AOBExtractionZone::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const
{
	// 공용 탈출구는 엔진 기본 규칙(거리 컬링 포함) 그대로.
	if (OwningTeamId == 0)
	{
		return Super::IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
	}

	const AController* C = Cast<AController>(RealViewer);
	if (!C)
	{
		if (const APawn* P = Cast<APawn>(ViewTarget)) C = P->GetController();
	}

	const AOBPlayerStateBase* PS = C ? C->GetPlayerState<AOBPlayerStateBase>() : nullptr;

	// 비콘이라 거리와 무관하게 항상 보여야 한다. 팀만 본다.
	return PS && PS->GetTeamId() == OwningTeamId;
}

void AOBExtractionZone::SetActiveWindow(int32 InStartSec, int32 InEndSec)
{
	ActiveStartSec = InStartSec;
	ActiveEndSec = InEndSec;
}

// Called when the game starts or when spawned
void AOBExtractionZone::BeginPlay()
{
	Super::BeginPlay();
	
	// 서버만 오버랩 구독 → 판정 서버 권위.
	if (HasAuthority())
	{
		Trigger->OnComponentBeginOverlap.AddDynamic(this, &AOBExtractionZone::OnTriggerBeginOverlap);
		Trigger->OnComponentEndOverlap.AddDynamic(this, &AOBExtractionZone::OnTriggerEndOverlap);
	}
	
	// 활성 상태 주기 체크(서버/클라 모두). 클라(소유자)에서 비콘이 렌더됨.
	CheckActiveState();
	GetWorldTimerManager().SetTimer(ActiveCheckTimer, this, &AOBExtractionZone::CheckActiveState, 1.0f, true);
}

void AOBExtractionZone::OnTriggerBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn) return;

	AController* C = Pawn->GetController();
	if (!C || !C->IsPlayerController()) return; // 플레이어만

	AOBPlayerStateBase* PS = C->GetPlayerState<AOBPlayerStateBase>();
	if (!PS || PS->GetExpeditionStatus() != EOBPlayerExpeditionStatus::Alive) return;

	HoldProgress.Add(C, 0.f);
	SetActorTickEnabled(true); // 추적 대상이 생겼으니 틱 가동
}

void AOBExtractionZone::OnTriggerEndOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn) return;

	AController* C = Pawn->GetController();
	if (!C) return;

	HoldProgress.Remove(C);

	// 나가면 진행도 리셋(HUD 끔).
	if (AOBPlayerStateBase* PS = C->GetPlayerState<AOBPlayerStateBase>())
	{
		PS->SetExtractionProgress(0.f, false);
	}

	if (HoldProgress.Num() == 0)
	{
		SetActorTickEnabled(false);
	}
}

void AOBExtractionZone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || HoldProgress.Num() == 0) return;

	const bool bActive = IsActiveNow();

	// 홀드 완료자는 순회 후 따로 처리(순회 중 맵 수정 금지).
	TArray<AController*> Finished;

	for (TPair<TObjectPtr<AController>, float>& Pair : HoldProgress)
	{
		AController* C = Pair.Key;
		if (!C) continue;

		AOBPlayerStateBase* PS = C->GetPlayerState<AOBPlayerStateBase>();

		// 무효 조건(사망/비활성/사용불가) → 진행도 리셋 후 다음.
		if (!PS || PS->GetExpeditionStatus() != EOBPlayerExpeditionStatus::Alive
			|| !bActive || !CanPlayerExtract(C))
		{
			Pair.Value = 0.f;
			if (PS) PS->SetExtractionProgress(0.f, false);
			continue;
		}

		Pair.Value += DeltaSeconds;
		PS->SetExtractionProgress(FMath::Clamp(Pair.Value / HoldTime, 0.f, 1.f), true);

		if (Pair.Value >= HoldTime)
		{
			Finished.Add(C);
		}
	}

	for (AController* C : Finished)
	{
		HoldProgress.Remove(C);
		if (AOBExpeditionGameMode* GM = GetWorld()->GetAuthGameMode<AOBExpeditionGameMode>())
		{
			GM->NotifyPlayerExtracted(C);
		}
	}

	if (HoldProgress.Num() == 0)
	{
		SetActorTickEnabled(false);
	}
}

bool AOBExtractionZone::IsActiveNow() const
{
	const AOBExpeditionGameState* GS = GetExpeditionGameState();
	if (!GS) return true; // GameState 없으면 안전하게 활성 취급

	const int32 Elapsed = GS->GetElapsedSeconds();
	if (Elapsed < ActiveStartSec) return false;
	if (ActiveEndSec > 0 && Elapsed > ActiveEndSec) return false;
	return true;
}

bool AOBExtractionZone::CanPlayerExtract(AController* C) const
{
	// 판정은 서버 오버랩이라 "안 보인다"만으로는 못 막는다. 팀을 여기서 걸러야 한다.
	if (OwningTeamId != 0)
	{
		const AOBPlayerStateBase* PS = C ? C->GetPlayerState<AOBPlayerStateBase>() : nullptr;
		if (!PS || PS->GetTeamId() != OwningTeamId) return false;
	}

	// 인벤토리 붙으면: ExtractType==Personal && RequiredItemTag 유효 시
	// 플레이어 인벤토리에 아이템이 있는지 검사 + 탈출 성사 시 소비.
	return true;
}

AOBExpeditionGameState* AOBExtractionZone::GetExpeditionGameState() const
{
	return GetWorld() ? GetWorld()->GetGameState<AOBExpeditionGameState>() : nullptr;
}

void AOBExtractionZone::CheckActiveState()
{
	const bool bNow = IsActiveNow();
	if (bNow != bLastActiveState)
	{
		bLastActiveState = bNow;
		OnExtractionActiveChanged(bNow); // BP: 비콘 on/off
	}
}

