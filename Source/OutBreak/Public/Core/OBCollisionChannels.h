#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

/*
왜 존재하는가?
 - 프로젝트 전역 커스텀 충돌 채널을 의미 있는 이름으로 별칭화한다.
 - 채널 인덱스(GameTraceChannel1 등)를 코드에 흩뿌리지 않기 위함.
주의:
 - 아래 값은 Project Settings에서 'CameraProbe'에 배정된 실제 채널과 반드시 일치해야 한다.
*/

// 카메라 스프링암 전용 프로브 채널: 벽=Block, 모든 Pawn=Ignore.
#define OB_TraceChannel_CameraProbe		ECC_GameTraceChannel1
#define OB_TraceChannel_Weapon		ECC_GameTraceChannel2