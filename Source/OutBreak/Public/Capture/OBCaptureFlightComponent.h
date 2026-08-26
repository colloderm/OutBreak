// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "OBCaptureFlightComponent.generated.h"

class ACharacter;

/**
 * 트레일러 촬영용 에디터식 자유 비행. 캐릭터를 숨길 때 붙었다가 되돌릴 때 제거된다.
 *
 * W/S : 시야 방향 전후 (에디터처럼 올려다보면 위로 난다)
 * A/D : 좌우
 * E/Q : 월드 기준 상승 / 하강
 * Shift : 가속
 *
 * 왜 MOVE_Flying이 아니라 액터를 직접 옮기는가:
 * - 이동 모드를 쓰면 중력·감속·콜리전과 계속 싸우고, 되돌릴 때 복원할 값이 4개로 늘어난다.
 * - AddActorWorldOffset(bSweep=false)은 콜리전 설정을 건드리지 않고도 벽을 통과한다.
 * - CMC는 MOVE_None으로 재워 두기만 하면 되므로 복원 대상이 이동 모드 하나뿐이다.
 *
 * 왜 Enhanced Input을 안 쓰는가:
 * - Q/E용 IA와 IMC를 새로 만들면 IMC 중복으로 키가 조용히 죽는 함정에 다시 걸린다.
 * - APlayerController::IsInputKeyDown()은 바인딩 없이 원시 키 상태를 읽는다.
 *   에셋 0개, BP 수정 0개로 끝난다.
 *
 * 왜 틱을 켜는가:
 * - 프로젝트 관례는 bCanEverTick=false지만, 키를 '누르고 있는 동안' 움직여야 하므로
 *   매 프레임 폴링이 불가피하다. 촬영 중에만 존재하는 컴포넌트라 상시 비용이 아니다.
 */
UCLASS()
class OUTBREAK_API UOBCaptureFlightComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOBCaptureFlightComponent();

	/** 폰에 붙이고 비행을 켠다. 이미 붙어 있으면 기존 것을 그대로 둔다. */
	static UOBCaptureFlightComponent* Enable(ACharacter* Character);

	/** 원래 이동 상태로 되돌리고 컴포넌트를 제거한다. */
	static void Disable(ACharacter* Character);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	void RestoreMovement();

	// 복원 대상은 이동 모드 하나뿐이다.
	EMovementMode CachedMovementMode = MOVE_Walking;
	bool bRestored = false;
};
