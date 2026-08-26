// Fill out your copyright notice in the Description page of Project Settings.

#include "Capture/OBCaptureFlightComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

namespace
{
	// 촬영 중 감각에 맞춰 조정한다. 에디터 기본 비행보다 조금 빠른 편.
	static TAutoConsoleVariable<float> CVarOBCaptureFlySpeed(
		TEXT("ob.Capture.FlySpeed"),
		1500.f,
		TEXT("촬영용 자유 비행 속도(cm/s)."),
		ECVF_Cheat);

	static TAutoConsoleVariable<float> CVarOBCaptureFlyBoost(
		TEXT("ob.Capture.FlyBoost"),
		4.f,
		TEXT("Shift를 누르고 있을 때의 속도 배율."),
		ECVF_Cheat);
}

UOBCaptureFlightComponent::UOBCaptureFlightComponent()
{
	// 키를 '누르고 있는 동안' 움직여야 해서 폴링이 필요하다. 헤더 주석 참조.
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(false);
}

UOBCaptureFlightComponent* UOBCaptureFlightComponent::Enable(ACharacter* Character)
{
	if (!IsValid(Character))
	{
		return nullptr;
	}

	if (UOBCaptureFlightComponent* Existing = Character->FindComponentByClass<UOBCaptureFlightComponent>())
	{
		return Existing;
	}

	UOBCaptureFlightComponent* Component = NewObject<UOBCaptureFlightComponent>(Character);
	Component->RegisterComponent();
	return Component;
}

void UOBCaptureFlightComponent::Disable(ACharacter* Character)
{
	if (!IsValid(Character))
	{
		return;
	}

	if (UOBCaptureFlightComponent* Existing = Character->FindComponentByClass<UOBCaptureFlightComponent>())
	{
		// 복원을 EndPlay에만 맡기지 않는다. DestroyComponent 경로에서 EndPlay가
		// 불리지 않는 경우가 있어 여기서 명시적으로 되돌린다(RestoreMovement는 멱등).
		Existing->RestoreMovement();
		Existing->DestroyComponent();
	}
}

void UOBCaptureFlightComponent::BeginPlay()
{
	Super::BeginPlay();

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Capture] CharacterMovement가 없어 자유 비행을 켤 수 없다."));
		return;
	}

	CachedMovementMode = Movement->MovementMode;

	// MOVE_None으로 재워 둔다. 중력·감속이 멈추므로 매 프레임 직접 옮긴 위치가 그대로 유지된다.
	Movement->DisableMovement();
	Movement->StopMovementImmediately();

	UE_LOG(LogTemp, Log, TEXT("[Capture] 자유 비행 켬 — WASD 이동 / E 상승 / Q 하강 / Shift 가속"));
}

void UOBCaptureFlightComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RestoreMovement();
	Super::EndPlay(EndPlayReason);
}

void UOBCaptureFlightComponent::RestoreMovement()
{
	if (bRestored)
	{
		return;
	}
	bRestored = true;

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr)
	{
		// MOVE_None을 그대로 복원하면 캐릭터가 영영 못 움직인다. 그 경우 걷기로 되돌린다.
		Movement->SetMovementMode(CachedMovementMode == MOVE_None ? MOVE_Walking : CachedMovementMode);
		UE_LOG(LogTemp, Log, TEXT("[Capture] 자유 비행 끔 — 이동 복원"));
	}
}

void UOBCaptureFlightComponent::TickComponent(
	float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	APlayerController* PC = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
	if (!PC)
	{
		return;
	}

	// 시야 방향 기준. Input_Move는 yaw만 쓰지만(수평 고정) 여기서는 피치를 포함해야
	// 에디터처럼 "올려다보고 W"로 상승한다.
	const FRotator ViewRotation = PC->GetControlRotation();
	const FVector Forward = FRotationMatrix(ViewRotation).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(ViewRotation).GetUnitAxis(EAxis::Y);

	FVector Direction = FVector::ZeroVector;
	if (PC->IsInputKeyDown(EKeys::W)) Direction += Forward;
	if (PC->IsInputKeyDown(EKeys::S)) Direction -= Forward;
	if (PC->IsInputKeyDown(EKeys::D)) Direction += Right;
	if (PC->IsInputKeyDown(EKeys::A)) Direction -= Right;

	// Q/E는 시야와 무관하게 월드 수직이다.
	if (PC->IsInputKeyDown(EKeys::E)) Direction += FVector::UpVector;
	if (PC->IsInputKeyDown(EKeys::Q)) Direction -= FVector::UpVector;

	if (Direction.IsNearlyZero())
	{
		return;
	}

	float Speed = FMath::Max(0.f, CVarOBCaptureFlySpeed.GetValueOnGameThread());
	if (PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift))
	{
		Speed *= FMath::Max(1.f, CVarOBCaptureFlyBoost.GetValueOnGameThread());
	}

	// bSweep=false — 콜리전 설정을 건드리지 않고 벽을 통과한다.
	Character->AddActorWorldOffset(
		Direction.GetSafeNormal() * Speed * DeltaTime,
		/*bSweep=*/false);
}
