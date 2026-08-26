// Fill out your copyright notice in the Description page of Project Settings.

// 트레일러 촬영용 콘솔 명령 모음. 헤더가 없다 — 게임플레이 코드가 참조할 일이 없고
// 콘솔에서만 호출한다. Shipping에서는 통째로 빠진다.

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Capture/OBCaptureFlightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Perception/AIPerceptionSystem.h"
#include "UI/HUD/OBHUD.h"

namespace OBCapture
{
	static APlayerController* GetLocalPC(const UWorld* World)
	{
		return (World && GEngine) ? GEngine->GetFirstLocalPlayerController(World) : nullptr;
	}

	/**
	 * 폰 아래에 매달린 모든 액터를 모은다.
	 *
	 * AActor::GetAttachedActors(recursive)를 쓰지 않는 이유:
	 * 그쪽은 액터 부착 관계만 따라가는데, 이 프로젝트의 무기는
	 * OBEquipmentComponent가 캐릭터의 '시각용 자식 스켈레탈 메시'에 붙인다.
	 * 부착이 액터 경계를 넘나들기 때문에, 컴포넌트 트리에서 소유자를 직접
	 * 걷어 올리는 편이 빠짐이 없다. 총기 부품(UStaticMeshComponent)도
	 * 무기 메시에 붙어 있으므로 이 순회에 전부 걸린다.
	 */
	static void CollectVisualOwners(APawn* Pawn, TSet<AActor*>& OutOwners)
	{
		OutOwners.Add(Pawn);

		USceneComponent* Root = Pawn->GetRootComponent();
		if (!Root)
		{
			return;
		}

		TArray<USceneComponent*> Descendants;
		Root->GetChildrenComponents(/*bIncludeAllDescendants=*/true, Descendants);

		for (const USceneComponent* Component : Descendants)
		{
			if (AActor* Owner = Component ? Component->GetOwner() : nullptr)
			{
				OutOwners.Add(Owner);
			}
		}
	}

	static int32 CountVisiblePrimitives(const AActor* Actor)
	{
		int32 Count = 0;
		Actor->ForEachComponent<UPrimitiveComponent>(/*bIncludeFromChildActors=*/true,
			[&Count](const UPrimitiveComponent* Primitive)
			{
				if (Primitive && Primitive->IsVisible())
				{
					++Count;
				}
			});
		return Count;
	}

	/** 캐릭터 + 무기 + 총기 부품을 지정 상태로 만든다. 반환: 처리한 액터 수(0이면 실패). */
	static int32 SetCharacterHidden(UWorld* World, bool bHide)
	{
		APlayerController* PC = GetLocalPC(World);
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Pawn)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Capture] 조종 중인 폰이 없다. 관전 중이면 먼저 빙의할 것."));
			return 0;
		}

		TSet<AActor*> Owners;
		CollectVisualOwners(Pawn, Owners);

		for (AActor* Actor : Owners)
		{
			if (!IsValid(Actor))
			{
				continue;
			}

			// SetActorHiddenInGame은 컴포넌트별 bVisible을 건드리지 않는다.
			// SetVisibility(propagate=true)를 쓰면 안 되는 이유가 이것 —
			// 되돌릴 때 원래 숨겨져 있던 메시(GASP 리타겟 소스 메시 등)까지
			// 같이 켜져서 캐릭터가 겹쳐 보인다.
			Actor->SetActorHiddenInGame(bHide);

			UE_LOG(LogTemp, Log, TEXT("[Capture]   %s %s (프리미티브 %d)"),
				bHide ? TEXT("숨김") : TEXT("표시"),
				*Actor->GetName(),
				CountVisiblePrimitives(Actor));
		}

		UE_LOG(LogTemp, Log, TEXT("[Capture] %s — 액터 %d개"),
			bHide ? TEXT("캐릭터 숨김") : TEXT("캐릭터 표시"), Owners.Num());

		// 콜리전을 끈다. 좀비 근접 공격은 오버랩/트레이스로 판정하므로, 인지를 끊어도
		// 이미 달라붙은 개체에게 계속 맞는다. 이게 피격을 끊는 실질적인 수단이다.
		// 비행은 AddActorWorldOffset(bSweep=false)이라 콜리전에 의존하지 않는다.
		Pawn->SetActorEnableCollision(!bHide);

		// AI 인지에서 아예 빼 버린다. 이 프로젝트는 별도 StimuliSource 컴포넌트 없이
		// 폰이 자동 등록되는 구조라, 시스템에서 직접 등록을 해제하는 게 맞다.
		// Sense를 비워 넘기면 시야·청각 전부에서 빠진다.
		if (UAIPerceptionSystem* Perception = World ? UAIPerceptionSystem::GetCurrent(*World) : nullptr)
		{
			if (bHide)
			{
				Perception->UnregisterSource(*Pawn);
			}
			else
			{
				Perception->RegisterSource(*Pawn);
			}
			UE_LOG(LogTemp, Log, TEXT("[Capture] AI 인지 %s"), bHide ? TEXT("해제") : TEXT("복구"));
		}

		// 숨긴 동안에는 에디터식 자유 비행을 켠다. 보이게 되돌리면 원래 이동으로 복귀한다.
		// 카메라가 아니라 폰이 움직이므로 World Partition 스트리밍 소스가 따라온다 —
		// ToggleDebugCamera로 카메라만 빼면 셀이 언로드돼 빈 땅이 찍히는 문제가 없다.
		if (ACharacter* Character = Cast<ACharacter>(Pawn))
		{
			if (bHide)
			{
				UOBCaptureFlightComponent::Enable(Character);
			}
			else
			{
				UOBCaptureFlightComponent::Disable(Character);
			}
		}

		return Owners.Num();
	}

	/**
	 * 게임플레이 HUD를 지정 상태로 만든다.
	 *
	 * AOBHUD가 이미 헬기 삽입 구간용으로 같은 기능을 갖고 있어 그대로 쓴다.
	 * 대상은 체력·탄약·소모품·세션타이머·크로스헤어다. 전체 지도(M키)는
	 * 그쪽 설계상 제외돼 있는데, 열지 않으면 화면에 없으므로 촬영에는 문제없다.
	 *
	 * 콘솔의 ShowHUD로는 안 된다 — 그건 AHUD 캔버스만 끄고 UMG 위젯은 남는다.
	 */
	static bool SetHUDVisible(const UWorld* World, bool bVisible)
	{
		APlayerController* PC = GetLocalPC(World);
		AOBHUD* HUD = PC ? PC->GetHUD<AOBHUD>() : nullptr;
		if (!HUD)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Capture] AOBHUD가 없다. HUD 클래스 설정을 확인할 것."));
			return false;
		}

		HUD->SetGameplayHUDVisible(bVisible);
		UE_LOG(LogTemp, Log, TEXT("[Capture] HUD %s"), bVisible ? TEXT("표시") : TEXT("숨김"));
		return true;
	}

	static bool IsCharacterHidden(const UWorld* World)
	{
		APlayerController* PC = GetLocalPC(World);
		const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		return Pawn && Pawn->IsHidden();
	}

	//~ 콘솔 진입점 ---------------------------------------------------------
	// 상태 변수를 두지 않고 매번 현재 상태를 뒤집는다. 재진입에 안전하고,
	// 도중에 리스폰이나 맵 이동이 있어도 어긋나지 않는다.

	static void ToggleCharacter(UWorld* World)
	{
		SetCharacterHidden(World, !IsCharacterHidden(World));
	}

	static void ToggleHUD(UWorld* World)
	{
		APlayerController* PC = GetLocalPC(World);
		const AOBHUD* HUD = PC ? PC->GetHUD<AOBHUD>() : nullptr;
		SetHUDVisible(World, HUD ? !HUD->IsGameplayHUDVisible() : false);
	}

	/** 촬영 중 가장 많이 쓸 명령. 캐릭터 기준으로 둘을 같은 상태로 맞춘다. */
	static void ToggleAll(UWorld* World)
	{
		const bool bHide = !IsCharacterHidden(World);
		SetCharacterHidden(World, bHide);
		SetHUDVisible(World, !bHide);
	}
}

static FAutoConsoleCommandWithWorld GOBCaptureToggleCharacter(
	TEXT("ob.Capture.ToggleCharacter"),
	TEXT("캐릭터·무기·총기 부품 표시를 토글한다."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&OBCapture::ToggleCharacter));

static FAutoConsoleCommandWithWorld GOBCaptureToggleHUD(
	TEXT("ob.Capture.ToggleHUD"),
	TEXT("게임플레이 HUD(체력·탄약·소모품·타이머·크로스헤어) 표시를 토글한다."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&OBCapture::ToggleHUD));

static FAutoConsoleCommandWithWorld GOBCaptureToggleAll(
	TEXT("ob.Capture.ToggleAll"),
	TEXT("캐릭터와 HUD를 함께 토글한다. 예: setbind H ob.Capture.ToggleAll"),
	FConsoleCommandWithWorldDelegate::CreateStatic(&OBCapture::ToggleAll));

#endif // !UE_BUILD_SHIPPING
